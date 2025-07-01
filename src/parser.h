#pragma once
#include <setjmp.h>
#include <stdnoreturn.h>
#include "util.h"
#include "x86/types.h"

typedef struct {
    TokenType type;
    union{
        RegisterType reg;
        char* literal;  
        uint64_t instruction;
    };
    int line_number;
    int col;
} Token;



typedef struct {
    ArrayList* tokens; 
    Token currentToken; 
    uint32_t tokenIndex;
    jmp_buf jmp;
} Parser;




ArrayList tokenize_file();

noreturn void parser_fatal_error_loc(Parser* p, int line_num, int col, const char* fmt, ...);



#define parser_fatal_error(p, fmt, ...) parser_fatal_error_loc(p, (p)->currentToken.line_number, (p)->currentToken.col,fmt,##__VA_ARGS__)


Token parser_next_token(Parser* p);

Token parser_peek_token(Parser* p);

void parser_expect_token(Parser* p, TokenType expected);


static inline void parser_expect_consume_token(Parser* p, TokenType expected){
    parser_expect_token(p, expected);
    parser_next_token(p);
}


bool parser_match_consume_token(Parser* p, TokenType m);


bool __match(Parser* p, ...); 

#define match(p,...) __match(p, __VA_ARGS__, TOK_MAX)

#define parser_is_last_token(p) ((p)->tokenIndex == (uint32_t)((p)->tokens->size - 1))


