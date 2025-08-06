#include "preprocess.h"
#include "entry.h"
#include "parser.h"
#include "util.h"
#include "x86/types.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern AssemblerFlags asm_flags;

typedef struct {
    Parser* p;
    ArrayList macros;
    ArrayList symbols;
    ArrayList builtin_symbols;
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




static void preprocessor_expect_token(TokenType expected){
    if(preprocessor.currentToken.type != expected){
        parser_error(preprocessor.p, "Expected %s found %s in macro\n", token_to_string(expected), 
                token_to_string(preprocessor.currentToken.type));
    }
}


static void preprocessor_expect_consume_token( TokenType expected){
   preprocessor_expect_token(expected); 
   preprocessor_next_token(); 
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
        case TOK_IFDEF: 
        case TOK_IFNDEF: {
            bool ifndef = preprocessor.currentToken.type == TOK_IFNDEF;
            int l = preprocessor.currentToken.line_number;
            int c = preprocessor.currentToken.col;
            preprocessor_next_token();
            preprocessor_expect_token(TOK_IDENTIFIER);
            char* m_name = preprocessor.currentToken.literal;
            bool is_defined = false;
            if(strncmp("__", m_name, 2) == 0){
                    if(asm_flags.ftype == BASM_FILE_ELF && strcmp(m_name, "__LINUX__") == 0){
                        is_defined = true;
                        goto add_body;
                    } else if(asm_flags.ftype == BASM_FILE_PE && strcmp(m_name, "__WINDOWS__") == 0){ 
                        is_defined = true;
                        goto add_body;
                    }
            } 
            for(int i = 0; i < preprocessor.symbols.size; i++){
                PreprocessorSymbol s = array_list_get((preprocessor.symbols), PreprocessorSymbol, i);
                if(strcmp(m_name,s.name) == 0){ 
                    is_defined = true;
                    break;
                }
            }
        add_body:
            preprocessor_next_token();
            preprocessor_expect_token(TOK_NEW_LINE);
            if((is_defined && !ifndef) || (!is_defined && ifndef)){
                while(preprocessor_peek_token().type != TOK_ENDIF){
                    if(parser_is_last_token(preprocessor.p)){
                        parser_error_loc(preprocessor.p, l, c, "if statement missing closing #endif\n");
                    }
                    evaluate_preprocessor_statement(new_tokens);
                }
            } else{
                //if macro is not defined skip over all these tokens
                int if_count = 0;
                int endif_count = 0;
                while(true){
                    if(preprocessor_peek_token().type == TOK_ENDIF && if_count == endif_count) break;
                    if(parser_is_last_token(preprocessor.p)){
                        parser_error_loc(preprocessor.p, l, c, "if statement missing closing #endif\n");
                    }
                    Token tmp = preprocessor_next_token();
                    if(tmp.type == TOK_IFDEF) if_count++; 
                    else if (tmp.type == TOK_ENDIF) endif_count++;
                }
            }
            
            preprocessor_next_token();
            preprocessor_expect_consume_token(TOK_ENDIF);
            preprocessor_expect_token(TOK_NEW_LINE);
            break;
        }
        case TOK_ENDIF: {
            parser_error(preprocessor.p, "Missing if statement\n");
            return;
        }
        case TOK_ENDMACRO: {
            parser_error(preprocessor.p, "Missing macro statement\n");
            return;
        } 
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


