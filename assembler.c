#include "nmemonics.h"
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
#include <sys/types.h>

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


#define LITTLE_ENDIAN_32(num)((num>>24)&0xff) | \
                    ((num<<8)&0xff0000) | \
                    ((num>>8)&0xff00) | \
                    ((num<<24)&0xff000000); 


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
/*
80 /0 ib 	ADD r/m8, imm8 	MI 	Valid 	Valid 	Add imm8 to r/m8.
REX + 80 /0 ib 	ADD r/m8*, imm8 	MI 	Valid 	N.E. 	Add sign-extended imm8 to r/m8.
81 /0 iw 	ADD r/m16, imm16 	MI 	Valid 	Valid 	Add imm16 to r/m16.
81 /0 id 	ADD r/m32, imm32 	MI 	Valid 	Valid 	Add imm32 to r/m32.
REX.W + 81 /0 id 	ADD r/m64, imm32 	MI 	Valid 	N.E. 	Add imm32 sign-extended to 64-bits to r/m64.
83 /0 ib 	ADD r/m16, imm8 	MI 	Valid 	Valid 	Add sign-extended imm8 to r/m16.
83 /0 ib 	ADD r/m32, imm8 	MI 	Valid 	Valid 	Add sign-extended imm8 to r/m32.
REX.W + 83 /0 ib 	ADD r/m64, imm8 	MI 	Valid 	N.E. 	Add sign-extended imm8 to r/m64.
00 /r 	ADD r/m8, r8 	MR 	Valid 	Valid 	Add r8 to r/m8.
REX + 00 /r 	ADD r/m8*, r8* 	MR 	Valid 	N.E. 	Add r8 to r/m8.
01 /r 	ADD r/m16, r16 	MR 	Valid 	Valid 	Add r16 to r/m16.
01 /r 	ADD r/m32, r32 	MR 	Valid 	Valid 	Add r32 to r/m32.
REX.W + 01 /r 	ADD r/m64, r64 	MR 	Valid 	N.E. 	Add r64 to r/m64.
02 /r 	ADD r8, r/m8 	RM 	Valid 	Valid 	Add r/m8 to r8.
REX + 02 /r 	ADD r8*, r/m8* 	RM 	Valid 	N.E. 	Add r/m8 to r8.
03 /r 	ADD r16, r/m16 	RM 	Valid 	Valid 	Add r/m16 to r16.
03 /r 	ADD r32, r/m32 	RM 	Valid 	Valid 	Add r/m32 to r32.
REX.W + 03 /r 	ADD r64, r/m64 	RM 	Valid 	N.E. 	Add r/m64 to r64.
*/








typedef enum{
    OP_REG32,
    OP_REG64,
    OP_MEM32,
    OP_MEM64,


    OP_MEM8 = 0,
    OP_IMM8,
    OP_IMM32,
    OP_IMM64,

    OP_SYM,
    OP_NONE,
} OperandType;




/*instructions 
mov
1260
89 /r             -> MOV r/m32, r32 
REX.W + 89 /r     -> MOV r/m64, r64 
8B /r             -> MOV r32, r/m32 RM 
REX.W + 8B /r     -> MOV r64, r/m64 
B8+ rd id         -> MOV r32, imm32 
REX.W + B8+ rd io -> MOV r64, imm64
C7 /0 id          -> MOV r/m32, imm32 
REX.W + C7 /0 id  -> MOV r/m64, imm32


syscall -> 0F 05
je ->  0F 84 cd
jne -> 0F 85 cd 
call -> E8 (addr relative to the next instruction)
ret -> C3


add
sub
lea
cmp
*/



typedef enum {
    TOK_INSTRUCTION,
    TOK_REG,
  
    TOK_COMMA,
    TOK_COLON,
    TOK_NEW_LINE,

    TOK_GLOBAL,
    TOK_SECTION,
    TOK_BSS,
    TOK_TEXT,
    TOK_DATA,
    TOK_RESB,

    TOK_INT,
    TOK_UINT,
    TOK_IDENTIFIER,

    TOK_MAX,
} TokenType;


/*
static inline bool is_instruction(TokenType type){
    return type > TOK_BEGIN_INSTRUCTION && type < TOK_END_INSTRUCTION;
}

static inline bool is_register(TokenType type){
    return type >= TOK_RAX && type <= TOK_R15;
}

static inline bool is_extended_reg(TokenType type){
    return type > TOK_RDI && type < TOK_R15;
}
*/

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



static inline bool is_extended_reg(RegisterType type){
    return type >= REG_R8 && type <= REG_R15 || \
        type >= REG_R8D && type <= REG_R15D || \
        type >= REG_R8W && type <= REG_R15W || \
        type >= REG_R8B;
}


typedef struct {
    TokenType type;
    union{
        RegisterType reg;
        char* literal;  
        uint64_t instruction;
    };
} Token;



const char* token_to_string(TokenType type) {
    switch (type) {
        case TOK_COMMA:return "TOK_COMMA";
        case TOK_COLON: return "TOK_COLON";
        case TOK_NEW_LINE: return "TOK_NEW_LINE";
        case TOK_INT: return "TOK_INT";
        case TOK_UINT: return "TOK_UINT";
        case TOK_IDENTIFIER: return "TOK_IDENTIFIER";
        case TOK_GLOBAL: return "TOK_GLOBAL";
        case TOK_RESB: return "TOK_RESB";
        case TOK_SECTION: return "TOK_SECTION";
        case TOK_MAX: return "TOK_MAX";
        case TOK_BSS: return "TOK_BSS";
        case TOK_TEXT: return "TOK_TEXT";
        case TOK_DATA: return "TOK_DATA";
        case TOK_INSTRUCTION: return "TOK_INSTRUCTION";
        case TOK_REG: return "TOK_REG";
        default: return "UNKNOWN_TOKEN";
    }
}


typedef struct{
   void* data;
   int size;
   int capacity;
} ArrayList;


#define array_list_create_cap(list, type, cap) \
do { \
    list.size = 0; \
    list.capacity = cap; \
    list.data = malloc(sizeof(type) * list.capacity); \
    memset(list.data, 0, sizeof(type) * list.capacity); \
} while(0) 


#define array_list_resize(list, type) \
do { \
    list.capacity *= 2; \
    list.data = realloc(list.data, sizeof(type) * list.capacity); \
} while(0) 

#define array_list_append(list, type, value) \
    do { \
        if(list.size == list.capacity){ \
            array_list_resize(list, type); \
        } \
        type* temp = (type*)list.data; \
        temp[list.size] = value; \
        list.size++; \
    } while(0)

#define array_list_get(list, type, index)((type*)list.data)[index]

#define array_list_delete(list) \
    do { \
        if(list != NULL) {  \
            if(list.capacity != 0) free(list.data); \
        } \
    } while(0)



typedef struct {
    char* data;
    uint32_t offset;
    uint32_t size;
} ScratchBuffer;



static ScratchBuffer scratch_buffer = {0};

#define SCRATCH_BUFFER_SIZE 8092

void init_scratch_buffer(){
    scratch_buffer.data = malloc(SCRATCH_BUFFER_SIZE);
    scratch_buffer.offset = 0;

    if(scratch_buffer.data == NULL){
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
}

static inline void scratch_buffer_full(){
    if(scratch_buffer.offset >= SCRATCH_BUFFER_SIZE){
       fprintf(stderr, "ScratchBuffer is full"); 
       exit(EXIT_FAILURE);
    }
}

void scratch_buffer_append_char(char c){
    scratch_buffer_full();
    *((char*)scratch_buffer.data + scratch_buffer.offset) = c;
    scratch_buffer.offset += sizeof(char);
}


void scratch_buffer_clear(){
    scratch_buffer.offset = 0;
} 


char* scratch_buffer_fmt(const char* fmt, ...){
    va_list list;
    va_start(list, fmt);
    uint32_t max_size = SCRATCH_BUFFER_SIZE - scratch_buffer.offset;
    vsnprintf(scratch_buffer.data + scratch_buffer.offset, max_size, fmt, list);
    va_end(list);
    return scratch_buffer.data;
}


char* scratch_buffer_vfmt(const char* fmt, va_list list){
    uint32_t max_size = SCRATCH_BUFFER_SIZE - scratch_buffer.offset;
    vsnprintf(scratch_buffer.data + scratch_buffer.offset, max_size, fmt, list);
    return scratch_buffer.data;
}


char* scratch_buffer_as_str(){ 
    if(scratch_buffer.offset == 0) return NULL;

    *((char*)scratch_buffer.data + scratch_buffer.offset) = '\0';
    return scratch_buffer.data;
}


static char peek_char(FILE* f){
    fpos_t pos;
    fgetpos(f, &pos);
    char c = fgetc(f);
    fsetpos(f, &pos);
    return c;

}


//determines if c is a valid start to an identifier
static inline bool is_id_alpha(char c){
    return isalpha(c) || c == '_' || c == '.';
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



static inline void get_literal(FILE* file){
    while(true){
        char next = peek_char(file);
        if(!isalnum(next)){
            break;
        }
        char c = fgetc(file);
        scratch_buffer_append_char(c); 
    }
}



/** 
 * Determine if the String is a keyword or identifier 
 */
static Token id_or_kw(FILE* file){
    get_literal(file);
    char* str = scratch_buffer_as_str();
    uint32_t str_size = scratch_buffer.offset;

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

    
    if(string_cmp_lower("section", str) == 0) return (Token){TOK_SECTION, 0};
    if(string_cmp_lower("resb", str) == 0) return (Token){TOK_RESB, 0};
    if(string_cmp_lower("global", str) == 0) return (Token){TOK_GLOBAL, 0};
    if(string_cmp_lower(".bss", str) == 0) return (Token){TOK_BSS, 0};
    if(string_cmp_lower(".data", str) == 0) return (Token){TOK_DATA, 0};
    if(string_cmp_lower(".text", str) == 0) return (Token){TOK_TEXT, 0};

   
    return (Token){TOK_IDENTIFIER, 0};
}


static ArrayList tokenize_file(FILE* file){
   init_scratch_buffer(); 

    ArrayList tokens;
    array_list_create_cap(tokens, Token, 256);

    while(!feof(file)){
        char c = fgetc(file);  
        while(isspace(c) && c != '\n'){
            c = fgetc(file);
        }
        if(feof(file)) break;

        Token token;
        token.type = TOK_MAX;
        token.literal = 0;

        switch (c) {
            case ':':
                token.type = TOK_COLON;
                break;
            case '\n':
                token.type = TOK_NEW_LINE;
                break;
            case ',':
                token.type = TOK_COMMA;
                break;
            default: {
                if (is_id_alpha(c)){
                    scratch_buffer_append_char(c);
                    token = id_or_kw(file);
                    if(token.type != TOK_IDENTIFIER) scratch_buffer_clear();

                }else if(isdigit(c)){
                    token.type = TOK_UINT;
                    scratch_buffer_append_char(c);
                    get_literal(file); 
                }else if(c == '-'){
                    scratch_buffer_append_char(c);
                    token.type = TOK_INT;
                    get_literal(file); 
                }
                else{
                    //unkown token
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
        scratch_buffer_clear();
    }

  
    /*
    for(int i = 0; i < tokens.size; i++){
        Token t = array_list_get(tokens, Token, i);
        if(t.type == TOK_IDENTIFIER || t.type == TOK_UINT || t.type == TOK_INT){
            printf("%s, %s\n", token_to_string(t.type), t.literal);
        } 
        if(t.type == TOK_REG){
            printf("%s, %d\n", token_to_string(t.type), t.reg);
        
        }
        else{
            printf("%s, %ld\n", token_to_string(t.type), t.instruction);
        }

    }
    */
   
    scratch_buffer_clear();
    return tokens;
}



typedef struct {
    char* name;
    TokenType section;
    uint64_t addr;
} SymbolTableEntry;


//TODO: IMPLEMENT AN ACTUAL HASHMAP 
typedef struct {
    ArrayList symbols;
} SymbolTable;


typedef struct {
    char* name;
    uint64_t location;
    uint64_t size;
} SymbolResolutionEntry;


typedef struct {
    uint8_t* data;
    uint64_t capacity;
    uint64_t size;
} Section;


typedef struct {
    SymbolTable symTable; //holds all the locations of the symbols 


    Section data;
    Section text;
    Section bss;
} Program;


static Program program = {0};


typedef struct {
    ArrayList* tokens; 
    Token currentToken; 
    uint32_t tokenIndex;
    jmp_buf jmp;
    //File* file;
} Parser;



noreturn void progam_fatal_error(const char* fmt, ...){
    fprintf(stderr,"Error: ");
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    exit(EXIT_FAILURE);
}



void symbol_table_add(char* name, uint64_t offset, TokenType section){
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(program.symTable.symbols, SymbolTableEntry, i);
        if(strcmp(e.name, name) == 0){
           progam_fatal_error("Many definitions of symbol: %s\n", name); 
        }
    }
    SymbolTableEntry e;
    e.name = name;
    e.addr = offset;
    e.section = section;
    array_list_append(program.symTable.symbols, SymbolTableEntry, e);
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



static inline uint64_t string_to_uint(char* string){
    int base = 10;

    //check for hexadecimal
    char* temp = strcasestr(string, "0x");
    if(temp != NULL && temp == string){
        base = 16;
    }
    return strtoll(string, NULL, base);
}



static void parse_bss_section(Parser* p){
    program.bss.data = NULL;
    program.bss.capacity = 0;

    while(p->currentToken.type != TOK_SECTION){
        parser_consume_newlines(p);

        parser_expect_token(p, TOK_IDENTIFIER); 
        Token id = p->currentToken;
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_COLON); 

        parser_consume_newlines(p);

        symbol_table_add(id.literal, program.bss.size, TOK_BSS);

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


static void section_realloc(Section* section){
   //TODO: CHECK FOR OVERFLOW
   uint64_t new_capacity = section->capacity * 2;
   section->data = realloc(section->data, new_capacity);
   if(section->data == NULL){
        progam_fatal_error("Out of memory\n");
   }
   section->capacity = new_capacity;
}


static inline void check_section_size(Section* section, uint64_t bytes_to_add){
    if(section->size + bytes_to_add >= section->capacity){
        section_realloc(section);
    }
}



typedef struct {
    OperandType type;
    uint8_t rex;
    union {
        uint8_t reg;
        uint8_t imm8;
        uint32_t imm32;
        uint64_t imm64;
        uint32_t mem32;
        uint64_t mem64;
    };
} Operand;


typedef struct {
    Operand op1;
    Operand op2;
} OperandPair;



static Operand parse_operand(Parser* p){
    Token op = parser_next_token(p);
    uint8_t w = 0,r = 0,x = 0,b = 0;
    Operand result = {0};

    //assume 64 bit reg size
    if(op.type == TOK_REG){
        result.type = OP_REG64;
        result.reg = op.type;
        w = 1;
       if(is_extended_reg(op.reg)){
            b = 1;
            result.reg -= op.reg;
            while(result.reg > 7) result.reg--;
        }
        result.rex = REX_PREFIX(w, r, x, b);
        return result;
    } else if(op.type == TOK_UINT){
        uint64_t imm = string_to_uint(op.literal);
        Operand result;

        /*
        if(imm <= UINT8_MAX){
            result.type = OP_IMM8;
            result.imm8 = (uint8_t)imm;
        }
        */
        if(imm <= UINT32_MAX){
            result.type = OP_IMM32;
            result.imm32 = (uint32_t)imm;
        } else{
            result.type = OP_IMM64;
            result.imm64 = (uint64_t)imm;
            w = 1;
        }
        result.rex = REX_PREFIX(w, r, x, b);
        return result;
    } else{
        progam_fatal_error("Operand Type not supported yet: %s\n",token_to_string(op.type));
    }
}



static OperandPair parse_operands(Parser* p){
    TokenType instruction = p->currentToken.type;
    Operand op1 = parse_operand(p);

    // I don't know of any instruction (with two operands) that allows an 
    // immediate as the first operand
    if(op1.type > OP_MEM64){
        progam_fatal_error("Invalid Operand for instruction: %s\n", token_to_string(instruction));
    }

    parser_next_token(p);
    parser_expect_token(p, TOK_COMMA);


    Operand op2 = parse_operand(p);
   

    if(!REX_CHECK_OP_SIZE(op2.rex)){
        op1.rex = REX_CLEAR_OP_SIZE(op1.rex); 
    }

    parser_next_token(p);
    parser_expect_consume_token(p, TOK_NEW_LINE);

    return (OperandPair){op1, op2};
}



/*
 *
REX.W + C7 /0 id  -> MOV r/m64, imm32
REX.W + 89 /r     -> MOV r/m64, r64 
REX.W + 8B /r     -> MOV r64, r/m64 
REX.W + B8+ rd io -> MOV r64, imm64

8B /r             -> MOV r32, r/m32 RM 
B8+ rd id         -> MOV r32, imm32 
89 /r             -> MOV r/m32, r32 
C7 /0 id          -> MOV r/m32, imm32 
*/

    
static void move(OperandPair *pair){
    if(REX_CHECK_OP_SIZE(pair->op1.rex)){


    } else{
        if(pair->op1.rex != 64){
            if(pair->op1.type == OP_REG32 || pair->op1.type == OP_REG64 && pair->op2.type == OP_IMM32){
                uint8_t rex = pair->op1.rex;
                uint8_t byte_one = 0xB8 + pair->op1.reg;
                uint32_t imm = LITTLE_ENDIAN_32(pair->op2.imm32);
                printf("0x%x 0x%x 0x%x\n", rex, byte_one, imm);
            }
            
        } else{
            if(pair->op1.type == OP_REG32 || pair->op1.type == OP_REG64 && pair->op2.type == OP_IMM32){
                uint8_t byte_one = 0xB8 + pair->op1.reg;
                uint32_t imm = LITTLE_ENDIAN_32(pair->op2.imm32);
                printf("0x%x 0x%x\n", byte_one, imm);
            } 
        }
    }
}


static void parse_text_section(Parser* p){
    program.text.size = 0;
    program.text.capacity = 256;
    program.text.data = malloc(program.text.capacity);
    if(program.text.data == NULL) progam_fatal_error("Out of memory\n");

    while(p->currentToken.type != TOK_SECTION){
        parser_consume_newlines(p);

        //functions and labels
        if(p->currentToken.type == TOK_IDENTIFIER){
            Token id = p->currentToken;
            parser_next_token(p);
            parser_expect_consume_token(p, TOK_COLON); 
            parser_next_token(p);
            symbol_table_add(id.literal, program.text.size, TOK_TEXT);
        } else if (p->currentToken.type == TOK_INSTRUCTION) {
            switch (p->currentToken.instruction) {
                case 198: {
                        OperandPair pair = parse_operands(p);
                        move(&pair);
                         

                    }
                    break;

                case 317: 
                    printf("0x0f 0x05\n");
                    parser_next_token(p);
                    parser_expect_consume_token(p, TOK_NEW_LINE);
                    break;
                case 272:
                    printf("0xc3\n");
                    parser_next_token(p);
                    parser_expect_consume_token(p, TOK_NEW_LINE);
                    break;


                default:
                    if(p->currentToken.type == TOK_INSTRUCTION){
                        progam_fatal_error("Instruction %s not implemented\n", NMEMONIC_TABLE[p->currentToken.instruction]);
                    } else{
                        progam_fatal_error("%s not implemented\n", token_to_string(p->currentToken.type));
                    } 
            }    

        }

    }
            
}


static void parse_tokens(ArrayList* tokens){
    Parser p ={0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;
 
    while(p.tokenIndex < p.tokens->size){
        if(setjmp(p.jmp) == 1){
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

                break;

            default:
                parser_fatal_error(&p, "Expected Section Name got %s\n", token_to_string(p.currentToken.type));     
                        
        }
    }



}



void assemble_program(MachineType arch, FILE* f){
   ArrayList tokens = tokenize_file(f); 

   parse_tokens(&tokens);
    


}

