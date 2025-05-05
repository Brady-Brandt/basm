#include "x86/nmemonics.h"
#include "util.h"
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <stdint.h>
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

#define REX_MODRM_REG 4
#define REX_SIB_INDEX 2
#define REX_B 1


/*
add BYTE PTR [r12+r13*1],0x1
43 80 04 2c 01

0x43 := REX = 0100 | 0011
Lower two bits are both one because we are using the extended registers r12 & r13
0x80 := Opcode Instruction

mod byte
00 | 000 | 100
0x04 := Other part of opcode instruction goes in reg portion in this case its 0
lower 3 bits 100 indicate a SIB follows

0x2c SIB byte 
00101100
00 -> Scale => 1
101 -> Index => r13 
100 -> Base => r12

addr = scale * index + base
*/




//call -> E8 (addr relative to the next instruction)


typedef enum {
    TOK_INSTRUCTION,
    TOK_REG,
  
    TOK_OPENING_BRACKET,
    TOK_CLOSING_BRACKET,
    TOK_COMMA,
    TOK_COLON,
    TOK_NEW_LINE,

    TOK_GLOBAL,
    TOK_SECTION,
    TOK_TEXT,
    TOK_DATA,
    TOK_BSS,
    TOK_RESB,
    TOK_DB,

    TOK_INT,
    TOK_UINT,
    TOK_IDENTIFIER,
    TOK_STRING,

    TOK_MAX,
} TokenType;


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


/*
Need to use REX prefix to use these instructions 
the LSB needs to be 1
0 R8B R8W R8D R8
1 R9B R9W R9D R9
2 R10B R10W R10D R10
3 R11B R11W R11D R11
4 R12B R12W R12D R12
5 R13B R13W R13D R13
6 R14B R14W R14D R14
7 R15B R15W R15D R15
*/

typedef enum {
    REG_RAX = 0,
    REG_RCX,
    REG_RDX,
    REG_RBX,
    REG_RSP,
    REG_RBP,
    REG_RSI,
    REG_RDI,

    REG_R8 = 8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,

    REG_EAX = 16,
    REG_ECX,
    REG_EDX,
    REG_EBX,
    REG_ESP,
    REG_EBP,
    REG_ESI,
    REG_EDI,


    REG_R8D = 24,
    REG_R9D,
    REG_R10D,
    REG_R11D,
    REG_R12D,
    REG_R13D,
    REG_R14D,
    REG_R15D,

    REG_AX = 32,
    REG_CX,
    REG_DX,
    REG_BX,
    REG_SP,
    REG_BP,
    REG_SI,
    REG_DI,

    REG_R8W = 40,
    REG_R9W,
    REG_R10W,
    REG_R11W,
    REG_R12W,
    REG_R13W,
    REG_R14W,
    REG_R15W,


    REG_AL = 48,
    REG_CL,
    REG_DL,
    REG_BL,
    REG_AH,
    REG_CH,
    REG_DH,
    REG_BH,

    REG_R8B = 56,
    REG_R9B,
    REG_R10B,
    REG_R11B,
    REG_R12B,
    REG_R13B,
    REG_R14B,
    REG_R15B,
    REG_MAX,
} RegisterType;



#define is_extended_reg(type) (type >= REG_R8 && type <= REG_R15)


#define is_r64(reg) (reg >= REG_RAX && reg <= REG_R15)
#define is_r32(reg) (reg >= REG_EAX && reg <= REG_R15D)
#define is_r16(reg) (reg >= REG_AX && reg <= REG_R15W)
#define is_r8(reg) (reg >= REG_AL && reg <= REG_R15B)


#define is_label(l) (l >= OPERAND_L8 && l <= OPERAND_L64)
#define is_general_reg(r) (r >= OPERAND_R8 && r <= OPERAND_R64)


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



const char* token_to_string(TokenType type) {
    switch (type) {
        case TOK_COMMA:return "TOK_COMMA";
        case TOK_COLON: return "TOK_COLON";
        case TOK_NEW_LINE: return "TOK_NEW_LINE";
        case TOK_INT: return "TOK_INT";
        case TOK_UINT: return "TOK_UINT";
        case TOK_STRING: return "TOK_STRING";
        case TOK_IDENTIFIER: return "TOK_IDENTIFIER";
        case TOK_GLOBAL: return "TOK_GLOBAL";
        case TOK_RESB: return "TOK_RESB";
        case TOK_DB: return "TOK_DB";
        case TOK_SECTION: return "TOK_SECTION";
        case TOK_BSS: return "TOK_BSS";
        case TOK_TEXT: return "TOK_TEXT";
        case TOK_DATA: return "TOK_DATA";
        case TOK_INSTRUCTION: return "TOK_INSTRUCTION";
        case TOK_REG: return "TOK_REG";
        case TOK_MAX: return "TOK_UNKNOWN";
        case TOK_OPENING_BRACKET: return "TOK_OPENING_BRACKET";
        case TOK_CLOSING_BRACKET: return "TOK_CLOSING_BRACKET";
    }
}


static char peek_char(FILE* f){
    fpos_t pos;
    fgetpos(f, &pos);
    char c = fgetc(f);
    fsetpos(f, &pos);
    return c;

}



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


static int bsearch_string_cmp_lower(const void* a, const void* b) {
    const char* s1 = *(const char**)a;
    const char* s2 = *(const char**)b; // assuming bsearch is being used on an array of `char*`
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



static inline void get_literal(FILE* file, int* col){
    while(true){
        char next = peek_char(file);
        if(!isalnum(next)){
            break;
        }
        char c = fgetc(file);
        scratch_buffer_append_char(c); 
        (*col)++;
    }
}




/** 
 * Determine if the String is a keyword or identifier 
 */
static Token id_or_kw(FILE* file, int* col){
    get_literal(file, col);
    char* str = scratch_buffer_as_str();
    //TODO FIX THIS
    uint32_t str_size = 0;

    if(str_size < 5){
        for(int i = 0; i < REG_MAX; i++){
            if(string_cmp_lower(REGISTER_TABLE[i], str) == 0){
                Token result;
                result.type = TOK_REG;
                result.reg = i;
                return result;
            }
        }
    }

    char** instruction = (char**)bsearch(&str, NMEMONIC_TABLE, NMEMONIC_TABLE_SIZE,sizeof(char*), bsearch_string_cmp_lower);

    if(instruction != NULL){
        Token result;
        result.type = TOK_INSTRUCTION;
        //compute the index into the nmemonic table
        result.instruction = ((void*)instruction - (void*)NMEMONIC_TABLE) / sizeof(char*);
        return result;
    }

    
    if(string_cmp_lower("section", str) == 0) return (Token){TOK_SECTION, 0,0,0};
    if(string_cmp_lower("resb", str) == 0) return (Token){TOK_RESB, 0,0,0};
    if(string_cmp_lower("global", str) == 0) return (Token){TOK_GLOBAL, 0,0,0};
    if(string_cmp_lower(".bss", str) == 0) return (Token){TOK_BSS, 0,0,0};
    if(string_cmp_lower(".data", str) == 0) return (Token){TOK_DATA, 0,0,0};
    if(string_cmp_lower(".text", str) == 0) return (Token){TOK_TEXT, 0,0,0};
    if(string_cmp_lower("db", str) == 0) return (Token){TOK_DB, 0,0,0};

   
    return (Token){TOK_IDENTIFIER, 0,0, 0};
}


static char* file_get_line(FILE* file, int line){

    //save file pos in case we aren't done tokenizing
    fpos_t pos;
    fgetpos(file, &pos);

    rewind(file);

    char c;

    int current_line = 1;
    while(current_line != line){
        c = fgetc(file);
        if(c == '\n') current_line++;
    }

    scratch_buffer_clear();
    c = 0;
    while(true){
        c = fgetc(file);
        if(feof(file) || c == '\n' || c == ';') break; 
        scratch_buffer_append_char(c);
    }
    fsetpos(file, &pos);
    return scratch_buffer_as_str();
}



static void get_string(FILE* file, uint32_t line, int col){
    while(true){
        char next = peek_char(file);
        if(next == '\"'){
            fgetc(file);
            scratch_buffer_append_char(0);
            break;
        }

        if(next == '\n'){
            fprintf(stderr, "Error: String doesn't close\nLine %d, Col %d\n", line, col);
            fprintf(stderr, "%s\n", file_get_line(file, line));
            fprintf(stderr,"%*s\n", col, "^");
            exit(EXIT_FAILURE);
        }
        char c = fgetc(file);
        scratch_buffer_append_char(c); 
    }
}




static ArrayList tokenize_file(FILE* file){
    init_scratch_buffer(); 

    ArrayList tokens;
    array_list_create_cap(tokens, Token, 256);

    int line_number = 1;
    int col = 1;

    while(!feof(file)){
        char c = fgetc(file);  
         
        while(isspace(c) && c != '\n'){
            col++;
            c = fgetc(file);
        }


        if(feof(file)) break;

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
                token.type = TOK_NEW_LINE;
                break;
            case ',':
                col++;
                token.type = TOK_COMMA;
                break;
            case '\"':
                get_string(file,line_number, col);
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
            case ';':
                while(true){
                    c = fgetc(file);
                    if(c == '\n' || feof(file)){
                        //if there are other tokens on the line 
                        // we want to put a new line
                        if(tokens.size > 1){
                            Token last_token = array_list_get(tokens, Token, tokens.size - 1);
                            if(last_token.line_number == line_number){
                                token.type = TOK_NEW_LINE;
                                array_list_append(tokens, Token, token);
                            }
                        }
                        col = 0;
                        line_number++;
                        goto comment;
                    } 
                } 
                break;
            default: {
                if (isalpha(c) || c == '_' || c == '.'){
                    int temp = col;
                    col++;
                    scratch_buffer_append_char(c);
                    token = id_or_kw(file, &col);
                    token.line_number = line_number;
                    token.col = temp;
                    if(token.type != TOK_IDENTIFIER) scratch_buffer_clear();

                }else if(isdigit(c)){
                    token.type = TOK_UINT;
                    scratch_buffer_append_char(c);
                    get_literal(file, &col); 
                }else if(c == '-'){
                    scratch_buffer_append_char(c);
                    token.type = TOK_INT;
                    get_literal(file, &col); 
                }
                else{
                    //unkown token
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

        array_list_append(tokens, Token, token);
        comment:
        scratch_buffer_clear();
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
    FILE* file;
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
        if(strcmp(e->name, name) == 0){
            //if we come across a label after declaring it global
            if(e->visibility == VISIBILITY_GLOBAL && visibility == VISIBILITY_LOCAL){
                e->section_offset = offset; 
                return;
            } else if(e->visibility == VISIBILITY_LOCAL && visibility == VISIBILITY_GLOBAL){
                e->visibility = VISIBILITY_GLOBAL;
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
void symbol_table_add_instance(char* symbol_name){
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
        

        if(e->instances.data == NULL){
            array_list_create_cap(e->instances, SymbolInstance, 2);
            SymbolInstance current_instance = {0};
            array_list_append(e->instances, SymbolInstance, current_instance);
            return;
        }
    }
    program_fatal_error("Symbol: %s not defined\n", symbol_name);
}


typedef enum {
    MACHINE_X86_64 = 62,
    MACHINE_ARM = 40,
} MachineType;


noreturn void parser_fatal_error(Parser *p, const char* fmt, ...){
    fprintf(stderr,"Error: ");
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    char* line = file_get_line(p->file, p->currentToken.line_number);
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



static Token parser_peek_token(Parser *p){
    if(p->tokenIndex + 1 < p->tokens->size){
        return array_list_get((*p->tokens), Token, p->tokenIndex + 1);
    }
    longjmp(p->jmp, 1);
}


//consumes new line tokens, consumes the next token that is not a new line
static Token parser_consume_newlines(Parser* p){
    Token res = p->currentToken;
    while(p->currentToken.type == TOK_NEW_LINE){
       res = parser_next_token(p); 
    }
    return res;
}


static void parser_expect_token(Parser *p, TokenType expected){
    if(p->currentToken.type != expected){
        parser_fatal_error(p, "Expected %s found %s\n", token_to_string(expected), token_to_string(p->currentToken.type));
    }
}

static inline void parser_expect_consume_token(Parser *p, TokenType expected){
    parser_expect_token(p, expected);
    parser_next_token(p);

}



bool __match(Parser *p, ...){
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




static inline uint64_t string_to_uint(char* string){
    int base = 10;

    //check for hexadecimal
    char* temp = strcasestr(string, "0x");
    if(temp != NULL && temp == string){
        base = 16;
    }
    return strtoull(string, NULL, base);
}




//TODO: ALLOW PSUEDOINSTRUCTIONS WITHOUT LABELS 
static void parse_bss_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){
        parser_consume_newlines(p);
        if(p->currentToken.type == TOK_SECTION) break;

        parser_expect_token(p, TOK_IDENTIFIER); 
        Token id = p->currentToken;
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_COLON); 

        parser_consume_newlines(p);

        symbol_table_add(id.literal, program.bss.size, SECTION_BSS, VISIBILITY_LOCAL);

        switch (p->currentToken.type) {
            case TOK_RESB:
                parser_next_token(p); 
                parser_expect_token(p, TOK_UINT);
                program.bss.size += string_to_uint(p->currentToken.literal); 
                parser_next_token(p);
                parser_expect_consume_token(p, TOK_NEW_LINE);
                break;
            default:
                parser_fatal_error(p, "Invalid bss section instruction\n");


        }
    } 
}




static void parse_data_section(Parser* p){ 
    while(p->currentToken.type != TOK_SECTION){
        parser_consume_newlines(p);
        if(p->currentToken.type == TOK_SECTION) break;

        parser_expect_token(p, TOK_IDENTIFIER); 
        Token id = p->currentToken;
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_COLON); 

        parser_consume_newlines(p);

        symbol_table_add(id.literal, program.data.size, SECTION_DATA, VISIBILITY_LOCAL);

        switch (p->currentToken.type) {
            case TOK_DB:
                parser_next_token(p); 

                if(p->currentToken.type == TOK_UINT){
                    uint8_t num = string_to_uint(p->currentToken.literal);
                    section_add_data(&program.data,&num, 1);
                } else if(p->currentToken.type == TOK_STRING){
                    section_add_data(&program.data, p->currentToken.literal, strlen(p->currentToken.literal) + 1);
                } else{
                    parser_fatal_error(p, "Invalid for operand\n");
                }

                parser_next_token(p);
                parser_expect_consume_token(p, TOK_NEW_LINE);
                break;
            default:
                parser_fatal_error(p, "Invalid data section instruction\n");


        }
    }
}


typedef struct {
    OperandType type;
    union {
        struct {
            uint8_t registerIndex;
            uint8_t rex;
        } reg;

        uint8_t imm8;
        uint16_t imm16;
        uint32_t imm32;
        uint64_t imm64;

        char* label;

        uint32_t mem32;
        uint64_t mem64;
    };
} Operand;







static Operand parse_operand(Parser* p){
    Operand result = {0};

    switch (p->currentToken.type) {
        case TOK_REG: {
            uint8_t w = 0;

            if(is_r64(p->currentToken.reg)){
                w = 1;
                result.type = OPERAND_R64;
                result.reg.registerIndex = p->currentToken.reg;
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

        case TOK_UINT:
            result.imm64 = string_to_uint(p->currentToken.literal);
            result.type = OPERAND_IMM64;
            return result;

        case TOK_IDENTIFIER: 
            result.type = OPERAND_L64;
            result.label = p->currentToken.literal; 
            return result;

        case TOK_OPENING_BRACKET: {
            parser_next_token(p);
            //TODO IMPLEMENT THIS FOR REGISTERS AND OFFSETS 
            parser_expect_consume_token(p, TOK_IDENTIFIER);
            program_fatal_error("TODO: IMPLEMENT BRACKETS");

        }
        
        default:
            program_fatal_error("Operand Type not supported yet: %s\n",token_to_string(p->currentToken.type));

    
    }
}



//converts an instruction of operand type register or memory to operand type RM 
#define TO_RM(input_instr, reg8_or_m8) (input_instr + (OPERAND_RM8 - reg8_or_m8))

static bool check_operand_type(OperandType table_instr, OperandType input_instr){

    //convert the label into OPERAND_M
    if(is_label(input_instr)){
        input_instr -= (OPERAND_L8 - OPERAND_M8);
    }

    if(table_instr == input_instr) return true;

    //these instructions either take a memory location or registers
    if(table_instr >= OPERAND_RM8 && table_instr <= OPERAND_RM64){

        if(table_instr == TO_RM(input_instr, OPERAND_R8) || table_instr == TO_RM(input_instr, OPERAND_M8)){
            return true;
        }
    }

    return false;
}



static Instruction* find_instruction(uint64_t instr, Operand operand[3]){
    uint64_t op_table_index = INSTRUCTION_TABLE_LOOK_UP[instr];    
    Instruction instruct = INSTRUCTION_TABLE[op_table_index];

    while(instr == instruct.instr){

        bool op1_bool = check_operand_type(instruct.op1, operand[0].type);
        bool op2_bool = check_operand_type(instruct.op2, operand[1].type); 
        bool op3_bool = check_operand_type(instruct.op3, operand[2].type);

        if(op1_bool && op2_bool && op3_bool){
            return (Instruction*)&INSTRUCTION_TABLE[op_table_index];
        }

        if(op_table_index == INSTRUCTION_TABLE_SIZE - 1) break;
        instruct = INSTRUCTION_TABLE[++op_table_index];
    }

    return NULL;
}



static void emit_instruction(Instruction* instruction, Operand operand[3]){
    uint8_t rex = instruction->rex;

    uint8_t opcode[4] = {0};
    memcpy(opcode, instruction->bytes, instruction->size);

    uint8_t mod_field = 0;
    bool uses_mod_field = false;


    //indicate opcode extension in the reg portion of modrm
    if(instruction->digit != -1){
       mod_field |= instruction->digit; 
       uses_mod_field = true;
    }

    // Instruction takes no operands
    if(operand[0].type == OPERAND_NOP){
        section_add_data(&program.text, opcode, instruction->size); 
        return;
    }

    if(is_general_reg(operand[0].type)){
        rex |= operand[0].reg.rex;

        //indicates we add register to the opcode
        if((instruction->r & 0x2)){
            opcode[instruction->size - 1] += operand[0].reg.registerIndex; 
        } else if ((instruction->r & 0x1)) { 
            if(is_general_reg(operand[1].type)){
                uses_mod_field = true;
                mod_field = MODRM_TABLE[(operand[0].reg.registerIndex * 8 + 0xC0) + operand[1].reg.registerIndex];
            } else{
                program_fatal_error("MODRM TABLE NOT IMPLEMENTED YET\n");
            }
        }

    }


    if(rex != 0x40) section_add_data(&program.text, &rex, 1);
    section_add_data(&program.text, opcode, instruction->size); 
    if(uses_mod_field) section_add_data(&program.text, &mod_field, 1);


    //indicates an immediate
    //for now immediates will only be in operand 2
    switch (instruction->ib) {
        //no immediate
        case -1:
            break;
        case 1: 
            section_add_data(&program.text, &operand[1].imm8, 1);
            break;
        case 2: {
            section_add_data(&program.text, &operand[1].imm16, 2);
            break;
        }
        case 4: {
            section_add_data(&program.text, &operand[1].imm32, 4);
            break;
        }

        case 8: {
            section_add_data(&program.text, &operand[1].imm64, 8);
            break;
        }
        case 10:
            program_fatal_error("10 byte immediate offsets not supported\n");
            break;
        default:
            program_fatal_error("Unreachable\n");
        
    }
        
}



static void match_operand_pairs(Operand* op1, Operand *op2){
    uint16_t operand_override_prefix = 0x66;

    //if we have extended registers r8-r15
    //convert them to their respected index  and set the REX prefix accordingly
    if(is_general_reg(op1->type) && is_extended_reg(op1->reg.registerIndex)){
        op1->reg.rex |= REX_B;
        op1->reg.registerIndex -= REG_R8;
    }
    if(is_general_reg(op2->type) && is_extended_reg(op2->reg.registerIndex)){
        op1->reg.rex |= REX_MODRM_REG;
        op2->reg.registerIndex -= REG_R8;
    }
    
    switch (op1->type) {
        case OPERAND_R64:
            if(op2->type == OPERAND_IMM64){
                //default to 32 bit values if we can
                if(op2->type == OPERAND_IMM64 && op2->imm64 <= UINT32_MAX){
                    op1->type = OPERAND_R32; 
                    op1->reg.rex = REX_CLEAR_OP_SIZE(op1->reg.rex);
                    op2->type = OPERAND_IMM32;
                    op2->imm32 = (uint32_t)op2->imm64;
                }
            } else if (op2->type >= OPERAND_R8 && op2->type < OPERAND_R64) {
                //promote op2 to a 64 bit register 
                op2->type = OPERAND_R64;
            }
            break;
        case OPERAND_R32:
            if(op2->type == OPERAND_IMM64){
                if(!(op2->imm64 <= UINT32_MAX)) program_fatal_error("Can't put 64 bit operand in 32 bit register\n");
                else{
                    op2->type = OPERAND_IMM32;
                    op2->imm32 = (uint32_t)op2->imm64;
                }
            } else if(op2->type >= OPERAND_R8 && op2->type < OPERAND_R32){ 
                //promote op2 to a 32 bit register 
                op2->type = OPERAND_R32;
            }
            break;

        case OPERAND_R16:
            if(op2->type == OPERAND_IMM64){
                if(!(op2->imm64 <= UINT16_MAX)) program_fatal_error("Value larger than 16 bits going in 16 bit register");
                else{
                    section_add_data(&program.text,&operand_override_prefix, 1);
                    op2->type = OPERAND_IMM16;
                    op2->imm16 = (uint16_t)op2->imm64;
                }
            } else if(op2->type == OPERAND_R8){
                //promote op2 to a 16 bit register 
                op2->type = OPERAND_R16;

                section_add_data(&program.text,&operand_override_prefix, 1);
            }
            break;

        case OPERAND_R8:
            if(op2->type == OPERAND_IMM64){
                if(!(op2->imm64 <= UINT8_MAX)) program_fatal_error("Value larger than 8 bits going in 8 bit register");
                else{
                    op2->type = OPERAND_IMM8;
                    op2->imm8 = (uint8_t)op2->imm64;
                }
            }
            break;

        
        default:
            program_fatal_error("Operand not supported yet: %s\n", operand_to_string(op1->type));
 
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
        parser_consume_newlines(p);
        if(p->currentToken.type == TOK_SECTION) break;

        if(p->currentToken.type == TOK_GLOBAL){
            parser_next_token(p);
            parser_expect_token(p, TOK_IDENTIFIER);

            Token id = p->currentToken;
            //TODO: ALLOW MANY GLOBAL DECLARATIONS AT ONCE
            symbol_table_add(id.literal, 0, SECTION_TEXT, VISIBILITY_GLOBAL);
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
                Operand operands[3] = {0};
                int operand_count = 0;
                uint64_t instr = p->currentToken.instruction; 
                while(p->currentToken.type != TOK_NEW_LINE){
                    Token op = parser_next_token(p);
                    if(op.type == TOK_NEW_LINE) break;
                    operands[operand_count++] = parse_operand(p);

                    //printf("%s, %d\n", token_to_string(p->currentToken.type), p->tokenIndex);
                    parser_next_token(p);
                    //printf("%s, %d\n", token_to_string(p->currentToken.type), p->tokenIndex);
                    if(!match(p, TOK_COMMA, TOK_NEW_LINE)){
                        parser_fatal_error(p, "Expected comma or new line after operand got %s\n", token_to_string(p->currentToken.type));

                    }

                }

                if(operand_count == 2)match_operand_pairs(&operands[0], &operands[1]);

                Instruction* found_instruction = find_instruction(instr, operands);
                
                if(found_instruction == NULL){
                    for(int i = 0; i < operand_count; i++){
                        scratch_buffer_fmt("%s ", operand_to_string(operands[i].type));
                    }
                    char* temp = scratch_buffer_as_str();
                    parser_fatal_error(p,"Couldn't find instruction for nmemonic: %s %s", NMEMONIC_TABLE[instr], temp); 
                } else{
                    emit_instruction(found_instruction, operands);
                }

        } else{
            parser_fatal_error(p, "Invalid token found in text section\n");
        }
    }

}


static void parse_tokens(FILE* file, ArrayList* tokens){
    Parser p ={0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;
    p.file = file;
    array_list_create_cap(program.symTable.symbols, SymbolTableEntry, 16);
 
    while(p.tokenIndex < p.tokens->size){
        if(setjmp(p.jmp) == 1){
            print_text_section();        
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



void assemble_program(MachineType arch, const char* file){
    FILE* f = fopen(file, "r");

    if(f == NULL){
        printf("Failed to open file: %s\n", file);
        exit(EXIT_FAILURE);
    }

   ArrayList tokens = tokenize_file(f); 

   parse_tokens(f, &tokens);

   write_elf(file, "btest.o", &program);
    
}

