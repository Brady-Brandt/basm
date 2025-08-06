#include "preprocess.h"
#include "entry.h"
#include "parser.h"
#include "eval.h"
#include "objectgen.h"
#include "util.h"
#include "x86/types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


//forward declare
static void evaluate_preprocessor_statement(ArrayList* new_tokens);


typedef struct {
    const char* name;
    Token (*symbol_function)();
} BuiltinSymbol;



static Token define_elf(){
    Token res = {0};
    res.type = TOK_INT;
    res.literal = (program.flags.ftype == BASM_FILE_ELF) ? "1" : "0";
    return res;
}


static Token define_win(){
    Token res = {0};
    res.type = TOK_INT;
    res.literal = (program.flags.ftype == BASM_FILE_PE) ? "1" : "0";
    return res;
}


static Token define_macho(){
    Token res = {0};
    res.type = TOK_INT;
    res.literal = (program.flags.ftype == BASM_FILE_MACHO) ? "1" : "0";
    return res;
}


static BuiltinSymbol BUILTIN_SYMBOLS[] = {
    {"__ELF__",define_elf},
    {"__WIN__", define_win},
    {"__MACHO__", define_macho},
};


#define BUILTIN_SYMBOLS_SIZE (sizeof(BUILTIN_SYMBOLS) / sizeof(BUILTIN_SYMBOLS[0]))

typedef struct {
    Parser* p;
    ArrayList macros;
    ArrayList symbols;
    PreprocessorCtx* stack;
    Token currentToken;
    int index;
} Preprocessor;



static Preprocessor preprocessor = {0};




static void preprocessor_ctx_stack_push(int starti, int endi){
    PreprocessorCtx* new_ctx = malloc(sizeof(PreprocessorCtx));
    if(new_ctx == NULL) fatal_error("Out of memory\n");
    memset(new_ctx, 0, sizeof(PreprocessorCtx));

    new_ctx->starti = starti;
    new_ctx->endi = endi;
    new_ctx->macro_index = NO_MACRO_PARAMS; //only care about this value if the macro has params
    new_ctx->index = starti;
    new_ctx->next = (struct PreprocessorCtx*)preprocessor.stack;
    preprocessor.stack = new_ctx;

}


static void preprocessor_ctx_stack_pop(){
    PreprocessorCtx* tmp = (PreprocessorCtx*)preprocessor.stack->next;

    if(preprocessor.stack->macro_index != NO_MACRO_PARAMS){
        array_list_delete(preprocessor.stack->arg_values);
    }
    free(preprocessor.stack);
    preprocessor.stack = tmp;
}

static Token preprocessor_next_token(){
    while(preprocessor.stack != NULL){
        if(preprocessor.stack->endi == preprocessor.stack->index){
            preprocessor_ctx_stack_pop();
        } else{
            Token res = array_list_get((*preprocessor.p->tokens), Token, preprocessor.stack->index); 
            //if we have a function like macro and identifier 
            // check if the the current token is a Parameter name and return its value 
            if(res.type == TOK_IDENTIFIER && preprocessor.stack->macro_index != NO_MACRO_PARAMS){
                PreprocessorMacro macro = array_list_get(preprocessor.macros, PreprocessorMacro, preprocessor.stack->macro_index);
                for(int i = 0; i < macro.args.size; i++){
                    char* arg = array_list_get(macro.args, char*, i);
                    if(strcmp(arg, res.literal) == 0){
                        res = array_list_get(preprocessor.stack->arg_values, Token, i);
                        break;
                    }
                }
            }
            preprocessor.stack->index++;
            preprocessor.index = preprocessor.stack->index;
            preprocessor.currentToken = res;
            return res; 
        }
    }  
    Token res = parser_next_token(preprocessor.p); 
    preprocessor.currentToken = res;
    preprocessor.index = preprocessor.p->tokenIndex;
    return res;
}




static bool preprocessor_expect_token(TokenType expected){
    if(preprocessor.currentToken.type != expected){
        parser_error(preprocessor.p, "Expected %s found %s in macro\n", token_to_string(expected), 
                token_to_string(preprocessor.currentToken.type));
        return false;
    }
    return true;
}


static bool preprocessor_expect_consume_token( TokenType expected){
   bool res = preprocessor_expect_token(expected);
   preprocessor_next_token(); 
   return res;
}


static Token preprocessor_peek_token(){
    return array_list_get((*preprocessor.p->tokens),Token, preprocessor.index);
}


static void preprocessor_add_symbol(){
    Token name = preprocessor_next_token(); 
    preprocessor_expect_consume_token(TOK_IDENTIFIER);

    int starti = preprocessor.index - 1;
    while(preprocessor_next_token().type != TOK_NEW_LINE);
    int endi = preprocessor.index - 1;

    for(int i = 0; i < preprocessor.symbols.size; i++){
        PreprocessorSymbol* s = &array_list_get((preprocessor.symbols), PreprocessorSymbol, i);
        if(strcmp(name.literal,s->name) == 0){
            s->starti = starti;
            s->endi = endi;
            return;
        }
    }
    PreprocessorSymbol s;
    s.name = name.literal;
    s.starti = starti;
    s.endi = endi;
    array_list_append((preprocessor.symbols), PreprocessorSymbol, s);
}


static BuiltinSymbol* get_builtin_symbol(char* name){
    for(size_t i = 0; i < BUILTIN_SYMBOLS_SIZE; i++){
        if(strcmp(name, BUILTIN_SYMBOLS[i].name) == 0){
            return &BUILTIN_SYMBOLS[i];
        }
    }
    return NULL;
}

static int64_t get_macro(char* name){
    for(int i = 0; i < preprocessor.macros.size; i++){
        PreprocessorMacro* mac = &array_list_get((preprocessor.macros), PreprocessorMacro, i);
        if(strcmp(name,mac->name) == 0){ 
            return i;
        }
    }
    return -1;
}


static bool macro_call(Token name_tok){
    preprocessor_next_token();
    
    int64_t macro_index = get_macro(name_tok.literal);

    if(macro_index == -1){
        parser_error_loc(preprocessor.p, name_tok.line_number, name_tok.col, "Macro not defined\n");
        return false;
    }

    PreprocessorMacro macro = array_list_get((preprocessor.macros), PreprocessorMacro, macro_index);

    ArrayList params = {0};
    if(preprocessor_peek_token().type != TOK_CLOSING_PAREN){
        array_list_create_cap(params, Token, 5);
        do {
            if(params.size >= 16){
                parser_error(preprocessor.p, "Invalid Parameter count: %s. MACROS only support 16 parameters\n");
                return false;
            } 

            Token arg = preprocessor_next_token();
            array_list_append(params, Token, arg);
            if(preprocessor_next_token().type != TOK_COMMA) break;
        }while (true);
        preprocessor_expect_consume_token(TOK_CLOSING_PAREN);
    } else{
        preprocessor_next_token();
        preprocessor_expect_consume_token(TOK_CLOSING_PAREN);
    }

    if(params.size != macro.args.size){
        parser_error_loc(preprocessor.p,name_tok.line_number,name_tok.col, "Expected %d got %d args\n",
                macro.args.size, params.size);
        return false;
    }

    preprocessor_ctx_stack_push(macro.starti, macro.endi);
    preprocessor.stack->macro_index = (params.size == 0) ? NO_MACRO_PARAMS : macro_index;
    preprocessor.stack->arg_values = params;
    return true;
}



static PreprocessorSymbol* get_symbol(char* name){
    for(int i = 0; i < preprocessor.symbols.size; i++){
        PreprocessorSymbol* s = &array_list_get((preprocessor.symbols), PreprocessorSymbol, i);
        if(strcmp(name,s->name) == 0){
            return s;
        }
    }
    return NULL;
}


static bool is_symbol_defined(char* name){
    if(strncmp(name, "__", 2) == 0){
        if(get_builtin_symbol(name) != NULL) return true;
    }
    return get_symbol(name) != NULL || get_macro(name) != -1;
}


static bool eval_if_expr(){
    //right now only support int true/false

    Token expr = preprocessor_next_token();

    if(expr.type == TOK_IDENTIFIER){
        BuiltinSymbol* symbol = get_builtin_symbol(expr.literal);
        if(symbol != NULL){
            uint64_t integer = 0;
            bool result = string_to_int(symbol->symbol_function().literal, &integer);

            if(!result){
                parser_error(preprocessor.p, "Invalid if condition: Conditions can only be integers right now\n");
                return false;
            }
            return integer;
        }

        parser_error(preprocessor.p, "Invalid if condition: Conditions can only be integers right now\n");
        return false;
    } else{
        if(expr.type != TOK_INT){
            parser_error(preprocessor.p, "Invalid if condition: Conditions can only be integers right now\n");
            return false;
        }

        uint64_t integer = 0;
        bool result = string_to_int(expr.literal, &integer);
        if(!result){
            parser_error(preprocessor.p, "Invalid if condition: Conditions can only be integers right now\n");
            return false;
        }
        return integer;
    }
}


static void consume_block(Token block_start){
    int if_count = 0;
    int endif_count = 0;
    while(true){
        if(preprocessor_peek_token().type == TOK_ELIF) break;
        if(preprocessor_peek_token().type == TOK_ENDIF && if_count == endif_count) break;
        if(parser_is_last_token(preprocessor.p)){
            parser_error_loc(preprocessor.p, block_start.line_number, block_start.col,
                    "if statement missing closing #endif\n");
            return;
        }
        Token tmp = preprocessor_next_token();
        if(tmp.type == TOK_IFDEF || tmp.type == TOK_IFNDEF || tmp.type == TOK_IF) if_count++;
        else if (tmp.type == TOK_ENDIF) endif_count++;
    }
}

static void eval_if_statement(Token if_token, bool condition, ArrayList* new_tokens){
    int l = if_token.line_number;
    int c = if_token.col;

    if(condition){
        preprocessor_next_token();
        preprocessor_expect_token(TOK_NEW_LINE);

        while(preprocessor_peek_token().type != TOK_ENDIF){
            if(parser_is_last_token(preprocessor.p)){
                parser_error_loc(preprocessor.p, l, c, "if statement missing closing #endif\n");
            }

            if(preprocessor_peek_token().type == TOK_ELIF) {
                //skip over all elif blocks that have the same depth as the if statement
                int if_count = 0;
                int endif_count = 0;
                while(true){ 
                    if(preprocessor_peek_token().type == TOK_ENDIF && if_count == endif_count) break;
                    if(parser_is_last_token(preprocessor.p)){
                        parser_error_loc(preprocessor.p, l, c,
                                "if statement missing closing #endif\n");
                        return;
                    }
                    Token tmp = preprocessor_next_token();
                    if(tmp.type == TOK_IFDEF || tmp.type == TOK_IFNDEF || tmp.type == TOK_IF) if_count++;
                    else if (tmp.type == TOK_ENDIF) endif_count++;
                }
                break;
            }

            evaluate_preprocessor_statement(new_tokens);
        }
    } else{
        int if_count = 0;
        int endif_count = 0;
        while(true){
            TokenType next_type = preprocessor_peek_token().type;
            if(next_type == TOK_ELIF && if_count == endif_count){
                Token elif = preprocessor_next_token();

                bool expr = eval_if_expr();
                if(expr){
                    eval_if_statement(elif, expr, new_tokens);
                    return;
                }
            } else if(next_type == TOK_ELSE && if_count == endif_count){
                    Token els = preprocessor_next_token();
                    eval_if_statement(els, true, new_tokens);
                    return;
            }

            if(next_type == TOK_ENDIF && if_count == endif_count) break;
            if(parser_is_last_token(preprocessor.p)){
                parser_error_loc(preprocessor.p, l, c,
                        "if statement missing closing #endif\n");
                return;
            }
            Token tmp = preprocessor_next_token();
            if(tmp.type == TOK_IFDEF || tmp.type == TOK_IFNDEF || tmp.type == TOK_IF) if_count++;
            else if (tmp.type == TOK_ENDIF) endif_count++;
        } 
    }
    preprocessor_next_token();
    preprocessor_expect_consume_token(TOK_ENDIF);
    preprocessor_expect_token(TOK_NEW_LINE);
}



static void evaluate_preprocessor_statement(ArrayList* new_tokens){
    Token t = preprocessor_next_token();
    switch (t.type) {
        case TOK_DEFINE:{
            preprocessor_add_symbol();
            break;
        } 
        case TOK_MACRO: {
            Token id = preprocessor_next_token();
            preprocessor_expect_consume_token( TOK_IDENTIFIER);
            preprocessor_expect_token(TOK_OPENING_PAREN);
            ArrayList params = {0};
            if(preprocessor_peek_token().type != TOK_CLOSING_PAREN){
                array_list_create_cap(params, char*, 5);
                do {
                    if(params.size >= 16){
                        parser_error(preprocessor.p, "Invalid Parameter count: %s. MACROS only support 16 parameters\n");
                    } 

                    Token arg = preprocessor_next_token();
                    preprocessor_expect_token(TOK_IDENTIFIER);
                    array_list_append(params, char*, arg.literal);
                    if(preprocessor_next_token().type != TOK_COMMA) break;
                }while (true);
            } else{
                preprocessor_next_token();
            }

            preprocessor_expect_consume_token(TOK_CLOSING_PAREN);
            int starti = preprocessor.index;
            while(preprocessor_next_token().type != TOK_ENDMACRO){
               if(parser_is_last_token(preprocessor.p)){
                    parser_error_loc(preprocessor.p, id.line_number, id.col, "Missing #endmacro\n");
               }
            }
            int endi = preprocessor.index - 1;
            preprocessor_expect_consume_token(TOK_ENDMACRO);
            preprocessor_expect_token(TOK_NEW_LINE);

            PreprocessorMacro m = {0};
            m.name = id.literal;
            m.args = params;
            m.starti = starti;
            m.endi = endi;
            array_list_append((preprocessor.macros), PreprocessorMacro, m);
            break;
        }
        case TOK_IDENTIFIER: {
            if(preprocessor_peek_token().type == TOK_OPENING_PAREN){
                macro_call(preprocessor.currentToken);
            } else{
                //check if it is #define macro
                PreprocessorSymbol* symbol = get_symbol(preprocessor.currentToken.literal);
                if(symbol != NULL){
                    preprocessor_ctx_stack_push(symbol->starti, symbol->endi);
                } else{
                    array_list_append((*new_tokens), Token, t); 
                }     
            } 
            break;
        }
        case TOK_IFDEF: {
            Token if_token = t;
            preprocessor_next_token();
            if(preprocessor_expect_token(TOK_IDENTIFIER)){
                char* m_name = preprocessor.currentToken.literal;
                eval_if_statement(if_token, is_symbol_defined(m_name), new_tokens);
            } else{
                eval_if_statement(if_token, false, new_tokens);
            }
           break;
        }
        case TOK_IFNDEF: {
            Token if_token = t;
            preprocessor_next_token();
            if(preprocessor_expect_token(TOK_IDENTIFIER)){
                char* m_name = preprocessor.currentToken.literal;
                eval_if_statement(if_token, !is_symbol_defined(m_name), new_tokens);
            } else{
                eval_if_statement(if_token, false, new_tokens);
            }
           break;
        }
        case TOK_IF: {
            Token if_token = t;
            eval_if_statement(if_token, eval_if_expr(), new_tokens);
           break;
        }
        case TOK_ELIF:
        case TOK_ELSE:
        case TOK_ENDIF: 
            parser_error(preprocessor.p, "Missing if statement\n");
            return;
        case TOK_ENDMACRO: 
            parser_error(preprocessor.p, "Missing macro statement\n");
            return;
        default:
            array_list_append((*new_tokens), Token, t);
    } 
}

/*
 * TODO: THIS LEAKS A LITTLE BIT OF MEMORY
 * When replacing a macro with its definition
 * the instance of the macro name will get leaked
 * #define x 5 
 *  x -> when x gets replaced with 5 here the pointer to x will get leaked
 */

ArrayList preprocess_tokens(ArrayList* tokens){  
    ArrayList new_tokens = {0};
    array_list_create_cap(new_tokens, Token, tokens->size);

    Parser p = {0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;

    preprocessor.p = &p;
    array_list_create_cap(preprocessor.symbols,PreprocessorSymbol, 16);
    array_list_create_cap(preprocessor.macros,PreprocessorMacro, 8);
    preprocessor.stack = NULL;
    preprocessor.currentToken.type = TOK_MAX;
    

    while(1){
        if(setjmp(p.jmp) == 1) break;
        evaluate_preprocessor_statement(&new_tokens); 
    }
 
    array_list_delete((*tokens));

    for(int i = 0; i < preprocessor.symbols.size; i++){
        PreprocessorSymbol s = array_list_get(preprocessor.symbols, PreprocessorSymbol, i);
        free(s.name); 
    }

    for(int i = 0; i < preprocessor.macros.size; i++){
        PreprocessorMacro s = array_list_get(preprocessor.macros,PreprocessorMacro, i);
        for(int j = 0; j < s.args.size; j++){
            char* arg_name = array_list_get(s.args, char*, j);
            free(arg_name);
        }
        array_list_delete(s.args);
        free(s.name); 
    }

    array_list_delete(preprocessor.macros);
    array_list_delete(preprocessor.symbols);
 
    return new_tokens; 
}


