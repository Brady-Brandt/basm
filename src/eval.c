#include "eval.h"
#include "src/util.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>


static inline bool is_operator(TokenType type){
    return type == '+' || type == '*' || type =='/' || type == '-' || \
                 type == '~' || type == '^' || type == '&' || type == '|';
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
        case TOK_ADD:
        case TOK_SUB:
            return 4;
        case TOK_MULTIPLY:
        case '/':
            return 5;
        default:
            return 0; 
    }
}


typedef struct{
    Token atom;
    struct Expression* lhs;
    struct Expression* rhs;
} Expression;


static Expression* parse_expression(Parser* assembler, jmp_buf* start, int min_precendence){
    Token t = parser_next_token(assembler);
    Expression* lhs = NULL;
    if(t.type == TOK_INT){
        lhs = malloc(sizeof(Expression));
        if(lhs == NULL) fatal_error("Out of memory\n");
        memset(lhs, 0, sizeof(Expression));
        lhs->atom = t;
    } else if (t.type == '(') {
        lhs = parse_expression(assembler, start, 0);
        parser_next_token(assembler);
        parser_expect_token(assembler, ')');
    } else if(t.type == '-' || t.type == '+' || t.type == '~'){
        lhs = malloc(sizeof(Expression));
        if(lhs == NULL) fatal_error("Out of memory\n");
        memset(lhs, 0, sizeof(Expression));
        lhs->lhs = (struct Expression*)parse_expression(assembler,start, 254);
        lhs->atom = t;
    }
    else{ 
        parser_error(assembler, "Invalid Token in expression\n");
        longjmp(*start, 1);
    }
     
    while(true){
        Token op = parser_peek_token(assembler);
        if(is_end_expr(op.type)){
            break;
        }

        int left_precendence = get_precendence(op.type);

        if(left_precendence < min_precendence) break;
    
        parser_next_token(assembler);
        Expression* rhs = parse_expression(assembler, start, left_precendence);

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


uint64_t string_to_int(char* string){
    int base = 10;

    //check for hexadecimal
    int size = strlen(string);
    if(size > 2){
        if(string[0] == '0' && string[1] == 'x'){
            base = 16;

        }  
    }
    return strtoull(string, NULL, base);
}

static int evaluate_expression(Parser* p, Expression* expr){
    if(expr->atom.type == TOK_INT){
        int result = string_to_int(expr->atom.literal);     
        free(expr);
        return result;
    }else {
        int lhs = evaluate_expression(p, (Expression*)expr->lhs);

        if(expr->rhs == NULL){
            int result = 0;
             switch (expr->atom.type) {
                case '+':
                    break;
                case '-':
                    result = -lhs;
                    break;
                case '~':
                    result = ~lhs;
                    break;
                default: 
                    parser_error(p, "Invalid Unary Operator: %s\n", token_to_string(expr->atom.type));
            }   
            free(expr);
            return result;
        }

        int rhs = evaluate_expression(p, (Expression*)expr->rhs); 
        int result = 0;
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
                parser_error_loc(p,expr->atom.line_number, expr->atom.col, "Invalid Operator %s\n");
        }
        free(expr);
        return result;
    }
}



uint64_t parse_and_eval_expression(Parser* p){
    if(!is_start_expr(p->currentToken.type)){
        parser_error(p, "Expected start of expression got %s\n", 
                token_to_string(p->currentToken.type));
        return 0;
    }
    
    TokenType next = parser_peek_token(p).type;
    if(is_end_expr(next) && next != TOK_CLOSING_PAREN){
        return string_to_int(p->currentToken.literal);
    }

    //parse expression will query the first token 
    p->tokenIndex -= 1;

    jmp_buf expr_begin;
    if(setjmp(expr_begin) == 1){
        return 0;
    }


    Expression* expr = parse_expression(p,&expr_begin, 0);

    return evaluate_expression(p, expr);
}


