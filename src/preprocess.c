#include "preprocess.h"
#include "entry.h"
#include "parser.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

extern AssemblerFlags asm_flags;

static void preprocessor_ctx_stack_push(Preprocessor* pre, int starti, int endi){
    PreprocessorCtx* new_ctx = malloc(sizeof(PreprocessorCtx));
    //TODO SWITCH BACK TO PROGRAM_FATAL_ERROR
    if(new_ctx == NULL) fatal_error("Out of memory\n");
    memset(new_ctx, 0, sizeof(PreprocessorCtx));

    new_ctx->starti = starti;
    new_ctx->endi = endi;
    new_ctx->macro_index = NO_MACRO_PARAMS; //only care about this value if the macro has params
    new_ctx->index = starti;
    new_ctx->next = (struct PreprocessorCtx*)pre->stack;
    pre->stack = new_ctx;

}


static void preprocessor_ctx_stack_pop(Preprocessor* pre){
    PreprocessorCtx* tmp = (PreprocessorCtx*)pre->stack->next;

    if(pre->stack->macro_index != NO_MACRO_PARAMS){
        array_list_delete(pre->stack->arg_values);
    }
    free(pre->stack);
    pre->stack = tmp;
}

static Token preprocessor_next_token(Preprocessor* pre){
    while(pre->stack != NULL){
        if(pre->stack->endi == pre->stack->index){
            preprocessor_ctx_stack_pop(pre);
        } else{
            Token res = array_list_get((*pre->p->tokens), Token, pre->stack->index); 
            //if we have a function like macro and identifier 
            // check if the the current token is a Parameter name and return its value 
            if(res.type == TOK_IDENTIFIER && pre->stack->macro_index != NO_MACRO_PARAMS){
                PreprocessorMacro macro = array_list_get((*pre->macros), PreprocessorMacro, pre->stack->macro_index);
                for(int i = 0; i < macro.args.size; i++){
                    char* arg = array_list_get(macro.args, char*, i);
                    if(strcmp(arg, res.literal) == 0){
                        res = array_list_get(pre->stack->arg_values, Token, i);
                        break;
                    }
                }
            }
            pre->stack->index++;
            pre->index = pre->stack->index;
            pre->currentToken = res;
            return res; 
        }
    }  
    Token res = parser_next_token(pre->p); 
    pre->currentToken = res;
    pre->index = pre->p->tokenIndex;
    return res;
}




static void preprocessor_expect_token(Preprocessor* pre, TokenType expected){
    if(pre->currentToken.type != expected){
        parser_error(pre->p, "Expected %s found %s in macro\n", token_to_string(expected), token_to_string(pre->currentToken.type));
    }
}


static void preprocessor_expect_consume_token(Preprocessor* pre, TokenType expected){
   preprocessor_expect_token(pre, expected); 
   preprocessor_next_token(pre); 
}


static Token preprocessor_peek_token(Preprocessor* pre){
    return array_list_get((*pre->p->tokens),Token, pre->index);
}


static void preprocessor_add_symbol(Preprocessor* pre){
    Token name = preprocessor_next_token(pre); 
    preprocessor_expect_consume_token(pre, TOK_IDENTIFIER);

    int starti = pre->index - 1;
    while(preprocessor_next_token(pre).type != TOK_NEW_LINE);
    int endi = pre->index - 1;

    for(int i = 0; i < pre->symbols->size; i++){
        PreprocessorSymbol* s = &array_list_get((*pre->symbols), PreprocessorSymbol, i);
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
    array_list_append((*pre->symbols), PreprocessorSymbol, s);
}


static void evaluate_preprocessor_statement(Preprocessor* pre, ArrayList* new_tokens){
    Token t = preprocessor_next_token(pre);
    switch (t.type) {
        case TOK_DEFINE:{
            preprocessor_add_symbol(pre);
            break;
        } 
        case TOK_MACRO: {
            Token id = preprocessor_next_token(pre);
            preprocessor_expect_consume_token(pre, TOK_IDENTIFIER);
            preprocessor_expect_token(pre, TOK_OPENING_PAREN);
            ArrayList params = {0};
            if(preprocessor_peek_token(pre).type != TOK_CLOSING_PAREN){
                array_list_create_cap(params, char*, 5);
                do {
                    if(params.size >= 16){
                        parser_error(pre->p, "Invalid Parameter count: %s. MACROS only support 16 parameters\n");
                    } 

                    Token arg = preprocessor_next_token(pre);
                    preprocessor_expect_token(pre, TOK_IDENTIFIER);
                    array_list_append(params, char*, arg.literal);
                    if(preprocessor_next_token(pre).type != TOK_COMMA) break;
                }while (true);
            } else{
                preprocessor_next_token(pre);
            }

            preprocessor_expect_consume_token(pre, TOK_CLOSING_PAREN);
            int starti = pre->index;
            while(preprocessor_next_token(pre).type != TOK_ENDMACRO){
               if(parser_is_last_token(pre->p)){
                    parser_error_loc(pre->p, id.line_number, id.col, "Missing #endmacro\n");
               }
            }
            int endi = pre->index - 1;
            preprocessor_expect_consume_token(pre, TOK_ENDMACRO);
            preprocessor_expect_token(pre, TOK_NEW_LINE);

            PreprocessorMacro m = {0};
            m.name = id.literal;
            m.args = params;
            m.starti = starti;
            m.endi = endi;
            array_list_append((*pre->macros), PreprocessorMacro, m);
            break;
        }
        case TOK_IDENTIFIER: {
            int l = pre->currentToken.line_number;
            int c = pre->currentToken.col;
            bool found_symbol = false;
            if(preprocessor_peek_token(pre).type == TOK_OPENING_PAREN){
                preprocessor_next_token(pre);
                for(int i = 0; i < pre->macros->size; i++){
                    PreprocessorMacro mac = array_list_get((*pre->macros), PreprocessorMacro, i);
                    if(strcmp(t.literal,mac.name) == 0){ 
                        ArrayList params = {0};
                        if(preprocessor_peek_token(pre).type != TOK_CLOSING_PAREN){
                            array_list_create_cap(params, Token, 5);
                            do {
                                if(params.size >= 16){
                                    parser_error(pre->p, "Invalid Parameter count: %s. MACROS only support 16 parameters\n");
                                } 

                                Token arg = preprocessor_next_token(pre);
                                array_list_append(params, Token, arg);
                                if(preprocessor_next_token(pre).type != TOK_COMMA) break;
                            }while (true);
                            preprocessor_expect_consume_token(pre, TOK_CLOSING_PAREN);
                        } else{
                            preprocessor_next_token(pre);
                            preprocessor_expect_consume_token(pre, TOK_CLOSING_PAREN);
                        }
                        if(params.size != mac.args.size){
                            parser_error_loc(pre->p,l,c, "Expected %d got %d args\n",
                                    mac.args.size, params.size, mac.name);
                        }
                        preprocessor_ctx_stack_push(pre, mac.starti, mac.endi);
                        pre->stack->macro_index = (params.size == 0) ? NO_MACRO_PARAMS : i;
                        pre->stack->arg_values = params;
                        found_symbol = true;
                        break;
                    }
                }
            } else{
                for(int i = 0; i < pre->symbols->size; i++){
                    PreprocessorSymbol s = array_list_get((*pre->symbols), PreprocessorSymbol, i);
                    if(strcmp(t.literal,s.name) == 0){
                        preprocessor_ctx_stack_push(pre, s.starti, s.endi);
                        found_symbol = true;
                        break;
                    }
                }
            }

            
            if(!found_symbol){
                array_list_append((*new_tokens), Token, t); 
            } 
            break;
        }
        case TOK_IFDEF: 
        case TOK_IFNDEF: {
            bool ifndef = pre->currentToken.type == TOK_IFNDEF;
            int l = pre->currentToken.line_number;
            int c = pre->currentToken.col;
            preprocessor_next_token(pre);
            preprocessor_expect_token(pre, TOK_IDENTIFIER);
            char* m_name = pre->currentToken.literal;
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
            for(int i = 0; i < pre->symbols->size; i++){
                PreprocessorSymbol s = array_list_get((*pre->symbols), PreprocessorSymbol, i);
                if(strcmp(m_name,s.name) == 0){ 
                    is_defined = true;
                    break;
                }
            }
        add_body:
            preprocessor_next_token(pre);
            preprocessor_expect_token(pre, TOK_NEW_LINE);
            if((is_defined && !ifndef) || (!is_defined && ifndef)){
                while(preprocessor_peek_token(pre).type != TOK_ENDIF){
                    if(parser_is_last_token(pre->p)){
                        parser_error_loc(pre->p, l, c, "if statement missing closing #endif\n");
                    }
                    evaluate_preprocessor_statement(pre, new_tokens);
                }
            } else{
                //if macro is not defined skip over all these tokens
                int if_count = 0;
                int endif_count = 0;
                while(true){
                    if(preprocessor_peek_token(pre).type == TOK_ENDIF && if_count == endif_count) break;
                    if(parser_is_last_token(pre->p)){
                        parser_error_loc(pre->p, l, c, "if statement missing closing #endif\n");
                    }
                    Token tmp = preprocessor_next_token(pre);
                    if(tmp.type == TOK_IFDEF) if_count++; 
                    else if (tmp.type == TOK_ENDIF) endif_count++;
                }
            }
            
            preprocessor_next_token(pre);
            preprocessor_expect_consume_token(pre, TOK_ENDIF);
            preprocessor_expect_token(pre, TOK_NEW_LINE);
            break;
        }
        case TOK_ENDIF: {
            parser_error(pre->p, "Missing if statement\n");
            return;
        }
        case TOK_ENDMACRO: {
            parser_error(pre->p, "Missing macro statement\n");
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
    ArrayList preprocess_symbols;
    array_list_create_cap(preprocess_symbols,PreprocessorSymbol, 16);

    ArrayList macros = {0};
    array_list_create_cap(macros,PreprocessorMacro, 8);

    ArrayList new_tokens = {0};
    array_list_create_cap(new_tokens, Token, tokens->size);

    Parser p = {0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;

    Preprocessor pre = {0};
    pre.symbols = &preprocess_symbols;
    pre.macros = &macros;
    pre.p = &p;
    pre.stack = NULL;
    pre.currentToken.type = TOK_MAX;



    while(1){
        if(setjmp(p.jmp) == 1) break;
        evaluate_preprocessor_statement(&pre, &new_tokens); 
    }
   
    array_list_delete((*tokens));

    for(int i = 0; i < preprocess_symbols.size; i++){
        PreprocessorSymbol s = array_list_get(preprocess_symbols, PreprocessorSymbol, i);
        free(s.name); 
    }

    for(int i = 0; i < macros.size; i++){
        PreprocessorMacro s = array_list_get(macros,PreprocessorMacro, i);
        for(int j = 0; j < s.args.size; j++){
            char* arg_name = array_list_get(s.args, char*, j);
            free(arg_name);
        }
        array_list_delete(s.args);
        free(s.name); 
    }

    array_list_delete(macros);
    array_list_delete(preprocess_symbols);
 
    return new_tokens; 
}


