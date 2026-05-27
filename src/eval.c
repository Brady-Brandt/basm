#include "eval.h"
#include "src/parser.h"
#include "src/util.h"
#include "x86/types.h"
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static inline bool is_operator(TokenType type){
    return type == '+' || type == '*' || type =='/' || type == '-' || \
                 type == '~' || type == '^' || type == '&' || type == '|' \
                 || type == TOK_LSHIFT || type == TOK_RSHIFT || type == TOK_MOD;
}

static inline bool is_start_expr(TokenType type){
    return type == '(' || type == TOK_INT || type == '-' || type == '+' || type == '~';
}

static inline bool is_end_expr(TokenType type){
    return type == TOK_NEW_LINE || type == TOK_COMMA || type == ')';
}

static inline int get_precendence(TokenType type){
    switch (type) {
        case TOK_OR:
            return 1;
        case TOK_XOR:
            return 2;
        case TOK_AND:
            return 3;
        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return 4;
        case TOK_ADD:
        case TOK_SUB:
            return 5;
        case TOK_MULTIPLY:
        case TOK_DIVIDE:
        case TOK_MOD:
            return 6;
        default:
            return 0; 
    }
}


typedef struct{
    Token atom;
    struct Expression* lhs;
    struct Expression* rhs;
} Expression;


static Expression* parse_expression(Parser* p, jmp_buf* start, int min_precendence){
    Token t = parser_next_token(p);
    Expression* lhs = NULL;
    if(t.type == TOK_INT){
        lhs = malloc(sizeof(Expression));
        if(lhs == NULL) fatal_error("Out of memory\n");
        memset(lhs, 0, sizeof(Expression));
        lhs->atom = t;
    } else if (t.type == '(') {
        lhs = parse_expression(p, start, 0);
        parser_next_token(p);
        if(!parser_expect_token(p, ')')) longjmp(*start, 1);
    } else if(t.type == '-' || t.type == '+' || t.type == '~'){
        lhs = malloc(sizeof(Expression));
        if(lhs == NULL) fatal_error("Out of memory\n");
        memset(lhs, 0, sizeof(Expression));
        lhs->lhs = (struct Expression*)parse_expression(p,start, 254);
        lhs->atom = t;
    }
    else{ 
        parser_error(p,"Invalid Token in expression\n");
        longjmp(*start, 1);
    }
     
    while(true){
        Token op = parser_peek_token(p);
        if(is_end_expr(op.type)){
            break;
        }

        if(!is_operator(op.type)){
            parser_error_loc(p, op.line_number, op.col, "Expected Operator\n");
            longjmp(*start, 1);
        }

        int left_precendence = get_precendence(op.type);

        if(left_precendence < min_precendence) break;
    
        parser_next_token(p);
        Expression* rhs = parse_expression(p, start, left_precendence);

        Expression* tmp = malloc(sizeof(Expression));
        if(tmp == NULL) fatal_error("Out of memory\n");
        memset(tmp, 0, sizeof(Expression)); 
        tmp->lhs = (struct Expression*)lhs;
        lhs = tmp;

        lhs->atom = op;
        lhs->rhs = (struct Expression*)rhs; 
    }
    return lhs;
}


bool string_to_int(char* string, uint64_t* result){
    int base = 10;
    if(strncmp(string, "0x", 2) == 0){
        base = 16;
    } 

    errno = 0;
    char* endptr = NULL;
    *result = strtoull(string, &endptr, base);

    if(*endptr != 0 || errno == ERANGE){
        return false;
    } 

    return true;
}

static int64_t evaluate_expression(Parser* p, jmp_buf* start, Expression* expr){
    if(expr->atom.type == TOK_INT){
        uint64_t result = 0;
        if(!string_to_int(expr->atom.literal, &result)){
            parser_error_loc(p, expr->atom.line_number, expr->atom.col, "Invalid Number\n");
            free(expr);
            longjmp(*start, 1);
        } 
        free(expr);
        return result;
    }else {
        int64_t lhs = evaluate_expression(p, start, (Expression*)expr->lhs);

        if(expr->rhs == NULL){
            int64_t result = 0;
             switch (expr->atom.type) {
                case '+':
                    result = lhs;
                    break;
                case '-':
                    result = -lhs;
                    break;
                case '~':
                    result = ~lhs;
                    break;
                default: 
                    parser_error_loc(p, expr->atom.line_number, expr->atom.col, "Invalid Unary Operator\n");
                    free(expr);
                    longjmp(*start, 1);
            }   
            free(expr);
            return result;
        }

        int64_t rhs = evaluate_expression(p, start, (Expression*)expr->rhs); 
        int64_t result = 0;
        switch (expr->atom.type) {
            case '+':
                result = lhs + rhs;
                break;
            case '*':
                result = lhs * rhs;
                break;
            case '/':
                result = lhs / rhs;
                break;
            case '-':
                result = lhs - rhs;
                break;
            case TOK_LSHIFT:
                result = (uint64_t)lhs << (uint64_t) rhs; 
                break;
            case TOK_RSHIFT:
                result = (uint64_t)lhs >> (uint64_t)rhs; 
                break;
            case '%':
                result = lhs % rhs; 
                break;
            case '^':
                result = lhs ^ rhs;
                break;
            case '|':
                result = lhs | rhs;
                break;
            case '&':
                result = lhs & rhs;
                break; 
            default: 
                parser_error_loc(p,expr->atom.line_number, expr->atom.col, "Invalid Operator\n");
                free(expr);
                longjmp(*start, 1);
        }
        free(expr);
        return result;
    }
}



bool parse_and_eval_expression(Parser* p, int64_t* result){
    if(!is_start_expr(p->currentToken.type)){
        if(p->currentToken.type > 255 || !isprint(p->currentToken.type)){
            parser_error(p, "Expected start of expression got %s\n",
                token_to_string(p->currentToken.type));
        } else{
            parser_error(p, "Expected start of expression got \'%c\'\n", p->currentToken.type);
        }
        *result = 0;
        return false;
    }
    
    TokenType next = parser_peek_token(p).type;
    if(is_end_expr(next) && next != TOK_CLOSING_PAREN){
        if(!string_to_int(p->currentToken.literal, (uint64_t*)result)){
            parser_error(p, "Number out of range or invalid\n");
            return false;
        }
        return true;
    }

    //parse expression will query the first token 
    p->tokenIndex -= 1;

    jmp_buf expr_begin;
    if(setjmp(expr_begin) == 1){
        return false;
    }


    Expression* expr = parse_expression(p,&expr_begin, 0);
    
    *result = evaluate_expression(p, &expr_begin, expr);
    return true;
}


