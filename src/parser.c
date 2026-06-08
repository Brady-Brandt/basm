#include "parser.h"
#include "objectgen.h"
#include "util.h"
#include "x86/types.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


extern FileBuffer* current_fb;

extern struct Keyword* find_keyword(char* str, size_t len);

static inline void get_literal(int* col){
    while(true){
        char next = file_buffer_peek_char(current_fb);
        //TODO: add more valid labels 
        if(!isalnum(next) && next != '_' && next != '.'){
            break;
        }
        char c = file_buffer_get_char(current_fb);
        scratch_buffer_append_char(c); 
        (*col)++;
    }
}



/** 
 * Determine if the String is a keyword or identifier 
 */
static Token id_or_kw(int* col){
    char* temp = scratch_buffer_get_data(0);
    bool preprocess = false;
    if(*temp == '#'){
        scratch_buffer_clear();
        preprocess = true;
    } 
    get_literal(col);
    char* str = scratch_buffer_as_str();
    uint32_t str_size = scratch_buffer_offset();


    const struct Keyword* kw = find_keyword(str, str_size); 
    if(kw != NULL){
        if(kw->type >= TOK_DREG && kw->type <= TOK_REG){
            return (Token){kw->type, .reg = kw->value, 0, 0};
        } else if (kw->type == TOK_INSTRUCTION) {
            // in order to not lose information about the 
            // instruction we are going to store the index 
            // into the KEYWORD table of the instruction in the token 
            uint64_t index = keyword_get_index(kw); 
            return (Token){TOK_INSTRUCTION, .instruction = index, 0, 0};    
        } else{
            if(kw->type > TOK_END_KEYWORDS && kw->type < TOK_END_PREPROCESSOR){
                if(preprocess){
                    return (Token){kw->type, 0, 0, 0};
                } else{
                    return (Token){TOK_IDENTIFIER, 0,0, 0};
                }
            }
            // plain keyword
            return (Token){kw->type, 0, 0, 0};
        }
    } else{
        return (Token){TOK_IDENTIFIER, 0,0, 0};
    } 
}



static void get_string(uint32_t line, int col, char close){
    while(true){
        char next = file_buffer_peek_char(current_fb);
        if(next == close){
            file_buffer_get_char(current_fb);
            scratch_buffer_append_char(0);
            return;
        }

        if(next == '\n'){
            fprintf(stderr, "Error: String doesn't close\nLine %d, Col %d\n", line, col);
            goto error;
        }
        char c = file_buffer_get_char(current_fb);
        if(c == '\\'){
            c = file_buffer_get_char(current_fb);
            switch (c) {
                case 'b':
                    scratch_buffer_append_char(8); 
                    break;
                case 't':
                    scratch_buffer_append_char(9); 
                    break;
                case 'n':
                    scratch_buffer_append_char(10); 
                    break;
                case 'f':
                    scratch_buffer_append_char(12); 
                    break;
                case 'r':
                    scratch_buffer_append_char(13); 
                    break;
                case '\"':
                    scratch_buffer_append_char(34); 
                    break;
                case '\'':
                    scratch_buffer_append_char(39); 
                    break;
                case '\\':
                    scratch_buffer_append_char(92); 
                    break;
                default:
                    fprintf(stderr, "Error: Invalid Escape Sequence\nLine %d, Col %d\n", line, col); 
                    goto error;
            }  
        } else{
            scratch_buffer_append_char(c); 
        }
    }

error:
    fprintf(stderr, "%s\n", file_get_line(current_fb, line));
    fprintf(stderr,"%*s\n", col, "^");
    exit(EXIT_FAILURE);

}








ArrayList tokenize_file(){
    init_scratch_buffer(); 

    ArrayList tokens;
    array_list_create_cap(tokens, Token, 256);

    int line_number = 1;
    int col = 1;

    char prev_newline = '\n';

    do{
        char c = file_buffer_get_char(current_fb);
   
        while(isspace(c) && c != '\n'){
            col++;
            c = file_buffer_get_char(current_fb);
        }

        if(file_buffer_eof(current_fb)) break;

        Token token;
        token.literal = 0;
        token.line_number = line_number;
        token.col = col;

        switch (c) {
            case ':':
                token.type = TOK_COLON;
                col++;
                break;
            case '\n':
                line_number++;
                col = 1;
                if(prev_newline == '\n') continue;
                token.type = TOK_NEW_LINE;
                break;
            case ',':
                col++;
                token.type = TOK_COMMA;
                break;
            case '\"':
                get_string(line_number, col, '\"');
                token.type = TOK_NSTRING;
                col++;
                break; 
            case '\'':
                get_string(line_number, col, '\'');
                token.type = TOK_STRING;
                col++;
                break; 
            case '[':
                col++;
                token.type = TOK_OPENING_BRACKET;
                break;
            case ']':
                col++;
                token.type = TOK_CLOSING_BRACKET;
                break;
            case '(':
                token.type = TOK_OPENING_PAREN;
                col++;
                break;
            case ')':
                token.type = TOK_CLOSING_PAREN;
                col++;
                break;
            case '+':
                col++;
                token.type = TOK_ADD;
                break;
            case '-':
                col++;
                token.type = TOK_SUB;
                break;
            case '~':
                col++;
                token.type = TOK_NEG;
                break;
            case '/':
                col++;
                token.type = TOK_DIVIDE;
                break;
            case '^':
                col++;
                token.type = TOK_XOR;
                break;
            case '%':
                col++;
                token.type = TOK_MOD;
                break;
            case '|':
                col++;
                token.type = TOK_OR;
                break;
            case '&':
                col++;
                token.type = TOK_AND;
                break;
            case '*':
                col++;
                token.type = TOK_MULTIPLY;
                break;
            case '<':
                if(file_buffer_peek_char(current_fb) != '<'){
                    //unkown token
                    col++;
                    token.type = c;
                    break;
                }
                file_buffer_get_char(current_fb);
                col++;
                token.type = TOK_LSHIFT;
                break;
            case '>':
                if(file_buffer_peek_char(current_fb) != '>'){
                    //unkown token
                    col++;
                    token.type = c;
                    break;
                }
                file_buffer_get_char(current_fb);
                col++;
                token.type = TOK_RSHIFT;
                break;
            case ';':
                while(true){
                    c = file_buffer_get_char(current_fb); 
                    if(c == '\n' || file_buffer_eof(current_fb)){
                        //if there are other tokens on the line 
                        // we want to put a new line
                        if(tokens.size > 1 && prev_newline != '\n'){
                            Token last_token = array_list_get(tokens, Token, tokens.size - 1);
                            if(last_token.line_number == line_number){
                                prev_newline = '\n';
                                token.type = TOK_NEW_LINE;
                                array_list_append(tokens, Token, token);
                            }
                        }
                        col = 1;
                        line_number++;
                        goto comment;
                    } 
                } 
                break;
            default: {
                if (isalpha(c) || c == '_' || c == '.' || c == '#'){
                    int temp = col;
                    col++;
                    scratch_buffer_append_char(c);
                    token = id_or_kw( &col);
                    token.line_number = line_number;
                    token.col = temp;
                    if(token.type != TOK_IDENTIFIER) scratch_buffer_clear();

                }else if(isdigit(c)){
                    token.type = TOK_INT;
                    col++;
                    scratch_buffer_append_char(c);
                    get_literal(&col); 
                }else{
                    //unkown token
                    col++;
                    token.type = c;
                }
            } 
        }

        char* id = scratch_buffer_as_str();

        //add the identifier to the token 
        if(id != NULL){
            char* dyn_id = strdup(id);
            if(dyn_id == NULL) fatal_error("Failed to alloc memory\n"); 
            token.literal = dyn_id; 
        }

        prev_newline = c;

        array_list_append(tokens, Token, token);
        comment:
        scratch_buffer_clear();
    } while(!file_buffer_eof(current_fb));

    
    //append new line after last token if there isn't one already 
    if(tokens.size != 0){
        Token temp = array_list_get(tokens, Token, tokens.size - 1);
        if(temp.type != TOK_NEW_LINE){
            Token new_tok = {TOK_NEW_LINE, 0, line_number, col};
            array_list_append(tokens, Token, new_tok);
        }

    }

    scratch_buffer_clear();
    return tokens;
}



void parser_error_loc(Parser* p, int line_num, int col, const char* fmt, ...){
    PRINT_COLORED_ERROR();
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    char* line = file_get_line(current_fb, line_num);
    fprintf(stderr, "%s: Line %d, Col %d\n", current_fb->name, line_num, col);
    fprintf(stderr, "%s\n", line);
    fprintf(stderr,"%*s\n",col, "^");
    scratch_buffer_clear();
    while(p->currentToken.type != TOK_NEW_LINE) parser_next_token(p);
    program.ret_code = 1;
    program.error_count++;

    if(program.error_count >= 20){
        fatal_error("Too many errors terminating assembly\n");
    }
}



void error_loc(int line_num, int col, const char* fmt, ...){
    PRINT_COLORED_ERROR();
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    char* line = file_get_line(current_fb, line_num);
    fprintf(stderr, "%s: Line %d, Col %d\n", current_fb->name, line_num, col);
    fprintf(stderr, "%s\n", line);
    fprintf(stderr,"%*s\n",col, "^");
    scratch_buffer_clear();
    program.ret_code = 1;
    program.error_count++;
    if(program.error_count >= 20){
        fatal_error("Too many errors terminating assembly\n");
    }
}




Token parser_next_token(Parser* p){
    if(p->tokenIndex < p->tokens->size){
        Token res = array_list_get((*p->tokens), Token, p->tokenIndex);
        p->currentToken = res;
        p->tokenIndex++;
        return res;

    }
    longjmp(p->jmp, 1);
}



Token parser_peek_token(Parser* p){
    if(p->tokenIndex < p->tokens->size){
        return array_list_get((*p->tokens), Token, p->tokenIndex);
    }
    longjmp(p->jmp, 1);
}


bool parser_expect_token(Parser* p, TokenType expected){
    if(p->currentToken.type != expected){
        if(p->currentToken.type > 255 || !isprint(p->currentToken.type)){
            parser_error(p, "Expected %s found %s\n", token_to_string(expected),
                token_to_string(p->currentToken.type));
        } else{
             parser_error(p, "Expected %s found \'%c\'\n", token_to_string(expected),
                p->currentToken.type);
        }
        return false;
    }
    return true;
}

bool parser_match_consume_token(Parser* p, TokenType m){
    if(p->currentToken.type == m){
        parser_next_token(p);
        return true;

    }
    return false;
}


bool __match(Parser* p, ...){
    va_list list;
    va_start(list, p);

    while(true){
        TokenType current = va_arg(list, TokenType);
        if(current == TOK_MAX) break;

        if(current == p->currentToken.type){ 
            va_end(list);
            return true;
        }
    }
    va_end(list);
    return false;
}




