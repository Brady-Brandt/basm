#include "x86/nmemonics.h"
#include "util.h"
#include "entry.h"
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <setjmp.h>




/*
digit — A digit between 0 and 7 indicates that the ModR/M byte of the instruction uses only the r/m (register
or memory) operand. The reg field contains the digit that provides an extension to the instruction's opcode.
• /r — Indicates that the ModR/M byte of the instruction contains a register operand and an r/m operand.
• cb, cw, cd, cp, co, ct — A 1-byte (cb), 2-byte (cw), 4-byte (cd), 6-byte (cp), 8-byte (co) or 10-byte (ct) value
following the opcode. This value is used to specify a code offset and possibly a new value for the code segment
register.
• ib, iw, id, io — A 1-byte (ib), 2-byte (iw), 4-byte (id) or 8-byte (io) immediate operand to the instruction that
follows the opcode, ModR/M bytes or scale-indexing bytes. The opcode determines if the operand is a signed
value. All words, doublewords, and quadwords are given with the low-order byte first.
• +rb, +rw, +rd, +ro — Indicated the lower 3 bits of the opcode byte is used to encode the register operand
without a modR/M byte. The instruction lists the corresponding hexadecimal value of the opcode byte with low
3 bits as 000b. In non-64-bit mode, a register code, from 0 through 7, is added to the hexadecimal value of the
opcode byte. In 64-bit mode, indicates the four bit field of REX.b and opcode[2:0] field encodes the register
operand of the instruction. “+ro” is applicable only in 64-bit mode. See Table 3-1 for the codes.
*/

/*
 * REX Page 529
- 7:4  0100
W   3   0 = Operand size determined by CS.D
        1 = 64 Bit Operand Size
R   2   Extension of the ModR/M reg field
X   1   Extension of the SIB index field
B   0   Extension of the ModR/M r/m field, SIB base field, or Opcode reg field
*/


#define REX_PREFIX(w,r,x,b) ((4 << 4) | (w * 8) | (r * 4) | (x * 2) | b)
#define REX_CLEAR_OP_SIZE(prefix) (prefix & 71)
#define REX_CHECK_OP_SIZE(prefix) (prefix & 16)

#define REX_W 8
#define REX_R 4
#define REX_X 2
#define REX_B 1



/*
0 AL AX EAX RAX
1 CL CX ECX RCX
2 DL DX EDX RDX
3 BL BX EBX RBX
4 AH, SPL 1 SP ESP RSP
5 CH, BPL 1 BP EBP RBP
6 DH, SIL 1 SI ESI RSI
7 BH, DIL 1 DI EDI RDI
*/

#define is_r256(reg) (reg >= REG_YMM0 && reg <= REG_YMM15)
#define is_r128(reg) (reg >= REG_XMM0 && reg <= REG_XMM15)
#define is_mmx(reg) (reg >= REG_MM0 && reg <= REG_MM7)
#define is_r64(reg) (reg >= REG_RAX && reg <= REG_R15)
#define is_r32(reg) (reg >= REG_EAX && reg <= REG_R15D)
#define is_r16(reg) (reg >= REG_AX && reg <= REG_R15W)
#define is_r8(reg) (reg >= REG_AL && reg <= REG_R15B)


//THIS MACRO SHOULD ONLY BE CALLED AFTER THE REGISTER HAS BEEN 
// CONVERTED TO AN INDEX 0-15
#define is_extended_reg(type) (type >= 8 && type <= 15)

//potentially remove mm and make rename this to is_sse_reg
#define is_advanced_reg(r) (r >= OPERAND_MM && r <= OPERAND_YMM)
#define is_reg32_or_64(type) (type == OPERAND_R32 || type == OPERAND_R64)
#define is_immediate(i) (i >= OPERAND_IMM8 && i <= OPERAND_IMM64)
#define is_label(l) (l >= OPERAND_L8 && l <= OPERAND_L64)
#define is_general_reg(r) (r >= OPERAND_R8 && r <= OPERAND_R64)
#define is_mem(m) (m >= OPERAND_M8 && m <= OPERAND_M80)
#define is_relative(x) (x >= OPERAND_REL8 && x <= OPERAND_REL32)


#define is_int32(n) ((int64_t)n <= INT32_MAX && (int64_t)n >= INT32_MIN)
#define is_int16(n) ((int64_t)n <= INT16_MAX && (int64_t)n >= INT16_MIN)
#define is_int8(n) ((int64_t)n <= INT8_MAX && (int64_t)n >= INT8_MIN)


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



static FileBuffer* current_fb = NULL;

static int string_cmp_lower(const void* a, const void* b) {
    const char* s1 = (const char*)a;
    const char* s2 = (const char*)b; 

    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);

        if (c1 != c2)
            return c1 - c2;

        s1++;
        s2++;
    }

    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}


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
        if(kw->type == TOK_REG){
            return (Token){TOK_REG, .reg = kw->value, 0, 0};
        } else if (kw->type == TOK_INSTRUCTION) {
            // in order to not lose information about the 
            // instruction we are going to store the index 
            // into the KEYWORD table of the instruction in the token 
            uint64_t index = ((uint8_t*)kw - (uint8_t*)KEYWORD_TABLE) / sizeof(struct Keyword); 
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



static void get_string(uint32_t line, int col){
    while(true){
        char next = file_buffer_peek_char(current_fb);
        if(next == '\"'){
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




static ArrayList tokenize_file(){
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
        token.type = TOK_MAX;
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
                get_string(line_number, col);
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
                    printf("%c\n", c);
                    token.type = TOK_MAX;
                }
            } 
        }

        char* id = scratch_buffer_as_str();

        //add the identifier to the token 
        if(id != NULL){
            char* dyn_id = strdup(id);
            if(dyn_id == NULL){
                fprintf(stderr, "Failed to alloc memory\n");
                exit(EXIT_FAILURE);
            }
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
    
    /* 
    for(int i = 0; i < tokens.size; i++){
        Token t = array_list_get(tokens, Token, i);
        if(t.type == TOK_IDENTIFIER || t.type == TOK_UINT || t.type == TOK_INT){
            printf("%s, %s, %d\n", token_to_string(t.type), t.literal, t.line_number);
        } 
        else if(t.type == TOK_REG){
            printf("%s, %d, %d\n", token_to_string(t.type), t.reg, t.line_number);
        
        }
        else{
            printf("%s, %ld, %d\n", token_to_string(t.type), t.instruction, t.line_number);
        }

    }    
    */
     
    scratch_buffer_clear();
    return tokens;
}


static Program program = {0};


typedef struct {
    ArrayList* tokens; 
    Token currentToken; 
    uint32_t tokenIndex;
    jmp_buf jmp;
} Parser;



noreturn void program_fatal_error(const char* fmt, ...){
    fprintf(stderr,"Error: ");
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    exit(EXIT_FAILURE);
}



void symbol_table_add(char* name, uint64_t offset, uint8_t section, uint8_t visibility){
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
        if(strcmp(e->name, name) == 0 && section != SECTION_UNDEFINED && visibility != VISIBILITY_UNDEFINED){
            //if we come across a label after declaring it global
            if(e->visibility == VISIBILITY_GLOBAL && visibility == VISIBILITY_LOCAL){
                e->section_offset = offset; 
                return;
            } else if(e->visibility == VISIBILITY_LOCAL && visibility == VISIBILITY_GLOBAL){
                e->visibility = VISIBILITY_GLOBAL;
                return;
            } else if (e->section == SECTION_UNDEFINED && e->visibility == VISIBILITY_UNDEFINED) {
                e->visibility = visibility;
                e->section_offset= offset;
                e->section = section;
                return;
            } 
           program_fatal_error("Many definitions of symbol: %s\n", name); 
        }
    }
    SymbolTableEntry e = {0};
    e.name = name;
    e.section_offset = offset;
    e.section = section;
    e.visibility = visibility;

    if(section == SECTION_TEXT){
        e.section_offset = program.text.size;
    }
    else if (section == SECTION_DATA){
        e.section_offset = program.data.size;
    }
    else{ 
        e.section_offset = program.bss.size;
    }


    array_list_append(program.symTable.symbols, SymbolTableEntry, e);
}

//TODO: MAKE IT A MULTIPASS ASSEMBLER
void symbol_table_add_instance(char* symbol_name, uint32_t offset, bool is_relative){
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);

        if(strcmp(e->name, symbol_name) == 0){
            if(e->instances.data == NULL){
                array_list_create_cap(e->instances, SymbolInstance, 2);
            }

            SymbolInstance current_instance = {offset, is_relative};
            array_list_append(e->instances, SymbolInstance, current_instance); 
            return;
        }
         
    }

    //add the symbol incase we encounter it later
    SymbolTableEntry e = {0};
    e.name = symbol_name;
    e.section_offset = MAX_OFFSET;
    e.section = SECTION_UNDEFINED;
    e.visibility = VISIBILITY_UNDEFINED;
    array_list_create_cap(e.instances, SymbolInstance, 2);
    SymbolInstance c = {offset, is_relative};
    array_list_append(e.instances, SymbolInstance, c); 
    array_list_append(program.symTable.symbols, SymbolTableEntry, e);
}


noreturn void parser_fatal_error(Parser* p, const char* fmt, ...){
    fprintf(stderr,"Error: ");
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    char* line = file_get_line(current_fb, p->currentToken.line_number);
    fprintf(stderr, "Line %d, Col %d\n", p->currentToken.line_number, p->currentToken.col);
    fprintf(stderr, "%s\n", line);
    fprintf(stderr,"%*s\n", p->currentToken.col, "^");
    exit(EXIT_FAILURE);
}


static Token parser_next_token(Parser* p){
    if(p->tokenIndex < p->tokens->size){
        Token res = array_list_get((*p->tokens), Token, p->tokenIndex);
        p->currentToken = res;
        p->tokenIndex++;
        return res;

    }
    longjmp(p->jmp, 1);
}



static Token parser_peek_token(Parser* p){
    if(p->tokenIndex < p->tokens->size){
        return array_list_get((*p->tokens), Token, p->tokenIndex);
    }
    longjmp(p->jmp, 1);
}


static void parser_expect_token(Parser* p, TokenType expected){
    if(p->currentToken.type != expected){
        parser_fatal_error(p, "Expected %s found %s\n", token_to_string(expected), token_to_string(p->currentToken.type));
    }
}



static inline void parser_expect_consume_token(Parser* p, TokenType expected){
    parser_expect_token(p, expected);
    parser_next_token(p);

}


static bool parser_match_consume_token(Parser* p, TokenType m){
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


#define match(p,...) __match(p, __VA_ARGS__, TOK_MAX)




typedef struct {
    char* name;
    //indices into token arraylist
    int starti; 
    int endi;
} PreprocesserSymbol;


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


static Expression* parse_expression(Parser* assembler, int min_precendence){
    Token t = parser_next_token(assembler);
    Expression* lhs = NULL;
    if(t.type == TOK_INT){
        lhs = malloc(sizeof(Expression));
        memset(lhs, 0, sizeof(Expression));
        lhs->atom = t;
    } else if (t.type == '(') {
        lhs = parse_expression(assembler, 0);
        parser_next_token(assembler);
        parser_expect_token(assembler, ')');
    } else if(t.type == '-' || t.type == '+' || t.type == '~'){
        lhs = malloc(sizeof(Expression));
        memset(lhs, 0, sizeof(Expression));
        lhs->lhs = (struct Expression*)parse_expression(assembler, 254);
        lhs->atom = t;
    }
    else{ 
        parser_fatal_error(assembler, "Invalid Token: %s\n", token_to_string(t.type));
    }
     
    while(true){
        Token op = parser_peek_token(assembler);
        if(is_end_expr(op.type)){
            break;
        }

        int left_precendence = get_precendence(op.type);

        if(left_precendence < min_precendence) break;
    
        parser_next_token(assembler);
        Expression* rhs = parse_expression(assembler, left_precendence);

        Expression* tmp = malloc(sizeof(Expression));
        memset(tmp, 0, sizeof(Expression)); 
        tmp->lhs = (struct Expression*)lhs;
        lhs = tmp;

        lhs->atom = op;
        lhs->rhs = (struct Expression*)rhs; 
    }
    return lhs;
}


static uint64_t string_to_int(char* string){
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

static int evaluate_expression(Parser* assembler, Expression* expr){
    if(expr->atom.type == TOK_INT){
        int result = string_to_int(expr->atom.literal);     
        free(expr);
        return result;
    }else {
        int lhs = evaluate_expression(assembler, (Expression*)expr->lhs);

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
                    parser_fatal_error(assembler, "Invalid Unary Operator: %s\n", token_to_string(expr->atom.type));
            }   
            free(expr);
            return result;
        }

        int rhs = evaluate_expression(assembler, (Expression*)expr->rhs); 
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
                parser_fatal_error(assembler, "Invalid Operator: %s\n", token_to_string(expr->atom.type));
        }
        free(expr);
        return result;
    }
}



static uint64_t parse_and_eval_instruction(Parser* assembler){
    if(!is_start_expr(assembler->currentToken.type)){
        parser_fatal_error(assembler, "Expected start of expression got %s\n", 
                token_to_string(assembler->currentToken.type));
    }
    
    TokenType next = parser_peek_token(assembler).type;
    if(is_end_expr(next) && next != TOK_CLOSING_PAREN){
        return string_to_int(assembler->currentToken.literal);
    }

    //parse expression will query the first token 
    assembler->tokenIndex -= 1;
    Expression* expr = parse_expression(assembler, 0);
    return evaluate_expression(assembler, expr);
}


void preprocessor_add_symbol(Parser* assembler, ArrayList* symbols){
    Token name = parser_next_token(assembler); 
    parser_expect_consume_token(assembler, TOK_IDENTIFIER);

    int starti = assembler->tokenIndex - 1;
    while(parser_next_token(assembler).type != TOK_NEW_LINE);
    int endi = assembler->tokenIndex - 1;

    for(int i = 0; i < symbols->size; i++){
        PreprocesserSymbol* s = &array_list_get((*symbols), PreprocesserSymbol, i);
        if(strcmp(name.literal,s->name) == 0){
            s->starti = starti;
            s->endi = endi;
            free(name.literal);
            return;
        }
    }
    PreprocesserSymbol s;
    s.name = name.literal;
    s.starti = starti;
    s.endi = endi;
    array_list_append((*symbols), PreprocesserSymbol, s);
}


void evaluate_preprocessor_statement(Parser* p, ArrayList* new_tokens, ArrayList* preprocess_symbols){
    Token t = parser_next_token(p);
    switch (t.type) {
        case TOK_DEFINE:{
            preprocessor_add_symbol(p, preprocess_symbols);
            break;
        }
        case TOK_IDENTIFIER: {
            bool found_symbol = false;
            for(int i = 0; i < preprocess_symbols->size; i++){
                PreprocesserSymbol s = array_list_get((*preprocess_symbols), PreprocesserSymbol, i);
                if(strcmp(t.literal,s.name) == 0){
                    for(int i = s.starti; i < s.endi; i++){
                        Token macro_token = array_list_get((*p->tokens), Token, i);
                        macro_token.line_number = t.line_number;
                        array_list_append((*new_tokens), Token, macro_token); 
                    } 
                    free(t.literal);
                    found_symbol = true;
                    break;
                }
            }
            if(!found_symbol){
                array_list_append((*new_tokens), Token, t); 
            } 
            break;
        }
        case TOK_IFDEF: {
            parser_next_token(p);
            parser_expect_token(p, TOK_IDENTIFIER);
            char* m_name = p->currentToken.literal;
            bool is_defined = false;
            if(strncmp("__", m_name, 2) == 0){
                #if defined(__linux__)
                    if(strcmp(m_name, "__LINUX__") == 0){
                        is_defined = true;
                        goto add_body;
                    }  
                #elif defined (_WIN64)
                    if(strcmp(m_name, "__WINDOWS__") == 0){ 
                        is_defined = true;
                        goto add_body;
                    }
                #endif
            } 
            for(int i = 0; i < preprocess_symbols->size; i++){
                PreprocesserSymbol s = array_list_get((*preprocess_symbols), PreprocesserSymbol, i);
                if(strcmp(m_name,s.name) == 0){ 
                    is_defined = true;
                    break;
                }
            }
        add_body:
            parser_next_token(p);
            parser_expect_token(p, TOK_NEW_LINE);
            if(is_defined){
                while(parser_peek_token(p).type != TOK_ENDIF){
                    if(p->tokenIndex == p->tokens->size - 1){
                        parser_fatal_error(p, "IF statement missing closing #endif\n");
                    }
                    evaluate_preprocessor_statement(p, new_tokens, preprocess_symbols);
                }
            } else{
                //if macro is not defined skip over all these tokens
                while(parser_peek_token(p).type != TOK_ENDIF){
                    parser_next_token(p);
                }
            }
            
            parser_next_token(p);
            parser_expect_consume_token(p, TOK_ENDIF);
            parser_expect_token(p, TOK_NEW_LINE);
            break;
        }
        case TOK_ENDIF: {
            parser_fatal_error(p, "Missing if statement\n");
            return;
        } 
        default:
            array_list_append((*new_tokens), Token, t);
    } 
}

ArrayList preprocess_tokens(ArrayList* tokens){ 
    ArrayList preprocess_symbols;
    array_list_create_cap(preprocess_symbols,PreprocesserSymbol, 16);

    ArrayList new_tokens;
    array_list_create_cap(new_tokens, Token, tokens->size);
 

    Parser p = {0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;
    array_list_create_cap(program.symTable.symbols, SymbolTableEntry, 16);

    while(1){
        if(setjmp(p.jmp) == 1) break;
        evaluate_preprocessor_statement(&p, &new_tokens, &preprocess_symbols); 
    }
   
    array_list_delete((*tokens));
    for(uint64_t i = 0; i < preprocess_symbols.size; i++){
        PreprocesserSymbol s = array_list_get(preprocess_symbols, PreprocesserSymbol, i);
        free(s.name); 
    }
    array_list_delete(preprocess_symbols);
    return new_tokens; 
}




static inline bool is_float(char* literal){
    int size = strlen(literal);
    for(int i = 0; i < size; i++){
        if(literal[i] == '.') return true;
    }
    return false;
}


static inline void init_section(Section* section, uint64_t start_size){
    if(section->data == NULL){
        section->capacity = start_size;
        section->data= malloc(section->capacity);
        if(section->data == NULL) program_fatal_error("Out of memory\n");
    }
}


static void section_realloc(Section* section){
   //TODO: CHECK FOR OVERFLOW
   uint64_t new_capacity = section->capacity * 2;
   section->data = realloc(section->data, new_capacity);
   if(section->data == NULL){
        program_fatal_error("Out of memory\n");
   }
   section->capacity = new_capacity;
}




#define check_section_size(section_ptr, bytes_to_add)\
    if(section->size + bytes_to_add >= section->capacity) section_realloc(section)
    


static inline void section_add_data(Section* section, void* data, size_t size){
    check_section_size(section, size);
    memcpy(section->data + section->size,data,size);  
    section->size += size;
}



//TODO: ALLOW PSUEDOINSTRUCTIONS WITHOUT LABELS 
static void parse_bss_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){
        parser_expect_token(p, TOK_IDENTIFIER); 
        Token id = p->currentToken;
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_COLON); 


        symbol_table_add(id.literal, program.bss.size, SECTION_BSS, VISIBILITY_LOCAL);

        int num = 1;
        switch (p->currentToken.type) {
            case TOK_RESB:
                num = 1; 
                break;
            case TOK_RESW:
                num = 2;
                break;
            case TOK_RESD:
                num = 4;
                break;
            case TOK_RESQ:
                num = 8;
                break;
            case TOK_REST:
                num = 10;
                break;
            case TOK_RESDQ:
                num = 16;
                break;
            case TOK_RESY:
                num = 32;
                break;
            default:
                parser_fatal_error(p, "Invalid bss section instruction\n");
        }
        parser_next_token(p);
        program.bss.size += num * parse_and_eval_instruction(p); 
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_NEW_LINE);

    } 
}



static void parse_data_section(Parser* p){ 
    while(p->currentToken.type != TOK_SECTION){
        parser_expect_token(p, TOK_IDENTIFIER); 
        Token id = p->currentToken;
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_COLON); 


        symbol_table_add(id.literal, program.data.size, SECTION_DATA, VISIBILITY_LOCAL);


        if(!match(p, TOK_DB, TOK_DW, TOK_DD, TOK_DQ,TOK_DT)){
            parser_fatal_error(p, "Invalid Data Section Instruction\n");
        }

        TokenType psuedo_instr = p->currentToken.type;
        parser_next_token(p);

        do{
            if(p->currentToken.type == TOK_INT){
                bool is_floating_point = false;
                if(is_float(p->currentToken.literal)){
                    if(!(psuedo_instr >= TOK_DD && psuedo_instr <= TOK_DT)){
                        parser_fatal_error(p, "Invalid floating point psuedoinstruction: %s", token_to_string(psuedo_instr));
                    }
                    is_floating_point = true;
                } 
                int64_t num = parse_and_eval_instruction(p);
                switch (psuedo_instr) {
                    case TOK_DB: {
                        uint8_t temp = 0;
                        if(num > UINT8_MAX && !is_int8(num)) parser_fatal_error(p, "Invalid Size: %ld\n", num);
                        temp = (uint8_t)num; 
                        section_add_data(&program.data,&temp, 1);
                        break;
                    }
                    case TOK_DW: {
                        uint16_t temp = 0;
                        if(num > UINT16_MAX && !is_int16(num)) parser_fatal_error(p, "Invalid Size: %ld\n", num);
                        temp = (uint16_t)num;  
                        section_add_data(&program.data,&temp, 2);
                        break;
                    }
                    case TOK_DD: {
                        uint32_t temp = 0;
                        if(is_floating_point){
                            float num = strtof(p->currentToken.literal, NULL); 
                            //TODO: CHECK FOR ERRORS
                            section_add_data(&program.data,&num, 4);
                            break;
                        } else{
                            if(num > UINT32_MAX && !is_int32(num)) parser_fatal_error(p, "Invalid Size: %ld\n", num);
                            temp = (uint32_t)num;
                        } 
                        section_add_data(&program.data,&temp, 4);
                        break;
                    }
                    case TOK_DQ: {
                        if(is_floating_point){
                            double num = strtod(p->currentToken.literal, NULL); 
                            //TODO: CHECK FOR ERRORS
                            section_add_data(&program.data,&num, 8);
                        } else{
                            uint64_t num = string_to_int(p->currentToken.literal);
                            section_add_data(&program.data,&num, 8);
                        }
                        break;
                    }
                    case TOK_DT: {
                        if(sizeof(long double) < 10){
                            parser_fatal_error(p, "Error: Don't support machines that don't have 128 bit floats yet");
                        }
                        long double num = strtold(p->currentToken.literal, NULL);
                        section_add_data(&program.data,&num, 10);
                        break;
                    }
                    default:
                        parser_fatal_error(p, "Unreachable"); 
                }
            } else if(p->currentToken.type == TOK_STRING){
                if(psuedo_instr != TOK_DB) parser_fatal_error(p, "Only byte size strings are allowed\n");
                section_add_data(&program.data, p->currentToken.literal, strlen(p->currentToken.literal) + 1);
            } else{
                parser_fatal_error(p, "Invalid for operand\n");
            } 
            parser_next_token(p);
        } while(parser_match_consume_token(p, TOK_COMMA));

        parser_expect_consume_token(p, TOK_NEW_LINE);

    }
}


#define mem_is_label(mem) ((mem.scale & 128))
#define mem_set_label(mem) (mem.scale |= 128)
#define mem_op_prefix(mem) (mem.scale & 64)
#define mem_set_prefix(mem) (mem.scale |= 64)
#define mem_get_scale(mem) ((mem.scale & 63))


typedef struct {
    OperandType type;
    union {
        struct{
            uint8_t registerIndex;
            uint8_t rex;
        } reg;

        uint8_t fpu_stack_index;
        uint8_t imm8;
        uint16_t imm16;
        uint32_t imm32;
        uint64_t imm64;

        char* label;

        struct {
            uint8_t rex;
            uint8_t base; //type reg
            uint8_t index; //type reg

            //the msb is going to indicate whether the union
            //is a label or an offset;
            //1 == label,0 == offset
            //next bit indicates if we need address size override prefix
            uint8_t scale;         

            //if value is zero, we aren't using either 
            union{
                int offset;            
                char* label;
            };
        } mem;


    };
} Operand;




static Operand parse_memory(Parser* p, OperandType mem_type){
    Operand result = {0};
    result.mem.base = REG_MAX;
    result.mem.index = REG_MAX;
    result.mem.rex = REX_PREFIX(0, 0, 0, 0);
    result.type = mem_type;

    OperandType base_size = OPERAND_NOP;
    OperandType index_size = OPERAND_NOP;

    bool check_scale = false;

    Token t = parser_next_token(p);

    while(t.type != TOK_CLOSING_BRACKET){
        switch (t.type) {
            case TOK_IDENTIFIER:
                result.mem.label = p->currentToken.literal;
                //TODO ALLOW BOTH LABEL AND INTEGER OFFSET 
                mem_set_label(result.mem);
                break;
            case TOK_REG: {
                    OperandType size = OPERAND_NOP;
                    uint8_t reg = REG_MAX;

                    
                    if(is_r64(p->currentToken.reg)){
                        result.mem.rex |= REX_W;
                        size = OPERAND_R64;
                        reg = p->currentToken.reg - REG_RAX;
                    } else if(is_r32(p->currentToken.reg)){
                        size = OPERAND_R32;
                        reg = p->currentToken.reg - REG_EAX;
                        mem_set_prefix(result.mem);
                    } else{
                        //In 64 bit mode these registers need to be 32 or 64 bit
                        parser_fatal_error(p, "Invalid Size\n");
                    }
 
                    if(result.mem.base == REG_MAX && parser_peek_token(p).type != TOK_MULTIPLY){
                        if(is_extended_reg(reg)){
                            result.mem.rex |= REX_B;
                            reg -= 8;
                        }
                        base_size = size;
                        result.mem.base = reg;
                    } else if(result.mem.index == REG_MAX){
                        if(is_extended_reg(reg)){
                            result.mem.rex |= REX_X;
                            reg -= 8;
                        }
                        index_size = size;
                        result.mem.index = reg;
                    } else{
                        parser_fatal_error(p, "Invalid address\n");
                    } 
                }
                break;
            case TOK_INT:{
                int temp = (int)string_to_int(p->currentToken.literal);
                if(check_scale){
                    switch (temp) {
                        case 1:
                            break;
                        case 2:
                            result.mem.scale |= 1; 
                            break;
                        case 4:
                            result.mem.scale |= 2;
                            break;
                        case 8:
                            result.mem.scale |= 3;
                            break;
                        default:
                            parser_fatal_error(p, "Invalid Scale Factor: %i\n", temp);
                    } 
                } else{
                    result.mem.offset = temp; 
                }
                break;
            }
            default:
                parser_fatal_error(p, "Invalid Token: %s\n", token_to_string(t.type));
        
        }

        t = parser_next_token(p);


        switch (t.type) {
            case TOK_MULTIPLY:{
                Token next = parser_peek_token(p); 
                if(next.type != TOK_INT || check_scale == true){ 
                    parser_fatal_error(p, "Invalid Address\n");
                }
                check_scale = true;
            }
            break;
            case TOK_ADD: {
                    Token next = parser_peek_token(p);
                    if(next.type == TOK_CLOSING_BRACKET){
                        parser_fatal_error(p, "Expected Label, Offset, or Register: %s\n", token_to_string(t.type));
                    }

                }
                break;
            case TOK_CLOSING_BRACKET:
                goto endloop;

            default:
                parser_fatal_error(p, "Invalid Token: %s\n", token_to_string(t.type)); 
        }


        t = parser_next_token(p);


    }

    endloop:


    //ensure the registeres are the same size
    if(base_size != OPERAND_NOP && index_size != OPERAND_NOP && base_size != index_size){ 
        parser_fatal_error(p, "Invalid: Registers must be the same size\n");
    }

    return result;
}





static Operand parse_operand(Parser* p){
    Operand result = {0};

    switch (p->currentToken.type) {
        case TOK_REG: {
            uint8_t w = 0;
            
            if(is_r256(p->currentToken.reg)){
                result.type = OPERAND_YMM;
                result.reg.registerIndex = p->currentToken.reg - REG_YMM0;
            }
            else if(is_r128(p->currentToken.reg)){
                result.type = OPERAND_XMM;
                result.reg.registerIndex = p->currentToken.reg - REG_XMM0;
            } else if(is_mmx(p->currentToken.reg)){
                result.type = OPERAND_MM;
                result.reg.registerIndex = p->currentToken.reg - REG_MM0;
            }
            else if(is_r64(p->currentToken.reg)){
                w = 1;
                result.type = OPERAND_R64;
                result.reg.registerIndex = p->currentToken.reg - REG_RAX;
            } else if(is_r32(p->currentToken.reg)){
                result.type = OPERAND_R32;
                result.reg.registerIndex = p->currentToken.reg - REG_EAX;
            } else if(is_r16(p->currentToken.reg)){
                result.type = OPERAND_R16;
                result.reg.registerIndex = p->currentToken.reg - REG_AX;
            } else{
                result.type = OPERAND_R8;
                result.reg.registerIndex = p->currentToken.reg - REG_AL;
            }
            result.reg.rex = REX_PREFIX(w, 0, 0, 0);
            return result;
        }

        case TOK_OPENING_PAREN:
        case TOK_ADD:
        case TOK_NEG:
        case TOK_SUB:
        case TOK_INT:
            result.imm64 = parse_and_eval_instruction(p);
            result.type = (result.imm64 > INT64_MAX) ? OPERAND_SIGNED : OPERAND_IMM64;
            return result; 

        case TOK_IDENTIFIER: 
            result.type = OPERAND_L64;
            result.label = p->currentToken.literal; 
            return result;

        case TOK_ST0:
        case TOK_ST1:
        case TOK_ST2:
        case TOK_ST3:
        case TOK_ST4:
        case TOK_ST5:
        case TOK_ST6:
        case TOK_ST7:
            result.type = OPERAND_STI;
            result.fpu_stack_index = p->currentToken.type - TOK_ST0;
            return result;

        case TOK_BYTE:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M8); 

        case TOK_WORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M16); 

        case TOK_DWORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M32); 

        case TOK_QWORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M64); 
        case TOK_TWORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M80); 
        case TOK_DQWORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M128);
        case TOK_YWORD:
            parser_next_token(p);
            parser_expect_token(p, TOK_OPENING_BRACKET);
            return parse_memory(p, OPERAND_M256);
        case TOK_OPENING_BRACKET:
            return parse_memory(p, OPERAND_MEM_ANY); 
        default:
            program_fatal_error("Operand Type not supported yet: %s\n",token_to_string(p->currentToken.type));

    
    }
}



//converts an instruction of operand type register or memory to operand type RM 
#define TO_RM(input_instr, reg8_or_m8) (input_instr + (OPERAND_RM8 - reg8_or_m8))

static bool check_operand_type(OperandType table_instr, OperandType input_instr, uint8_t reg_index){
    if(table_instr == input_instr) return true;

    //these instructions either take a memory location or registers
    if(table_instr >= OPERAND_RM8 && table_instr <= OPERAND_RM64){
        if(table_instr == TO_RM(input_instr, OPERAND_M8)|| table_instr == TO_RM(input_instr, OPERAND_R8)){
            return true;
        }
    }

    if(table_instr == OPERAND_M && input_instr >= OPERAND_MEM_ANY && input_instr <= OPERAND_M64) return true;

    if(table_instr == OPERAND_REL32 && input_instr == OPERAND_L64) return true;


    if(table_instr >= OPERAND_XMMM32 && table_instr <= OPERAND_XMMM128){
        if(input_instr == OPERAND_XMM || (input_instr + (OPERAND_XMMM8 - OPERAND_M8)) == table_instr ) return true;
    }
 
    if(table_instr == OPERAND_YMMM256 && (input_instr == OPERAND_YMM || input_instr == OPERAND_M256)) return true;

    if(table_instr == OPERAND_MMM32 || table_instr == OPERAND_MMM64){
        return input_instr == OPERAND_MM || table_instr == input_instr + (OPERAND_MMM32 - OPERAND_M32);
    }

    if(table_instr >= OPERAND_AL && table_instr <= OPERAND_RAX){
        if(input_instr == OPERAND_R64 && reg_index == 0){
            return table_instr == OPERAND_RAX;
        } else if (input_instr == OPERAND_R32 && reg_index == 0) { 
            return table_instr == OPERAND_EAX;
        } else if (input_instr == OPERAND_R16) {
            if(reg_index == 0 && table_instr == OPERAND_AX) return true; 
            else if(reg_index == 2 && table_instr == OPERAND_DX) return true; 
        } else if (input_instr == OPERAND_R8) {
            if(reg_index == 0 && table_instr == OPERAND_AL) return true;
            else if(reg_index == 1 && table_instr == OPERAND_CL) return true;  
        }
    }

 
    return false;
}



static Instruction* find_instruction(uint64_t instr, Operand operand[4]){
    //get the location in the instruction instruction variant table
    uint64_t op_table_index = KEYWORD_TABLE[instr].value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;


    // loop through each variant of the instruction check if the operands match 
    for(int i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];
        bool op1_bool = check_operand_type(instruct_var.op1, operand[0].type, operand[0].reg.registerIndex);
        if(!op1_bool) continue;
        bool op2_bool = check_operand_type(instruct_var.op2, operand[1].type, operand[1].reg.registerIndex); 
        if(!op2_bool) continue;
        bool op3_bool = check_operand_type(instruct_var.op3, operand[2].type, operand[2].reg.registerIndex);
        if(!op3_bool)continue;;
        if(operand[3].type != OPERAND_NOP){
            if(instruct_var.ib & INSTR_OP4_IS_REG){
                //if the 4 operand is a reg it must be the same type of register as the first
                if(operand[3].type != operand[0].type) continue;
            } else {
                if(operand[3].type != OPERAND_IMM8 && instruct_var.ib != 1) continue;
            }
        } 


        return (Instruction*)&INSTRUCTION_TABLE[i];
    }

    return NULL;
}



#define MODRM_INDEX 0
#define SIB_INDEX 1
#define DISPLACEMENT_SIZE 4

static int modrm_sib_fields(Operand* op, uint8_t *data, char** label){
    uint8_t ADDRESS_OVERRIDE_PREFIX = 0x67;
    int size = 1;
    int32_t offset = (int32_t)op->mem.offset;

    if(mem_is_label(op->mem)){
        (*label) = op->mem.label;
        offset = 0;
    }
    if(mem_op_prefix(op->mem)) section_add_data(&program.text, &ADDRESS_OVERRIDE_PREFIX, 1);

    if(op->mem.base == REG_MAX && op->mem.index == REG_MAX){
        //TODO: FIGURE OUT WHEN THE R/M FIELD IS 101  
        data[MODRM_INDEX] |= 0x4;
        data[SIB_INDEX] = 0x25;
        size++;
        size += DISPLACEMENT_SIZE;  
        memcpy(data + 2, &offset, DISPLACEMENT_SIZE);
    } else if (op->mem.base != REG_MAX && op->mem.index == REG_MAX) {
        //TODO: FIGURE OUT WHEN 8 bit displacements are used 
        data[MODRM_INDEX] |= op->mem.base;
        //check if we have an offset
        if(op->mem.label != 0){
            // we want a 32 bit displacement
            data[MODRM_INDEX] += 0x80;
            size += DISPLACEMENT_SIZE;
            // the esp register requires a sib byte
            if(op->mem.base == REG_ESP){
                data[SIB_INDEX] = 0x24;
                size++;
                memcpy(data + 2, &offset, DISPLACEMENT_SIZE);
            } else{
                memcpy(data + 1, &offset, DISPLACEMENT_SIZE);
            }
        } else{
            // the esp register requires a sib byte
            if(op->mem.base == REG_ESP){
                data[SIB_INDEX] = 0x24; 
                size += 1 + DISPLACEMENT_SIZE;
                memcpy(data + 2, &offset, DISPLACEMENT_SIZE);
            } else if(op->mem.base == REG_EBP){
                // in this case we have to just do an 8 bit displacement of zeros 
                data[MODRM_INDEX] |= 1 << 6;
                size++;
                uint8_t zero = 0;
                memcpy(data + 1, &zero, 1);
            }
        }
    } else if (op->mem.base == REG_MAX && op->mem.index != REG_MAX) {
        //need an sib byte
        data[MODRM_INDEX] |= 0x4;
        data[SIB_INDEX] |= op->mem.scale << 6;
        data[SIB_INDEX] |= op->mem.index << 3;
        data[SIB_INDEX] |= 0x5;
        size++;
        size += DISPLACEMENT_SIZE;
        //either copy zeros, an offset or zeros for a label
        memcpy(data + 2, &offset, DISPLACEMENT_SIZE); 
    } else{
        //both a base and an index 
        data[MODRM_INDEX] |= 0x4;
        data[SIB_INDEX] |= op->mem.scale << 6;
        data[SIB_INDEX] |= op->mem.index << 3;
        data[SIB_INDEX] |= op->mem.base;
        size++;

        if(op->mem.offset != 0){
            //SIB PLUS DIS 32
            data[MODRM_INDEX] |= 1 << 7;
            memcpy(data+2, &offset, DISPLACEMENT_SIZE);
            size += DISPLACEMENT_SIZE;
        }
    } 
    

    return size;
}

/*
W   3   0 = Operand size determined by CS.D
        1 = 64 Bit Operand Size
R   2   Extension of the ModR/M reg field
X   1   Extension of the SIB index field
B   0   Extension of the ModR/M r/m field, SIB base field, or Opcode reg field
*/

/*
Two Byte 

0xC5 R vvvv L pp 
R - > Extension of reg field
vvvv -> ~register index ones complement

Three byte
0xC4 R X B mmmmm   W vvvv L pp


*/

#define VEX_TWO_BYTE_R 0x8000
#define VEX_TWO_BYTE_X 0x4000
#define VEX_TWO_BYTE_B 0x2000
#define VEX_UNUSED_REG 0x78



//R bit gets inverted 
#define REX_TO_ONE_BYTE_VEX(rex) ((~(rex >> 2)) << 7)

#define ONE_VEX_TO_TWO_BYTE_VEX(vex) (((VEX_TWO_BYTE_R) & ((vex & 0x80) << 8)) | vex & 0x7F7F)
#define VEX_REGISTER(reg) (((~(reg)) & 0xF) << 3)


static void emit_vex_instruction(Instruction* instruction, Operand operand[4]){
    uint16_t vex = 0xE000;

    uint8_t modrm_sib[6] = {0};
    uint8_t modrm_size = 0;
    char* lbl = NULL;
    int imm_index = 1;

    
    if(is_reg32_or_64(operand[0].type)){
        if(operand[0].reg.rex & REX_B) operand[0].reg.rex |= REX_R; 

        //these few instructions have a different operand order encoding then all the other ones
        // so we just swap the second and third operand to keep it consistent
        if(instruction->op2 == OPERAND_RM32 || instruction->op2 == OPERAND_RM64){
            Operand tmp = operand[1];
            operand[1] = operand[2];
            operand[2] = tmp;
        } else if (instruction->op2 == OPERAND_XMM) {
            if(operand[0].reg.rex & REX_R) operand[0].reg.rex |= REX_B; 
            //swap rex prefixes as well
            Operand tmp = operand[0];
            operand[0] = operand[1];
            operand[1] = tmp; 
        }
    }



    if(is_advanced_reg(operand[0].type) || is_reg32_or_64(operand[0].type)){
        vex |= (uint8_t)REX_TO_ONE_BYTE_VEX(operand[0].reg.rex);
        if(is_advanced_reg(operand[1].type) || is_reg32_or_64(operand[1].type)){
            imm_index++;
            uint8_t reg_portion_modrm = 0;
            uint8_t rm_portion_modrm = 0;

            if(is_advanced_reg(operand[2].type) || is_reg32_or_64(operand[2].type)){
                imm_index++;
                if(operand[1].reg.rex & REX_B){
                    operand[1].reg.registerIndex += 8;
                }
                rm_portion_modrm = 2;
                vex |= VEX_REGISTER(operand[1].reg.registerIndex);
            } else if(is_mem(operand[2].type)){
                imm_index++;
                vex ^= operand[2].mem.rex << 13;
                if(operand[1].reg.rex & REX_B){
                    operand[1].reg.registerIndex += 8;
                }
                vex |= VEX_REGISTER(operand[1].reg.registerIndex);
                modrm_sib[MODRM_INDEX] |= (operand[0].reg.registerIndex << 3);
                modrm_size = modrm_sib_fields(&operand[2], modrm_sib, &lbl);
                goto encode_vex;
            } else{
                vex |= VEX_UNUSED_REG; 
                rm_portion_modrm = 1;
            }

            //if this is set we need to use 3 byte vex
            if(operand[rm_portion_modrm].reg.rex & REX_B){
                vex ^= VEX_TWO_BYTE_B;
            }
 
            modrm_size = 1;
            modrm_sib[MODRM_INDEX] |= 192;
            modrm_sib[MODRM_INDEX] |=(operand[reg_portion_modrm].reg.registerIndex << 3);
            modrm_sib[MODRM_INDEX] |= operand[rm_portion_modrm].reg.registerIndex; 
        } else if (is_mem(operand[1].type)){
            imm_index++;
            vex ^= (uint8_t)(operand[0].reg.rex << 7);
            vex ^= operand[1].mem.rex << 13;

            // for most vex instructions if second operand
            // is a memory address then its only a 2 operand instruction
            // but for some instructions that operate on rm32/64 like BEXTR this is not the case
            if(operand[2].type != OPERAND_NOP){
                if(operand[2].reg.rex & REX_B){
                    operand[2].reg.registerIndex += 8;
                }
                vex |= VEX_REGISTER(operand[2].reg.registerIndex);
            } else{
                vex |= VEX_UNUSED_REG; 
            }

            modrm_sib[MODRM_INDEX] |= (operand[0].reg.registerIndex << 3);
            modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
        }
    } else if(is_mem(operand[0].type)){
        vex |= 0x80;
        if(operand[1].reg.rex & REX_R) operand[1].reg.rex |= REX_B; 
        vex ^= (uint8_t)(operand[1].reg.rex << 7);
        vex ^= operand[0].mem.rex << 13;
        vex |= VEX_UNUSED_REG; 
        modrm_sib[MODRM_INDEX] |= (operand[1].reg.registerIndex << 3);
        modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
    }


    encode_vex:
    if((instruction->r & INSTR_USES_2VEX) && ((vex & 0xE000) == 0xE000)){
        vex |= instruction->three_vex;
        uint8_t tmp = 0xC5;
        section_add_data(&program.text, &tmp, 1); 
        section_add_data(&program.text, &vex, 1); 
    } else {
        vex = ONE_VEX_TO_TWO_BYTE_VEX(vex);
        vex |= instruction->three_vex;
        //instruction that can be encoded with 2 byte
        //needs to be encoded with 3 byte vex
        //set mmmmm part to 1
        if(((vex >> 8) & 0x1F) == 0){
            vex |= (1 << 8);
        }
        uint8_t tmp = 0xC4;
        section_add_data(&program.text, &tmp, 1); 

        //on little endian lsb goes first
        //need to put the upper 16 bits in first though
        uint8_t upper = vex >> 8;
        uint8_t lower = (0x00FF & vex);
        section_add_data(&program.text, &upper, 1);  
        section_add_data(&program.text, &lower, 1);  
    }


    section_add_data(&program.text, instruction->bytes, instruction->size); 
    if(modrm_size != 0) section_add_data(&program.text, modrm_sib, modrm_size);

    if(lbl != NULL){
        symbol_table_add_instance(lbl, program.text.size - DISPLACEMENT_SIZE, false);
    }

    
    if(instruction->ib != -1){
        if(instruction->ib & INSTR_OP4_IS_REG){
            uint8_t payload = (operand[3].reg.registerIndex) << 4; 
            section_add_data(&program.text, &payload, 1); 
        } else{ 
            section_add_data(&program.text, &operand[imm_index].imm8, 1); 
        }
    }



}



static void emit_instruction(Instruction* instruction, Operand operand[4]){
    uint8_t rex = instruction->rex;

    if((instruction->r & INSTR_USES_2VEX) || (instruction->r & INSTR_USES_3VEX)){
        emit_vex_instruction(instruction, operand);
        return;
    }

    uint8_t opcode[4] = {0};
    memcpy(opcode, instruction->bytes, instruction->size);


    uint8_t modrm_sib[6] = {0};
    uint8_t modrm_size = 0;
    char* lbl = NULL;

    int imm_index = 1;


    //indicate opcode extension in the reg portion of modrm
    if(instruction->digit != -1){
        modrm_size = 1;
        modrm_sib[MODRM_INDEX] |= (instruction->digit << 3);
    }

    // Instruction takes no operands
    if(operand[0].type == OPERAND_NOP){
        section_add_data(&program.text, opcode, instruction->size); 
        return;
    }

    if(operand[1].type == OPERAND_NOP){
        // handle call, jmp, jcc instructions
        if(operand[0].type == OPERAND_L64){
            section_add_data(&program.text, opcode, instruction->size); 
            //assume its a relative address
            uint32_t zero = 0;
            //add some temp zeros
            section_add_data(&program.text, &zero, 4);
            symbol_table_add_instance(operand[0].label, program.text.size, true); 
            return;
        } else if (is_general_reg(operand[0].type) && is_extended_reg(operand[0].reg.registerIndex)) {
            operand[0].reg.rex |= REX_B;
            operand[0].reg.registerIndex -= 8; 
        } else if (operand[0].type == OPERAND_STI){
            opcode[instruction->size - 1] += operand[0].fpu_stack_index; 
        } 
    }

   
    if(is_general_reg(operand[0].type) || is_advanced_reg(operand[0].type)){
        rex |= operand[0].reg.rex;
        //indicates we add register to the opcode
        if((instruction->r & ADD_REG_TO_OPCODE)){
            opcode[instruction->size - 1] += operand[0].reg.registerIndex; 
        } 
        //if we encode both operands in the modrm byte
        else if ((instruction->r & MODRM_CONTAINS_REG_AND_MEM)) {
            if(is_general_reg(operand[1].type)){
                rex |= operand[1].reg.rex;
                modrm_size = 1;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |=(operand[1].reg.registerIndex << 3);
                modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex; 
            } else if(is_advanced_reg(operand[1].type)){
                rex |= operand[1].reg.rex;  
                modrm_size = 1;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |=(operand[0].reg.registerIndex << 3);
                modrm_sib[MODRM_INDEX] |= operand[1].reg.registerIndex; 
            }else{
               rex |= operand[1].mem.rex;
               modrm_sib[MODRM_INDEX] |= (operand[0].reg.registerIndex << 3);
               modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
            }
            imm_index++;
        } 
        // if we have an opcode extension but still need to encode data in the modrm
        else {
             //     op ext   reg index
            //11    000      000
            modrm_sib[MODRM_INDEX] |= 0xC0;
            modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex; 
        }

    } else if(is_mem(operand[0].type)){
        rex |= operand[0].mem.rex;
        if(is_general_reg(operand[1].type)){
            rex |= operand[1].reg.rex;
            modrm_size = 1;
            modrm_sib[MODRM_INDEX] |= operand[1].reg.registerIndex << 3; 
            modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
            imm_index++;
        } else if(is_immediate(operand[1].type) || operand[1].type == OPERAND_NOP){
            modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
        }   
    }     


    if(rex > 0x40){
        //rex prefix must come right before escape prefix
        if(opcode[1] == 0x0f){ 
            section_add_data(&program.text, &opcode[0], 1);
            opcode[0] = rex;
        } else{
            section_add_data(&program.text, &rex, 1);
        }
    }
         
    section_add_data(&program.text, opcode, instruction->size); 
    if(modrm_size != 0) section_add_data(&program.text, modrm_sib, modrm_size);

    if(lbl != NULL){
        symbol_table_add_instance(lbl, program.text.size - DISPLACEMENT_SIZE, false);
    } 


    //indicates an immediate
    //for now immediates will only be in operand 2
    switch (instruction->ib) {
        //no immediate
        case -1:
            break;
        case 1: 
            section_add_data(&program.text, &operand[imm_index].imm8, 1);
            break;
        case 2: {
            section_add_data(&program.text, &operand[imm_index].imm16, 2);
            break;
        }
        case 4: {
            section_add_data(&program.text, &operand[imm_index].imm32, 4);
            break;
        }
       case 8: {
            section_add_data(&program.text, &operand[imm_index].imm64, 8);
            break;
        }
        default:
            program_fatal_error("Unreachable\n");
        
    }
        
}

static void match_operand_pairs(Operand* op1, Operand *op2){
    uint16_t operand_override_prefix = 0x66;

    //if we have extended registers r8-r15
    //convert them to their respected index  and set the REX prefix accordingly
    if((is_general_reg(op1->type) || is_advanced_reg(op1->type)) && is_extended_reg(op1->reg.registerIndex)){
        //if op2 is a memory address, the first operand goes into  
        // the reg portion of modrm instead of the r/m portion
        if(op2->type >= OPERAND_MEM_ANY && op2->type <= OPERAND_M64 || is_advanced_reg(op2->type)){
            op1->reg.rex |= REX_R;
        } else{
            op1->reg.rex |= REX_B;
        }
        op1->reg.registerIndex -= 8;
    }
    if((is_general_reg(op2->type) || is_advanced_reg(op2->type)) && is_extended_reg(op2->reg.registerIndex)){
        if((op1->type >= OPERAND_MEM_ANY && op1->type <= OPERAND_M64) || is_general_reg(op1->type)){
            op2->reg.rex |= REX_R;
        } else{
            op2->reg.rex |= REX_B;
        }
        op2->reg.registerIndex -= 8;
    }

 
     
    if(op2->type == OPERAND_IMM64 || op2->type == OPERAND_SIGNED){
        switch (op1->type) {
            case OPERAND_R64:
                if(op2->type == OPERAND_IMM64 && op2->imm64 <= UINT32_MAX){
                    op1->reg.rex = REX_CLEAR_OP_SIZE(op1->reg.rex);
                    op1->type = OPERAND_R32;
                    op2->type = OPERAND_IMM32;
                    op2->imm32 = (uint32_t)op2->imm64;
                } else if (op2->type == OPERAND_SIGNED) { 
                    op1->reg.rex = REX_CLEAR_OP_SIZE(op1->reg.rex);
                    op2->type = OPERAND_IMM64;
                }
                break;
            case OPERAND_M64:
            case OPERAND_R32:
            case OPERAND_M32:
                if(op2->type == OPERAND_SIGNED){
                    if(!is_int32(op2->imm64)) program_fatal_error("Invalid Operand Size\n"); 
                } else{
                    if(!(op2->imm64 <= UINT32_MAX))program_fatal_error("Invalid Operand Size\n"); 
                }
                op2->type = OPERAND_IMM32;
                op2->imm32 = (uint32_t)op2->imm64;
                break;
            case OPERAND_M16:
            case OPERAND_R16:
                if(op2->type == OPERAND_SIGNED){
                    if(!is_int16(op2->imm64)) program_fatal_error("Invalid Operand Size\n"); 
                } else{
                    if(!(op2->imm64 <= UINT16_MAX))program_fatal_error("Invalid Operand Size\n"); 
                }
                section_add_data(&program.text,&operand_override_prefix, 1);
                op2->type = OPERAND_IMM16;
                op2->imm16 = (uint16_t)op2->imm64;
                break;
            case OPERAND_M8:
            case OPERAND_R8:
                if(op2->type == OPERAND_SIGNED){
                    if(!is_int8(op2->imm64)) program_fatal_error("Invalid Operand Size\n"); 
                } else{
                    if(!(op2->imm64 <= UINT8_MAX))program_fatal_error("Invalid Operand Size\n"); 
                }
                op2->type = OPERAND_IMM8;
                op2->imm8 = (uint8_t)op2->imm64;
                break;
            case OPERAND_MEM_ANY:
                program_fatal_error("Expected Size specifier\n");
                break;
        
            default:
                return;

        }
        return;
    }


    switch (op1->type) {
        case OPERAND_R64:
            if(op2->type == OPERAND_MEM_ANY) op2->type = OPERAND_M64;
            return;
        case OPERAND_R32:
            if (op2->type == OPERAND_MEM_ANY)op2->type = OPERAND_M32;
            return;
        case OPERAND_R16:
            if (op2->type == OPERAND_MEM_ANY){
                section_add_data(&program.text,&operand_override_prefix, 1);
                op2->type = OPERAND_M32;
            }
            return;
        case OPERAND_R8:
            if(op2->type == OPERAND_MEM_ANY) op2->type = OPERAND_M8;
            return;

        case OPERAND_MEM_ANY:
            if(is_general_reg(op2->type)){
                op1->type = op2->type + (OPERAND_M8 - OPERAND_R8);
                if(op2->type == OPERAND_R16){
                    section_add_data(&program.text,&operand_override_prefix, 1);
                }
            }
            return;
        //TODO: TURN THIS SWITCH INTO IF STATEMENTS
        case OPERAND_M8:
        case OPERAND_M16:
        case OPERAND_M32:
        case OPERAND_M64:
        case OPERAND_M128:
        case OPERAND_XMM:
        case OPERAND_YMM:
        case OPERAND_MM:
            return;
        default:
            program_fatal_error("Operand Combo not supported yet: %s, %s\n", 
                    operand_to_string(op1->type), operand_to_string(op2->type));

    }
}





static void match_operand_triples(Operand* op1, Operand *op2, Operand* op3){
    if(op3->type == OPERAND_IMM64){
         if (op3->imm64 <= UINT8_MAX) {
            op3->type = OPERAND_IMM8;
            op3->imm8 = (uint8_t)(op3->imm64); 
        } else if(op3->imm64 <= UINT16_MAX){
            op3->type = OPERAND_IMM16;
            op3->imm16 = (uint16_t)(op3->imm64);
        } else if(op3->imm64 <= UINT32_MAX){
            op3->type = OPERAND_IMM32;
            op3->imm32 = (uint32_t)(op3->imm64);
        } 
    } else if (op3->type == OPERAND_SIGNED) {
        if(is_int8(op3->imm64)){
            op3->type = OPERAND_IMM8;
            op3->imm8 = (int8_t)(op3->imm64);
        }else if(is_int16(op3->imm64)){
            op3->type = OPERAND_IMM16;
            op3->imm16 = (int16_t)(op3->imm64);
        }else if(is_int32(op3->imm64)){
            op3->type = OPERAND_IMM32;
            op3->imm32 = (int32_t)(op3->imm64);
        }  
    }

    if(is_general_reg(op1->type) && is_general_reg(op2->type) && is_immediate(op3->type)){
        uint8_t tmp = op1->reg.registerIndex;        
        // the order of how 2 registers is packed into modrm is different 
        // for instructions with three parameters so we just swap them 
        op1->reg.registerIndex = op2->reg.registerIndex;
        op2->reg.registerIndex = tmp;
    }

    //if we have extended registers r/xmm/ymm8-r/xmm/ymm15
    //convert them to their respected index  and set the REX prefix accordingly
    if((is_general_reg(op1->type) || is_advanced_reg(op1->type)) && is_extended_reg(op1->reg.registerIndex)){
        //if op2 is a memory address, the first operand goes into  
        // the reg portion of modrm instead of the r/m portion
        if((op2->type >= OPERAND_MEM_ANY && op2->type <= OPERAND_M64) || is_advanced_reg(op2->type)){
            op1->reg.rex |= REX_R;
        } else{
            op1->reg.rex |= REX_B;
        }
        op1->reg.registerIndex -= 8;
    }
    if((is_general_reg(op2->type) || is_advanced_reg(op2->type)) && is_extended_reg(op2->reg.registerIndex)){
        if((op1->type >= OPERAND_MEM_ANY && op1->type <= OPERAND_M64)){
            op2->reg.rex |= REX_R;
        } else{
            op2->reg.rex |= REX_B;
        }
        op2->reg.registerIndex -= 8;
    }

    if((is_advanced_reg(op3->type) || is_reg32_or_64(op3->type)) && is_extended_reg(op3->reg.registerIndex)){
        op3->reg.rex |= REX_B;
        op3->reg.registerIndex -= 8;
    }
}




//temp function
static void print_text_section(){
    for(int i = 0; i < program.text.size; i++){
        printf("%02x ", program.text.data[i]);
    }
}




static void parse_text_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){
        if(p->currentToken.type == TOK_SECTION) break;

        if(p->currentToken.type == TOK_GLOBAL || p->currentToken.type == TOK_EXTERN){
            int section = (p->currentToken.type == TOK_GLOBAL) ? SECTION_TEXT : SECTION_EXTERN;
            parser_next_token(p);
            parser_expect_token(p, TOK_IDENTIFIER);

            Token id = p->currentToken;
            //TODO: ALLOW MANY GLOBAL DECLARATIONS AT ONCE
            symbol_table_add(id.literal, 0, section, VISIBILITY_GLOBAL);
            parser_next_token(p);
            parser_expect_consume_token(p, TOK_NEW_LINE);
        }
        else if(p->currentToken.type == TOK_IDENTIFIER){
            Token id = p->currentToken;
            parser_next_token(p);
            parser_expect_consume_token(p, TOK_COLON); 
            parser_next_token(p);
            symbol_table_add(id.literal, program.text.size, SECTION_TEXT, VISIBILITY_LOCAL);
        } else if (p->currentToken.type == TOK_INSTRUCTION) {
                Operand operands[4] = {0};
                int operand_count = 0;
                uint64_t instr = p->currentToken.instruction; 
                while(p->currentToken.type != TOK_NEW_LINE){
                    Token op = parser_next_token(p);
                    if(op.type == TOK_NEW_LINE) break;
                    operands[operand_count++] = parse_operand(p);

                    parser_next_token(p);

                    if(!match(p, TOK_COMMA, TOK_NEW_LINE)){
                        parser_fatal_error(p, "Expected comma or new line after operand got %s\n", token_to_string(p->currentToken.type));

                    }

                }

                if(operand_count == 2)match_operand_pairs(&operands[0], &operands[1]);
                else if(operand_count == 3) match_operand_triples(&operands[0], &operands[1], &operands[2]);
                else if (operand_count == 4){
                    if(operands[3].type == OPERAND_IMM64){
                        operands[3].type = OPERAND_IMM8;
                    }
                    match_operand_triples(&operands[0], &operands[1], &operands[2]);
                }

                Instruction* found_instruction = find_instruction(instr, operands);
                
                if(found_instruction == NULL){
                    for(int i = 0; i < operand_count; i++){
                        scratch_buffer_fmt("%s ", operand_to_string(operands[i].type));
                    }
                    char* temp = scratch_buffer_as_str();
                    parser_fatal_error(p,"Couldn't find instruction for nmemonic: %s %s", KEYWORD_TABLE[instr].name, temp); 
                } else{
                    emit_instruction(found_instruction, operands);
                }

                parser_expect_consume_token(p, TOK_NEW_LINE);

        } else{
            parser_fatal_error(p, "Invalid token found in text section\n");
        }
    }

}


static void parse_tokens(ArrayList* tokens){
    Parser p ={0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;
    array_list_create_cap(program.symTable.symbols, SymbolTableEntry, 16);
 
    while(p.tokenIndex < p.tokens->size){
        if(setjmp(p.jmp) == 1){
            //print_text_section();        
            break;
        }

        if(p.currentToken.type != TOK_SECTION){
            parser_next_token(&p);

            if(p.currentToken.type != TOK_SECTION){
                parser_fatal_error(&p, "Expected Section got %s\n", token_to_string(p.currentToken.type));     
            }
        }
        


        parser_next_token(&p); 

        switch (p.currentToken.type) {
            case TOK_TEXT:
                init_section(&program.text, 256);
                parser_next_token(&p);
                parser_expect_consume_token(&p, TOK_NEW_LINE);      
                parse_text_section(&p); 
                break;

            case TOK_BSS:
                parser_next_token(&p);
                parser_expect_consume_token(&p, TOK_NEW_LINE);      
                parse_bss_section(&p);
                break;

            case TOK_DATA:
                init_section(&program.data, 64);
                parser_next_token(&p);
                parser_expect_consume_token(&p, TOK_NEW_LINE);       
                parse_data_section(&p);
                break;

            default:
                parser_fatal_error(&p, "Expected Section Name got %s\n", token_to_string(p.currentToken.type));     
                        
        }
    }



}



bool basm_assemble_program(AssemblerFlags* flags){
     current_fb = file_buffer_create(flags->input_file);

     if(current_fb == NULL) return false;
   

     ArrayList tokens = tokenize_file(); 
     tokens = preprocess_tokens(&tokens);
    
     parse_tokens(&tokens);

     for(int i = 0; i < program.symTable.symbols.size; i++){
         SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
         if(e->section == SECTION_UNDEFINED && e->visibility == VISIBILITY_UNDEFINED){
             program_fatal_error("Symbol %s used but never defined\n", e->name);
         }

         for(int j = 0; j < e->instances.size; j++){
             SymbolInstance* instance =  &array_list_get(e->instances, SymbolInstance, j);

             //NOTE WE ONLY ALLOW USING SYMBOLS 
             //IN THE TEXT SECTION FOR NOW 
             if(instance->is_relative){
                 //want the linker to handle relocation
                 if(e->section == SECTION_EXTERN){
                     instance->is_relative = false;
                     continue;
                 } 
                 //assume size of 4   
                 uint64_t next_instruction = instance->offset;
                 uint64_t rip_addr = e->section_offset;
                 //not sure if this is supposed to be signed or unsigned
                 int32_t rel_addr = (int32_t)(rip_addr - next_instruction);
                 memcpy(&program.text.data[instance->offset - 4], &rel_addr, 4);
             }

         }
     }

     file_buffer_delete(current_fb);

     if(flags->ftype == BASM_FILE_ELF){
        return write_elf(flags->input_file, flags->output_file, &program);
     } else if(flags->ftype == BASM_FILE_PE){
        return write_pe(flags->input_file, flags->output_file, &program);
     } else{
        fprintf(stderr, "Unknown output file type\n");
        return false;
     }
}




bool basm_parse_flags(AssemblerFlags* flags, int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "./basm input_file\n");
        return false;
    }
    flags->output_file = "a.out";

    for(int i = 1; i < argc; i++){
        if(strcmp("-f", argv[i]) == 0){
            i++;
            if(i == argc){
                fprintf(stderr, "Invalid File Type\n");
                return false;
            }
            if(strcmp("win", argv[i]) == 0){
                flags->ftype = BASM_FILE_PE;
            } else if (strcmp("elf", argv[i]) == 0) { 
                flags->ftype = BASM_FILE_ELF;
            } else{
                fprintf(stderr, "Invalid File Type: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp("-o", argv[i]) == 0) {
            i++;
            if(i == argc){
                fprintf(stderr, "Output file not specified\n");
                return false;
            }
            flags->output_file = argv[i];
             
        } else if(string_cmp_lower("--help", argv[i]) == 0){
            basm_help();
            return false;
        }else{
            flags->input_file = argv[i];
        }
    }
    return true;
}

void basm_help(){
    printf("./basm input_file\n");
    printf("Flags: \n");
    printf("-f (file type)        -> win | elf\n");
    printf("-o (output file name) -> output file\n");
}
