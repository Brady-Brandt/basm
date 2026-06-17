#include "x86/types.h"
#include "parser.h"
#include "objectgen.h"
#include "eval.h"
#include "util.h"
#include "entry.h"
#include "preprocess.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>
#include <ctype.h>




#define REX_PREFIX(w,r,x,b) ((4 << 4) | (w * 8) | (r * 4) | (x * 2) | b)
#define REX_NIBBLE 0x40
#define REX_W 8    // 0 = Operand size determined by CS.D 1 = 64 Bit Operand Size
#define REX_R 4   // Extension of the ModR/M reg field
#define REX_X 2  // Extension of the SIB index field
#define REX_B 1 // Extension of the ModR/M r/m field, SIB base field, or Opcode reg field



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

#define is_ah_to_bh(op) (!op.isExtendedRegister && (op.registerIndex >= (REG_AH - REG_AL) \
                        && op.registerIndex <= (REG_BH - REG_AL)))
#define is_advanced_reg(r) (r >= OPERAND_MM && r <= OPERAND_YMM)
#define is_reg32_or_64(type) (type == OPERAND_R32 || type == OPERAND_R64)
#define is_immediate(i) (i >= OPERAND_IMM8 && i <= OPERAND_IMM64)
#define is_mem(m) (m >= OPERAND_M8 && m <= OPERAND_M80 || m == OPERAND_MEM_ANY)


#define is_int32(n) ((int64_t)n <= INT32_MAX && (int64_t)n >= INT32_MIN)
#define is_int16(n) ((int64_t)n <= INT16_MAX && (int64_t)n >= INT16_MIN)
#define is_int8(n) ((int64_t)n <= INT8_MAX && (int64_t)n >= INT8_MIN)


FileBuffer* current_fb = NULL;

Program program = {0};


void symbol_table_add(char* name, int l, int c, uint64_t offset, uint8_t section, uint8_t visibility){
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
                e->line = l;
                e->col = c;
                return;
            } 
           error_loc(l, c, "Many definitions of symbol: %s\n", name); 
           error_loc(e->line, e->col, "%s Previosuly Defined here\n", name);
        }
    }
    SymbolTableEntry e = {0};
    e.name = name;
    e.section_offset = offset;
    e.section = section;
    e.visibility = visibility;
    e.line = l;
    e.col = c;

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

void symbol_table_add_instance(char* symbol_name, int line, int col, uint32_t offset, int32_t addend, bool is_relative){ 
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);

        if(strcmp(e->name, symbol_name) == 0){
            if(e->instances.data == NULL){
                array_list_create_cap(e->instances, SymbolInstance, 2);
            }

            SymbolInstance current_instance = {offset,addend, is_relative, line, col};
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
    SymbolInstance c = {offset,addend, is_relative, line, col};
    array_list_append(e.instances, SymbolInstance, c); 
    array_list_append(program.symTable.symbols, SymbolTableEntry, e);
}

static bool fits_backward_rel8_jmp(char* lbl, int64_t next_instruction){
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
        if(strcmp(lbl, e->name) == 0){
            if(((int64_t)e->section_offset - (int64_t)next_instruction) >= -128)
                return true;
        }
    }
    return false;
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
        if(section->data == NULL) fatal_error("Out of memory\n");
    }
}


static void section_realloc(Section* section){
   //TODO: CHECK FOR OVERFLOW
   uint64_t new_capacity = section->capacity * 2;
   section->data = realloc(section->data, new_capacity);
   if(section->data == NULL) fatal_error("Out of memory\n");
   
   section->capacity = new_capacity;
}




#define check_section_size(section_ptr, bytes_to_add)\
    if(section->size + bytes_to_add >= section->capacity) section_realloc(section)
    


static inline void section_add_data(Section* section, void* data, size_t size){
    check_section_size(section, size);
    memcpy(section->data + section->size,data,size);
    section->size += size;
}


//uses memmove instead of copy
static inline void section_mov_data(Section* section, void* data, size_t size){
    check_section_size(section, size);
    memmove(section->data + section->size,data,size);
    section->size += size;
}


//TODO: ALLOW PSUEDOINSTRUCTIONS WITHOUT LABELS 
static void parse_bss_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){

        if(!parser_expect_token(p, TOK_IDENTIFIER)){
            parser_next_token(p);
            continue;
        }  

        Token id = p->currentToken;
        parser_next_token(p);

        if(!parser_expect_consume_token(p, TOK_COLON)){
            parser_next_token(p);
            continue;
        }  


        symbol_table_add(id.literal,id.line_number, id.col, program.bss.size, SECTION_BSS, VISIBILITY_LOCAL);

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
                parser_error(p, "Invalid bss section instruction\n");
                parser_next_token(p);
                continue;
        }
        parser_next_token(p);
        int64_t amount = 0;
        if(!parse_and_eval_expression(p, &amount)){
            parser_next_token(p);
            continue;
        }
        program.bss.size += num * (uint64_t)amount; 
        parser_next_token(p);
        parser_expect_consume_token(p, TOK_NEW_LINE);
    } 
}


static bool handle_data_psuedoinstr(Parser* p, Token psuedo_instr_token){
    Token next = p->currentToken;

    int64_t num = 0;
    bool is_floating_point = false;
    if(next.type == TOK_INT){
        if(is_float(p->currentToken.literal)){
            if(!(psuedo_instr_token.type >= TOK_DD && psuedo_instr_token.type <= TOK_DT)){
                parser_error_loc(p,psuedo_instr_token.col,
                        psuedo_instr_token.line_number, "Invalid floating point Psuedoinstruction\n");
                return false;
            }
            is_floating_point = true;
        } else{
            if(!parse_and_eval_expression(p, &num)) return false;
        }

    } else if (next.type == TOK_NSTRING) {
        if(psuedo_instr_token.type != TOK_DB){
            parser_error(p, "Only strings with one byte characters are allowed\n");
            return false;
        }
        size_t str_size = strlen(p->currentToken.literal) + 1;
        section_add_data(&program.data, p->currentToken.literal, str_size);
        return true;
    } else if (next.type == TOK_STRING) {
        if(psuedo_instr_token.type != TOK_DB){
            parser_error(p, "Only strings with one byte characters are allowed\n");
            return false;
        }
        size_t str_size = strlen(p->currentToken.literal);
        section_add_data(&program.data, p->currentToken.literal, str_size);
        return true;
    } else{
        parser_error(p, "Invalid Type or Psuedoinstruction\n");
        return false;
    }

    switch (psuedo_instr_token.type) {
        case TOK_DB: {
            uint8_t temp = 0;
            if(num > UINT8_MAX && !is_int8(num)){
                parser_error(p, "Invalid Size\n");
                return false;
            }
            temp = (uint8_t)num;
            section_add_data(&program.data,&temp, 1);
            break;
        }
        case TOK_DW: {
            uint16_t temp = 0;
            if(num > UINT16_MAX && !is_int16(num)){
                parser_error(p, "Invalid Size\n");
                return false;
            }
            temp = (uint16_t)num;
            section_add_data(&program.data,&temp, 2);
            break;
        }
        case TOK_DD: {
            uint32_t temp = 0;
            if(is_floating_point){
                errno = 0;
                char* endptr = NULL;
                float num = strtof(p->currentToken.literal, &endptr);
                if(*endptr != 0 || errno == ERANGE){
                    parser_error(p,"Invalid float\n");
                    return false;
                }
                section_add_data(&program.data,&num, 4);
                break;
            } else{
                if(num > UINT32_MAX && !is_int32(num)){
                    parser_error(p, "Invalid Size\n", num);
                    return false;
                }
                temp = (uint32_t)num;
            }
            section_add_data(&program.data,&temp, 4);
            break;
        }
        case TOK_DQ: {
            if(is_floating_point){

                errno = 0;
                char* endptr = NULL;
                double num = strtod(p->currentToken.literal, &endptr);
                if(*endptr != 0 || errno == ERANGE){
                    parser_error(p,"Invalid double\n");
                    return false;
                }
                section_add_data(&program.data,&num, 8);
            } else{
                section_add_data(&program.data,&num, 8);
            }
            break;
        }
        case TOK_DT: {
            //there is no way to guarentee that floats are a certain size
            //so we have to implement our own string to float functions
            //for now I am just going to assume floats are 32 bits and doubles are 64
            //because this should be the most common format but in the future I need to
            //implement my own format handler
            parser_error(p, "80 bit floats are not supported yet\n");
            return false;
            break;
        }
        default:
            parser_error_loc(p,psuedo_instr_token.line_number, psuedo_instr_token.col,
                    "Invalid Data Section Instruction\n");
            return false;
    }
    return true;
}


static void parse_data_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){

        if(p->currentToken.type == TOK_IDENTIFIER){
            Token id = p->currentToken;
            parser_next_token(p);

            if(!parser_expect_consume_token(p, TOK_COLON)) goto next_iteration;

            symbol_table_add(id.literal,id.line_number, id.col, program.data.size, SECTION_DATA, VISIBILITY_LOCAL);

            Token next = p->currentToken;

            int start = program.data.size;
            uint64_t num = 0;

            if(next.type == TOK_TIMES){
                parser_next_token(p);
                if(!parser_expect_token(p, TOK_INT)) goto next_iteration;

                if(!string_to_int(p->currentToken.literal, &num)){
                    parser_error(p, "Invalid Number\n");
                    goto next_iteration;
                }

                next = parser_next_token(p);
                num -= 1;

            }
            parser_next_token(p);
            do{
                if(!handle_data_psuedoinstr(p, next)) goto next_iteration;
                parser_next_token(p);
            } while(parser_match_consume_token(p, TOK_COMMA));

            int end = program.data.size;
            for(uint64_t i = 0; i < num; i++){
                section_mov_data(&program.data, &program.data.data[start], end - start);
            }

        } else if (p->currentToken.type == TOK_TIMES) {
            uint64_t num = 0;
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_INT)) goto next_iteration;

            if(!string_to_int(p->currentToken.literal, &num)){
                parser_error(p, "Invalid Number\n");
                goto next_iteration;
            }

            int start = program.data.size;

            Token psuedo_instr_token = parser_next_token(p);
            num -= 1;

            parser_next_token(p);
            do{
                if(!handle_data_psuedoinstr(p, psuedo_instr_token)) goto next_iteration;
                parser_next_token(p);
            } while(parser_match_consume_token(p, TOK_COMMA));

            int end = program.data.size;
            for(uint64_t i = 0; i < num; i++){
                section_mov_data(&program.data, &program.data.data[start], end - start);
            }
        } else{
            Token psuedo_instr_token = p->currentToken;
            parser_next_token(p);
            do{
                if(!handle_data_psuedoinstr(p, psuedo_instr_token)) goto next_iteration;
                parser_next_token(p);
            } while(parser_match_consume_token(p, TOK_COMMA));
        }


        next_iteration:
        parser_expect_consume_token(p, TOK_NEW_LINE);

    }
}


#define MEMORY_TYPE_MASK  0x3f
typedef enum {
    MEM_NO_REGS  = 0,
    MEM_BASE     = 1 << 0,
    MEM_INDEX    = 1 << 1,
    MEM_RELATIVE = 1 << 6,
    MEM_OVERRIDE = 1 << 7,
} MemoryFlag;

#define REGISTER_RSP 0x4
#define REGISTER_RBP 0x5

#define MEMORY_BASE_MASK 0x7
#define MEMORY_SCALE_FACTOR1_MASK 0x3f
#define MEMORY_SCALE_FACTOR2_MASK 0x7f

#define MEMORY_SCALE_FACTOR2 0x40
#define MEMORY_SCALE_FACTOR4 0x80
#define MEMORY_SCALE_FACTOR8 0xC0

typedef struct {
    OperandType type;
    union {
        struct{
            uint8_t registerIndex;
            bool isExtendedRegister;
        };
        uint8_t fpuIndex;

        uint8_t imm8;
        uint16_t imm16;
        uint32_t imm32;
        uint64_t imm64;

        char* label;

        struct {
            uint8_t rex;
            uint8_t data;
            uint8_t flags;
            uint8_t reserved;
            int32_t offset;
            char* label;
        } mem;
    };
    int line;
    int col;
} Operand;


static bool check_scale_factor(Parser* p, int scale, Operand* op, uint8_t* base, uint8_t* index){
    switch (scale) {
        case 1:
            return true;
        case 2:
            op->mem.data |= MEMORY_SCALE_FACTOR2;
            return true;
        case 3:
            //[reg * 3] -> [reg + reg * 2]
            if(*base != REG_MAX) goto invalid_scale;
            *base = *index;
            op->mem.data  |= MEMORY_SCALE_FACTOR2;
            op->mem.flags |= MEM_BASE;
            return true;
        case 4:
            op->mem.data |= MEMORY_SCALE_FACTOR4;
            return true;
        case 5:
            //[reg * 5] -> [reg + reg * 4]
            if(*base != REG_MAX) goto invalid_scale;
            *base = *index;
            op->mem.data  |= MEMORY_SCALE_FACTOR4;
            op->mem.flags |= MEM_BASE;
            return true;
        case 8:
            op->mem.data |= MEMORY_SCALE_FACTOR8;
            return true;
        case 9:
            //[reg * 9] -> [reg + reg * 8]
            if(*base != REG_MAX) goto invalid_scale;
            *base = *index;
            op->mem.data  |= MEMORY_SCALE_FACTOR8;
            op->mem.flags |= MEM_BASE;
            return true;
        default:
            break;
    }
invalid_scale:
    parser_error(p, "Invalid Scale Factor: %i\n", scale);
    return false;

}


static bool parse_memory(Parser* p, OperandType mem_type, Operand* op){
    uint8_t base = REG_MAX;
    uint8_t index = REG_MAX;
    op->type = mem_type;
    op->mem.flags = 0;

    OperandType base_size = OPERAND_NOP;
    OperandType index_size = OPERAND_NOP;

    Token t = parser_peek_token(p);

    int l = t.line_number;
    int c = t.col;
    op->line = l; 
    op->col = c;

    bool is_rel = false;

    if(t.type == TOK_REL){
        is_rel = true;
        op->mem.flags |= MEM_RELATIVE;
        t = parser_next_token(p);
    } 
    

    while(t.type != TOK_CLOSING_BRACKET){
        t = parser_next_token(p);
        switch (t.type) {
            case TOK_IDENTIFIER:
                if(op->mem.label != NULL){ 
                    parser_error(p, "Invalid Address: Cannot have two labels in address\n");
                    return false;
                }
                op->mem.label = p->currentToken.literal;
                break;
            case TOK_REG: {
                    if(is_rel){
                        parser_error(p, "Cannot have a register in a relative address\n");
                        return false;
                    }
                    OperandType size = OPERAND_NOP;
                    uint8_t reg = REG_MAX; 
                    if(is_r64(p->currentToken.reg)){
                        size = OPERAND_R64;
                        reg = p->currentToken.reg - REG_RAX;
                    } else if(is_r32(p->currentToken.reg)){
                        size = OPERAND_R32;
                        reg = p->currentToken.reg - REG_EAX;
                        op->mem.flags |= MEM_OVERRIDE;
                    } else{
                        //In 64 bit mode these registers need to be 32 or 64 bit
                        parser_error(p, "Invalid Register Size\n");
                        return false;
                    }

                    if(parser_peek_token(p).type == TOK_MULTIPLY){
                        if(index != REG_MAX){
                            parser_error(p, "Invalid address\n");
                            return false;
                        }

                        if(reg == (REG_RSP - REG_RAX)){
                            parser_error(p, "This register cannot be an index\n");
                            return false;
                        }

                        parser_next_token(p);
                        parser_next_token(p);
                        if(!parser_expect_token(p, TOK_INT)) return false;
                        uint64_t scale = 0;
                        if(!string_to_int(p->currentToken.literal, &scale)){
                            parser_error(p,"Invalid Number\n");
                            return false;
                        }
                        index = reg;
                        index_size = size;
                        op->mem.flags |= MEM_INDEX;
                        if(!check_scale_factor(p, scale, op, &base, &index))
                            return false;
                    } else if(base == REG_MAX){
                        base_size = size;
                        base = reg;
                        op->mem.flags |= MEM_BASE;
                    } else if(index == REG_MAX){
                        op->mem.flags |= MEM_INDEX;
                        //rsp/r12 cannot be an index 
                        if(reg == (REG_RSP - REG_RAX)){
                            index_size = base_size;
                            index = base;
                            base = reg;
                            base_size = size;
                        } else{
                            index_size = size;
                            index = reg;
                        }
                    } else{
                        parser_error(p, "Invalid address\n");
                        return false;
                    } 
                }
                break;
            case TOK_INT:{
                uint64_t t = 0;
                if(!string_to_int(p->currentToken.literal, &t)){
                    parser_error(p, "Invalid Number\n");
                    return false;
                }
                
                int32_t temp = (int32_t)t;

                if(parser_peek_token(p).type == TOK_MULTIPLY){
                    if(is_rel){
                        Token mul = parser_peek_token(p);
                        parser_error_loc(p, mul.line_number, mul.col, "Invalid Relative Address\n");
                        return false;
                    }
                    if(index != REG_MAX){
                        parser_error(p, "Invalid address\n");
                        return false;
                    }
                    parser_next_token(p);
                    parser_next_token(p);
                    if(!parser_expect_token(p, TOK_REG)) return false;

                    OperandType size = OPERAND_NOP;
                    uint8_t reg = REG_MAX; 
                    if(is_r64(p->currentToken.reg)){
                        size = OPERAND_R64;
                        reg = p->currentToken.reg - REG_RAX;
                    } else if(is_r32(p->currentToken.reg)){
                        size = OPERAND_R32;
                        reg = p->currentToken.reg - REG_EAX;
                        op->mem.flags |= MEM_OVERRIDE;
                    } else{
                        //In 64 bit mode these registers need to be 32 or 64 bit
                        parser_error(p, "Invalid Register Size\n");
                        return false;
                    }

                    if(reg == (REG_RSP - REG_RAX)){
                        parser_error(p, "This register cannot be an index\n");
                        return false;
                    }

                    index = reg;
                    index_size = size;
                    op->mem.flags |= MEM_INDEX;
                    if(!check_scale_factor(p, temp, op, &base, &index))
                        return false;
                } else{
                    op->mem.offset += temp; 
                }
                break;
            }
            case TOK_ADD: {
                Token next = parser_peek_token(p);
                if(next.type != TOK_IDENTIFIER && next.type != TOK_REG && next.type != TOK_INT && next.type != TOK_SUB){
                    parser_error_loc(p, next.line_number, next.col, 
                            "Expected Label, Offset, or Register\n");
                    return false;
                }
            }
            break;
            case TOK_SUB: {
                parser_next_token(p);
                if(!parser_expect_token(p, TOK_INT)) return false;
                uint64_t temp = 0;
                if(!string_to_int(p->currentToken.literal, &temp)){
                    parser_error(p, "Invalid Number\n");
                    return false;
                } 
                op->mem.offset -= (int32_t)temp;   
            }
            break;

            case TOK_MULTIPLY:
                parser_error(p, "Invalid Scale\n");
                return false;
                break;
            case TOK_CLOSING_BRACKET:
                break;
            default:
                if(t.type == TOK_REL){
                    parser_error(p, "Rel must come at the beginning of an address\n");
                } else{
                    parser_error(p, "Invalid Token in address\n");
                } 
                return false; 
        }  
    }

    //ensure the registeres are the same size
    if(base_size != OPERAND_NOP && index_size != OPERAND_NOP && base_size != index_size){ 
        parser_error_loc(p,l,c, "Invalid Address: Registers must be the same size\n");
        return false;
    }
    
    
    if(index != REG_MAX && base == REG_MAX){
        //converts [reg * 1] -> [reg]
        if((op->mem.data & MEMORY_SCALE_FACTOR1_MASK) == op->mem.data){
            base = index;
            index = REG_MAX;
            op->mem.flags ^= MEM_INDEX;
            op->mem.flags |= MEM_BASE;
        } else if ((op->mem.data & MEMORY_SCALE_FACTOR2_MASK) == op->mem.data) {
            //converts [reg * 2] -> [reg + reg]
            base = index;
            //set scale factor to 1
            op->mem.data &= MEMORY_SCALE_FACTOR1_MASK;
            op->mem.flags |= MEM_BASE;
        }
    }

    if(is_extended_reg(base)){
        op->mem.rex |= REX_B;
        base -= 8;
    }

    if(is_extended_reg(index)){
        op->mem.rex |= REX_X;
        index -= 8;
    }

    if(program.flags.ftype == BASM_FILE_MACHO){
        if(op->mem.label != NULL && !is_rel){
            parser_error_loc(p, l, c, "Invalid Address: Macho addresses with labels must be rip relative\n");
            return false;
        }

        if(op->mem.offset != 0){
            parser_error_loc(p, l, c, "Invalid Macho Address: Offsets are not supported\n");
            return false;
        }
    }

    op->mem.data |= ((index & 7) << 3);
    op->mem.data |= base & 7;
    
    return true;
}





static bool parse_operand(Parser* p, Operand* op){
    op->line = p->currentToken.line_number;
    op->col = p->currentToken.col;

    switch (p->currentToken.type) {
        case TOK_REG: { 
            if(is_r256(p->currentToken.reg)){
                op->type = OPERAND_YMM;
                op->registerIndex = p->currentToken.reg - REG_YMM0;
            }
            else if(is_r128(p->currentToken.reg)){
                op->type = OPERAND_XMM;
                op->registerIndex = p->currentToken.reg - REG_XMM0;
            } else if(is_mmx(p->currentToken.reg)){
                op->type = OPERAND_MM;
                op->registerIndex = p->currentToken.reg - REG_MM0;
            }
            else if(is_r64(p->currentToken.reg)){
                op->type = OPERAND_R64;
                op->registerIndex = p->currentToken.reg - REG_RAX;
            } else if(is_r32(p->currentToken.reg)){
                op->type = OPERAND_R32;
                op->registerIndex = p->currentToken.reg - REG_EAX;
            } else if(is_r16(p->currentToken.reg)){
                op->type = OPERAND_R16;
                op->registerIndex = p->currentToken.reg - REG_AX;
            } else {
                op->type = OPERAND_R8;
                op->registerIndex = p->currentToken.reg - REG_AL;
            }             

            op->isExtendedRegister = false;
            if(is_extended_reg(op->registerIndex)){
                op->isExtendedRegister = true;
                op->registerIndex -= 8;
            }


            return true;
        }
        case TOK_SREG:
            op->type = OPERAND_SREG;
            op->registerIndex = p->currentToken.reg;
            return true;
        case TOK_TREG:
            op->type = OPERAND_TMM;
            op->registerIndex = p->currentToken.reg;
            return true;
        case TOK_BNDREG:
            op->type = OPERAND_BND;
            op->registerIndex = p->currentToken.reg;
            return true;
        case TOK_DREG:
            op->type = OPERAND_DREG;
            op->registerIndex = p->currentToken.reg;
            return true;
        case TOK_CREG:
            op->registerIndex = p->currentToken.reg;
            if(p->currentToken.reg == 8){
                op->type = OPERAND_CR8;
                op->isExtendedRegister = true;
                op->registerIndex -= 8;
            }
            else
                op->type = OPERAND_CREG;
            return true;
        case TOK_NSTRING:
        case TOK_STRING:
            op->type = OPERAND_IMM64;
            memccpy(&op->imm64, p->currentToken.literal,0, 8);
            return true;
        case TOK_OPENING_PAREN:
        case TOK_ADD:
        case TOK_NEG:
        case TOK_SUB:
        case TOK_INT:
            if(!parse_and_eval_expression(p, (int64_t*)&op->imm64)){
               return false; 
            }
            op->type = ((uint64_t)1 << 63 & op->imm64) ? OPERAND_SIMM64 : OPERAND_IMM64;
            return true; 

        case TOK_IDENTIFIER: 
            op->type = OPERAND_LABEL;
            op->label = p->currentToken.literal; 
            return true;

        case TOK_ST0:
        case TOK_ST1:
        case TOK_ST2:
        case TOK_ST3:
        case TOK_ST4:
        case TOK_ST5:
        case TOK_ST6:
        case TOK_ST7:
            op->type = OPERAND_STI;
            op->fpuIndex = p->currentToken.type - TOK_ST0;
            return true;

        case TOK_MOFFSET:
            op->type = OPERAND_MOFFSET;
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_INT)) return false;

            if(!string_to_int(p->currentToken.literal, &op->imm64)){
                return false;
            }
            return true;
        case TOK_BYTE:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M8, op); 

        case TOK_WORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M16, op); 

        case TOK_DWORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M32, op); 

        case TOK_QWORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M64, op); 
        case TOK_TWORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M80, op); 
        case TOK_DQWORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M128, op);
        case TOK_YWORD:
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_OPENING_BRACKET)) return false;
            return parse_memory(p, OPERAND_M256, op);
        case TOK_OPENING_BRACKET:
            return parse_memory(p, OPERAND_MEM_ANY, op); 
        default:
            parser_error(p, "Invalid Operand\n");
            return false;
 
    }
}


static inline bool check_operand_certain_register(OperandType table_instr, Operand* op){
    if(table_instr >= OPERAND_AL && table_instr <= OPERAND_RAX){
        if(op->type == OPERAND_R64)
            return table_instr == OPERAND_RAX && (!op->isExtendedRegister && op->registerIndex == 0);
        if (op->type == OPERAND_R32)
            return table_instr == OPERAND_EAX && (!op->isExtendedRegister && op->registerIndex == 0);
        if (op->type == OPERAND_R16) {
            if(op->isExtendedRegister)
                return false;
            if(op->registerIndex == 0 && table_instr == OPERAND_AX)
                return true;
            return op->registerIndex == 2 && table_instr == OPERAND_DX;
        }
        if (op->type == OPERAND_R8 && !op->isExtendedRegister) {
            if(op->registerIndex == 0 && table_instr == OPERAND_AL)
                return true;
            return op->registerIndex == 1 && table_instr == OPERAND_CL;
        }
    }
    return false;
}



static bool fit_immediate(OperandType immediate_size, Operand* op){
    if(op->type == OPERAND_SIMM64){
        switch (immediate_size) {
            case OPERAND_SIMM8:
            case OPERAND_IMM8:
                if(!is_int8(op->imm64)) return false;
                op->imm64 = (int8_t)op->imm64;
                op->type = OPERAND_IMM8;
                return true;
            case OPERAND_SIMM16:
            case OPERAND_IMM16:
                if(!is_int16(op->imm64)) return false;
                op->imm64 = (int16_t)op->imm16;
                op->type = OPERAND_IMM16;
                return true;
            case OPERAND_SIMM32:
            case OPERAND_IMM32:
                if(!is_int32(op->imm64)) return false;
                op->imm64 = (int32_t)op->imm32;
                op->type = OPERAND_IMM32;
                return true;
            case OPERAND_SIMM64:
            case OPERAND_IMM64:
                return true;
            default:
                return false;        
        }
    } else if(op->type == OPERAND_IMM64){
        switch (immediate_size) {
            case OPERAND_SIMM8:
                if(op->imm64 > (uint64_t)INT8_MAX) return false;
                op->type = OPERAND_IMM8;
                return true;
            case OPERAND_IMM8:
                if(op->imm64 > UINT8_MAX) return false;
                op->type = OPERAND_IMM8;
                return true;
            case OPERAND_SIMM16:
                if(op->imm64 > (uint64_t)INT16_MAX) return false;
                op->type = OPERAND_IMM16;
                return true;
            case OPERAND_IMM16:
                if(op->imm64 > UINT16_MAX) return false;
                op->type = OPERAND_IMM16;
                return true;
            case OPERAND_SIMM32:
                if(op->imm64 > (uint64_t)INT32_MAX) return false;
                op->type = OPERAND_IMM32;
                return true;
            case OPERAND_IMM32:
                if(op->imm64 > UINT32_MAX) return false;
                op->type = OPERAND_IMM32;
                return true;
            case OPERAND_SIMM64:
            case OPERAND_IMM64:
                return true;
            default:
                return false;        
        }
    }
    return false;
}


static const Instruction* find_instruction_nop(uint64_t instr_index){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].count;

    for(uint64_t i = op_table_index; i < op_table_index + instruction_variant_count; i++){
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op1 != OPERAND_NOP) continue;

        return (Instruction*)&INSTRUCTION_TABLE[i];

    }
    return NULL;
}

#define END_OP_BITMASK (1 << 21)
static const Instruction* find_instruction_one_operand(uint64_t instr_index, Operand* op){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].count;

    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index; i < op_table_index + instruction_variant_count; i++){
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op2 != OPERAND_NOP) continue;
        if(op->type < END_OP_BITMASK && instruct_var.op1 < END_OP_BITMASK){
            if(instruct_var.op1 & op->type)
                return &INSTRUCTION_TABLE[i];
        } else{
            if(instruct_var.op1 == op->type)
                return &INSTRUCTION_TABLE[i];

            if(is_immediate(instruct_var.op1) && fit_immediate(instruct_var.op1, op))
                return &INSTRUCTION_TABLE[i];

            //for jmp instructions just assume rel32 for now
            if(op->type == OPERAND_LABEL){
                int64_t next_instruction = program.text.size + GET_INSTR_SIZE(instruct_var.flags) + 1;
                // return rel8 jumps if label is within a rel8 for backwards jumps only or
                // the instruction only supports rel8 (loop,loopne,loope, JECXZ)
                if(instruct_var.op1 == OPERAND_REL8 && (instruction_variant_count == 1
                                            || fits_backward_rel8_jmp(op->label,  next_instruction)))
                {
                    return &INSTRUCTION_TABLE[i];
                }
                else if (instruct_var.op1 == OPERAND_REL32)
                    return &INSTRUCTION_TABLE[i];
            }

            // FSTSW, FNSTSW instructions
            if(instruct_var.op1 == OPERAND_AX && op->type == OPERAND_R16 && !op->isExtendedRegister
                    && op->registerIndex == 0)
                return &INSTRUCTION_TABLE[i];

            //push/pop
            if(instruct_var.op1 == OPERAND_FS && op->type == OPERAND_SREG && op->registerIndex == SREG_FS)
                return &INSTRUCTION_TABLE[i];
            if(instruct_var.op1 == OPERAND_GS && op->type == OPERAND_SREG && op->registerIndex == SREG_GS)
                return &INSTRUCTION_TABLE[i];
        }
    }
    return NULL;
}


static const Instruction* find_instruction_two_operands(uint64_t instr_index, Operand* op1, Operand* op2){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].count;

    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index; i < op_table_index + instruction_variant_count; i++){
        Instruction instruct_var = INSTRUCTION_TABLE[i];
        if(instruct_var.op3 != OPERAND_NOP) continue;
        if(op1->type < END_OP_BITMASK && instruct_var.op1 < END_OP_BITMASK){
            if((op1->type & instruct_var.op1) == 0)
                continue;
        } else{
            if(op1->type != instruct_var.op1 && !fit_immediate(instruct_var.op1, op1) &&
                    !check_operand_certain_register(instruct_var.op1, op1))
            continue;
        }

        if(op2->type < END_OP_BITMASK && instruct_var.op2 < END_OP_BITMASK){
            if((op2->type & instruct_var.op2))
                return &INSTRUCTION_TABLE[i];
        } else{
            if(op2->type == instruct_var.op2)
                return &INSTRUCTION_TABLE[i];

            if(fit_immediate(instruct_var.op2, op2))
                return &INSTRUCTION_TABLE[i];

            if(check_operand_certain_register(instruct_var.op2, op2))
                return &INSTRUCTION_TABLE[i];

            // lea instruction
            if(instruct_var.op2 == OPERAND_M && op2->type == OPERAND_MEM_ANY)
                return &INSTRUCTION_TABLE[i];
        }
    }
    return NULL;
}


static const Instruction* find_instruction_three_operands(uint64_t instr_index, Operand* op1, Operand* op2, Operand* op3){ 
    uint64_t op_table_index = get_keyword(instr_index)->value; 
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].count;
    
    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index; i < op_table_index + instruction_variant_count; i++){
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        // 4 operand instructions so skip
        if(instruct_var.encoding == OP_ENC_RVMI || instruct_var.encoding == OP_ENC_RVMR) continue;

        if(op1->type < END_OP_BITMASK && instruct_var.op1 < END_OP_BITMASK){
            if((op1->type & instruct_var.op1) == 0)
                continue;
        } else{
            if(op1->type != instruct_var.op1)
                continue;
        }

        if(op2->type < END_OP_BITMASK && instruct_var.op2 < END_OP_BITMASK){
            if((op2->type & instruct_var.op2) == 0)
                continue;
        } else{
            if((op2->type != instruct_var.op2))
                continue;
        }

        if(op3->type < END_OP_BITMASK && instruct_var.op3 < END_OP_BITMASK){
            if((op3->type & instruct_var.op3))
                return &INSTRUCTION_TABLE[i];
        } else{
            if(op3->type == instruct_var.op3)
                return &INSTRUCTION_TABLE[i];
            else if (fit_immediate(instruct_var.op3, op3))
                return &INSTRUCTION_TABLE[i];
            else if(check_operand_certain_register(instruct_var.op3, op3))
                return &INSTRUCTION_TABLE[i];
        }
    }
    return NULL;
}


static const Instruction* find_instruction_four_operands(uint64_t instr_index, Operand* op1, Operand* op2, Operand* op3, Operand* op4){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].count;

    //the fourth operand is either an imm8 or a register 
    //the register has to be the same size as the first operand 
    if(op4->type == OPERAND_IMM64){
        if(!fit_immediate(OPERAND_IMM8, op4))
            return NULL;
    } else if(op1->type != op4->type) {
        return NULL;
    }
    
    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index; i < op_table_index + instruction_variant_count; i++){
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op1 != op1->type) continue;
        if(instruct_var.op2 != op2->type) continue;

        if(op3->type < END_OP_BITMASK && (instruct_var.op3 & op3->type)){
            if(instruct_var.encoding == OP_ENC_RVMR){
                if(op4->type == OPERAND_XMM || op4->type == OPERAND_YMM)
                    return &INSTRUCTION_TABLE[i];
            }
            else if(instruct_var.encoding == OP_ENC_RVMI) {
                if(op4->type == OPERAND_IMM8)
                    return &INSTRUCTION_TABLE[i];
            }
        }
    }
    return NULL;
}



#define MODRM_INDEX 0
#define SIB_INDEX 1
#define DISPLACEMENT_SIZE 4

#define set_mod(mod) (mod << 6)

// see page 45-46 of intel.pdf for full modrm/sib tables
static int modrm_sib_fields(Operand* op, uint8_t *data, char** label){
    uint8_t ADDRESS_OVERRIDE_PREFIX = 0x67;
    int size = 1;
    int32_t offset = op->mem.offset;


    if(op->mem.label != NULL){ 
        (*label) = op->mem.label;
        //this offset info is stored in the elf file format
        //but for windows we need to keep it in the instruction output
        if(program.flags.ftype != BASM_FILE_PE){
            offset = 0;
        }
    }

    if(op->mem.flags & MEM_OVERRIDE)
        section_add_data(&program.text, &ADDRESS_OVERRIDE_PREFIX, 1);

    if((op->mem.flags & MEMORY_TYPE_MASK) == MEM_NO_REGS){
        if(op->mem.flags & MEM_RELATIVE){
            data[MODRM_INDEX] |= 0x5;
            size += DISPLACEMENT_SIZE;  
            memcpy(data + 1, &offset, DISPLACEMENT_SIZE);
        } else{
            data[MODRM_INDEX] |= 0x4;
            data[SIB_INDEX] = 0x25;
            size++;
            size += DISPLACEMENT_SIZE;  
            memcpy(data + 2, &offset, DISPLACEMENT_SIZE);
        }
    }
    else if ((op->mem.flags & MEM_BASE) && (op->mem.flags & MEM_INDEX)) {
        //both a base and an index
        data[SIB_INDEX] = op->mem.data;
        data[MODRM_INDEX] |= 0x4;
        size++;

        // 32 bit displacement after sib
        if(op->mem.label != NULL || !is_int8(op->mem.offset)){
            data[MODRM_INDEX] += 0x80;
            memcpy(data + size, &offset, DISPLACEMENT_SIZE);
            size += DISPLACEMENT_SIZE;
        } else if (op->mem.offset != 0 || (op->mem.data & MEMORY_BASE_MASK) == REGISTER_RBP) {
            // 8 bit displacement after sib
            data[MODRM_INDEX] += 0x40;
            memcpy(data + size, &offset, 1);
            size += 1;
        }
    }
    else if (op->mem.flags & MEM_BASE) {
        data[MODRM_INDEX] |= op->mem.data & 0x7;

        // the rsp/r12 register requires a sib byte
        if((op->mem.data & MEMORY_BASE_MASK) == REGISTER_RSP){
            data[SIB_INDEX] = 0x24;
            size += 1;
        }

        if(op->mem.label != NULL || !is_int8(op->mem.offset)){
            // 32 bit displacement
            data[MODRM_INDEX] += 0x80;
            memcpy(data + size, &offset, DISPLACEMENT_SIZE);
            size += DISPLACEMENT_SIZE;
        } else if(op->mem.offset != 0){
            //8 bit displacement
            data[MODRM_INDEX] += 0x40;
            memcpy(data + size, &offset, 1);
            size += 1;
        } else if((op->mem.data & MEMORY_BASE_MASK) == REGISTER_RBP){
            //can't just do [RBP] so we do [RBP + disp8]
            data[MODRM_INDEX] |= set_mod(0x01);
            memcpy(data + size, &offset, 1);
            size++;
        }
    }
    else if (op->mem.flags & MEM_INDEX) {
        //need an sib byte
        data[MODRM_INDEX] |= 0x4;
        //0x5 indicates we have no base
        data[SIB_INDEX] = op->mem.data | 0x5;
        size++;
        //either copy zeros, an offset or zeros for a label
        //you need to have a disp32 if you are just using an index
        memcpy(data + 2, &offset, DISPLACEMENT_SIZE);
        size += DISPLACEMENT_SIZE;
    }
    return size;
}


static inline void emit_operand_overide_prefix(OperandType op1, OperandType instr_op2){
    uint8_t prefix = 0x66;
    if(op1 == OPERAND_M16 || op1 == OPERAND_R16 || op1 == OPERAND_IMM16 || instr_op2 == OPERAND_AX){ 
        section_add_data(&program.text,&prefix, 1);
    }

}

#define VEX_REGISTER(idx, is_extended) (is_extended) ? (((~(idx + 8)) & 0xF) << 3): (((~(idx)) & 0xF) << 3);
#define VEX_UNUSED_REG 0x78


static void emit_instruction(Parser *p, const Instruction* instruction, Operand operand[4]){
    uint8_t vex_reg = VEX_UNUSED_REG;

    // these can be rex, vex, or evex
    uint8_t rex = 0;

    uint8_t modrm_sib[6] = {0};
    uint8_t modrm_size = 0;
    char* lbl = NULL;
    int lbl_l = 0;
    int lbl_c = 0;
    int imm_index = -1;

    uint8_t opcode[4] = {0};
    int instruction_size = GET_INSTR_SIZE(instruction->flags);
    memcpy(opcode, instruction->bytes, instruction_size);

    int32_t addend = 0;
    bool is_rel = false; 


    if(instruction->flags & FLAG_OPCODE_EXTENSION){
        modrm_size = 1;
        modrm_sib[MODRM_INDEX] |= GET_OP_DIGIT(instruction->flags);
    }

    if(instruction->encoding != OP_ENC_FPU && (instruction->flags & FLAG_REX_W) == 0)
        emit_operand_overide_prefix(operand[0].type, instruction->op2);

    switch (instruction->encoding) {
        case OP_ENC_ZO:
            break;
        case OP_ENC_I:
            if(is_immediate(instruction->op1))
                imm_index = 0;
            else
                imm_index = 1;
            break;
        case OP_ENC_MI:
            //fall through expected
            imm_index = 1;
        case OP_ENC_M1: // 1 imm is implicit
        case OP_ENC_MC: //cl operand is implicit
        case OP_ENC_M:
            if(is_mem(operand[0].type)){
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset;
                is_rel = operand[0].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
                lbl_l = operand[0].line;
                lbl_c = operand[0].col;
            } else{
                if(operand[0].isExtendedRegister)
                    rex |= REX_B;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[0].registerIndex;
                modrm_size = 1;
            }
            break;
        case OP_ENC_MRI:
            //fall through expected
            imm_index = 2;
        case OP_ENC_MRC: //cl is immplicit
        case OP_ENC_MR:
            if(operand[1].isExtendedRegister)
                rex |= REX_R;

            modrm_sib[MODRM_INDEX] |= (operand[1].registerIndex << 3);

            if(is_mem(operand[0].type)){
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset;
                is_rel = operand[0].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
                lbl_l = operand[0].line;
                lbl_c = operand[0].col;
            } else{
                if(operand[0].isExtendedRegister)
                    rex |= REX_B;

                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[0].registerIndex;
                modrm_size = 1;
            }
            break;
        case OP_ENC_RMI:
            //fall through expected
            imm_index = 2;
        case OP_ENC_RM0: //zero indicates implicit xmm0 operand
        case OP_ENC_RM: 
            if(operand[0].isExtendedRegister)
                rex |= REX_R;

            modrm_sib[MODRM_INDEX] |= operand[0].registerIndex << 3; 
            if(is_mem(operand[1].type)){
                rex |= operand[1].mem.rex;
                addend = operand[1].mem.offset;
                is_rel = operand[1].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
                lbl_l = operand[1].line;
                lbl_c = operand[1].col;
            } else{
                if(operand[1].isExtendedRegister)
                    rex |= REX_B;

                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].registerIndex;
                modrm_size = 1;
            }
            break;
        case OP_ENC_RVMR:
        case OP_ENC_RVMI:
            //fall through expected
            imm_index = 3;
        case OP_ENC_RVM:
            if(operand[0].isExtendedRegister)
                rex |= REX_R;
            vex_reg = VEX_REGISTER(operand[1].registerIndex, operand[1].isExtendedRegister);
            modrm_sib[MODRM_INDEX] |= (operand[0].registerIndex << 3);
            if(is_advanced_reg(operand[2].type) || is_reg32_or_64(operand[2].type)){
                if(operand[2].isExtendedRegister)
                    rex |= REX_B;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[2].registerIndex;
                modrm_size = 1;
            } else{
                rex |= operand[2].mem.rex;
                addend = operand[2].mem.offset;
                is_rel = operand[2].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[2], modrm_sib, &lbl);
                lbl_l = operand[2].line;
                lbl_c = operand[2].col;
            }
            break;
        case OP_ENC_RMV: 
            if(operand[0].isExtendedRegister)
                rex |= REX_R;

            vex_reg = VEX_REGISTER(operand[2].registerIndex, operand[2].isExtendedRegister);
            modrm_sib[MODRM_INDEX] |= (operand[0].registerIndex << 3);
            if(is_mem(operand[1].type)){
                rex |= operand[1].mem.rex;
                addend = operand[1].mem.offset; 
                is_rel = operand[1].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
                lbl_l = operand[1].line;
                lbl_c = operand[1].col;
            } else{
                if(operand[1].isExtendedRegister)
                    rex |= REX_B;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].registerIndex;
                modrm_size = 1;
            }
            break;
        case OP_ENC_VMI: 
            //fallthrough expected
            imm_index = 2;
        case OP_ENC_VM: 
            vex_reg = VEX_REGISTER(operand[0].registerIndex, operand[0].isExtendedRegister);
            if(is_advanced_reg(operand[1].type) || is_reg32_or_64(operand[1].type)){
                if(operand[1].isExtendedRegister)
                    rex |= REX_B;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].registerIndex;
                modrm_size = 1;
            } else{
                rex |= operand[1].mem.rex;
                addend = operand[1].mem.offset;
                is_rel = operand[1].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
                lbl_l = operand[1].line;
                lbl_c = operand[1].col;
            } 
            break;
        case OP_ENC_MVR:
            if(operand[2].isExtendedRegister)
                rex |= REX_R;
            vex_reg = VEX_REGISTER(operand[1].registerIndex, operand[1].isExtendedRegister);
            modrm_sib[MODRM_INDEX] |= (operand[2].registerIndex << 3);
            if(is_advanced_reg(operand[0].type) || is_reg32_or_64(operand[0].type)){
                if(operand[0].isExtendedRegister)
                    rex |= REX_B;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[0].registerIndex;
                modrm_size = 1;
            } else{
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset; 
                is_rel = operand[0].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
                lbl_l = operand[0].line;
                lbl_c = operand[0].col;
            } 
            break;
        //add register to opcode
        case OP_ENC_OI:
            if(operand[0].isExtendedRegister)
                rex |= REX_B;   

            opcode[instruction_size - 1] += operand[0].registerIndex;
            imm_index = 1;
            break;
        case OP_ENC_O: 
            // handles xchg with operand AX,EAX,RAX
            if(operand[1].type != OPERAND_NOP && operand[0].registerIndex == 0 && !operand[0].isExtendedRegister){
                Operand temp = operand[0];
                operand[0] = operand[1];
                operand[1] = temp;
            }
            if(operand[0].isExtendedRegister)
                rex |= REX_B; 

            opcode[instruction_size - 1] += operand[0].registerIndex;
            break;
        case OP_ENC_R:
            if(operand[0].isExtendedRegister)
                rex |= REX_B;
            modrm_size = 1;
            modrm_sib[MODRM_INDEX] |= 192;
            modrm_sib[MODRM_INDEX] |= operand[0].registerIndex;
            break;
        case OP_ENC_D:
            if(instruction->op1 == OPERAND_REL8){
                section_add_data(&program.text, opcode, instruction_size);
                int l = operand[0].line;
                int c = operand[0].col;
                for(int i = 0; i < program.symTable.symbols.size; i++){
                    SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
                    if(strcmp(operand[0].label, e->name) == 0){
                        int32_t rel_addr = ((int64_t)e->section_offset - (int64_t)(program.text.size + 1));
                        if(rel_addr < -128){
                            parser_error_loc(p, l, c, "Jump destination out of range\n");
                            return;
                        }
                        int8_t rel8 = rel_addr;
                        section_add_data(&program.text, &rel8, 1);
                        return;
                    }
                }
                // label hasn't been defined yet
                uint8_t zero = 0;
                symbol_table_add_instance(operand[0].label,l,c, program.text.size, -1,true);
                section_add_data(&program.text, &zero, 1);
                return;
            } else if(instruction->op1 == OPERAND_REL32){
                section_add_data(&program.text, opcode, instruction_size);
                uint32_t zero = 0;
                //add some temp zeros
                int l = operand[0].line;
                int c = operand[0].col;
                symbol_table_add_instance(operand[0].label,l,c, program.text.size, -DISPLACEMENT_SIZE,true);
                section_add_data(&program.text, &zero, 4);
                return;
            } 
            break;
        case OP_ENC_FPU: 
            if(operand[0].type == OPERAND_STI){
                opcode[instruction_size - 1] += operand[0].fpuIndex;
            } else if(is_mem(operand[0].type)){
                //these instructions technically call fwait before being executed
                //if we have an operand override prefix or rex prefix its must
                //come after 0x9b (fwait)
                if(opcode[0] == 0x9b){
                    section_add_data(&program.text, &opcode[0], 1);
                    instruction_size--;
                    opcode[0] = opcode[1];
                    opcode[1] = opcode[2];
                    opcode[2] = opcode[3];
                }
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset;
                is_rel = operand[0].mem.flags & MEM_RELATIVE;
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
                lbl_l = operand[0].line;
                lbl_c = operand[0].col;
            }
            break;
        case OP_ENC_II:
            //the only instruction that has this encoding is enter
            section_add_data(&program.text, opcode, instruction_size);
            section_add_data(&program.text, &operand[0].imm16, 2);
            section_add_data(&program.text, &operand[1].imm8, 1);
            return; 

        //moffsets
        case OP_ENC_FD:
            //fallthrough expected
            imm_index++;
        case OP_ENC_TD: 
            imm_index++;
            if(instruction->flags & FLAG_REX_W){
                uint8_t rexw = REX_PREFIX(1, 0, 0, 0);
                section_add_data(&program.text, &rexw, 1);
            }
            section_add_data(&program.text, opcode, instruction_size); 

            section_add_data(&program.text, &operand[imm_index].imm64, 8);
            return;

        case OP_ENC_RVSV:
            //should be Unreachable 
            print_instruction(instruction);
            fatal_error("These encodings are not supported yet\n");
            break;
    }

    bool upgrade_to_3byte = (instruction->flags & FLAG_TWO_BYTE_VEX) && ((rex & REX_B) || (rex & REX_X));
    if((instruction->flags & FLAG_THREE_BYTE_VEX) || upgrade_to_3byte){
        uint8_t VEX_3BYTE_PREFIX = 0xC4;
        // | R X B | m m m m m|
        // RXB is stored in inverted format
        uint8_t VEX_PAYLOAD0 = ((~rex) << 5) | GET_VEX_OPCODE_MAP(instruction->flags);
        // instruction that can be encoded with 2 byte
        // that needs to be encoded with 3 byte vex
        // set mmmmm part to 1
        if(upgrade_to_3byte)
            VEX_PAYLOAD0 |= 1;

        // |w| vvvv | l | pp|
        uint8_t VEX_PAYLOAD1 = GET_THREE_VEX_PAYLOAD(instruction->flags) | vex_reg;
        section_add_data(&program.text, &VEX_3BYTE_PREFIX, 1);
        section_add_data(&program.text, &VEX_PAYLOAD0, 1);
        section_add_data(&program.text, &VEX_PAYLOAD1, 1);
        section_add_data(&program.text, opcode, instruction_size);
    }
    else if (instruction->flags & FLAG_TWO_BYTE_VEX) {
        uint8_t VEX_2BYTE_PREFIX = 0xC5;
        // |R| vvvv| l | pp
        // R is stored in inverted format
        uint8_t VEX_PAYLOAD = GET_TWO_VEX_PAYLOAD(instruction->flags) | vex_reg;
        if(!(rex & REX_R))
            VEX_PAYLOAD |= (1 << 7);
        section_add_data(&program.text, &VEX_2BYTE_PREFIX, 1);
        section_add_data(&program.text, &VEX_PAYLOAD, 1);
        section_add_data(&program.text, opcode, instruction_size);
    }
    else if (instruction->flags & FLAG_EVEX) {
        fatal_error("EVEX not supported yet Unreachable\n");
    } else{
        rex |= REX_NIBBLE;
        rex |= GET_REX_W(instruction->flags);
        if(rex > REX_NIBBLE){
            const char* REX_8BIT_REG_ERR = "Cannot use AH, BH, CH, or DH with rex prefix\n";
            if(operand[0].type == OPERAND_R8 && is_ah_to_bh(operand[0]))
                return parser_error_loc(p, operand[0].line, operand[0].col, REX_8BIT_REG_ERR);
            if(operand[1].type == OPERAND_R8 && is_ah_to_bh(operand[1]))
                return parser_error_loc(p, operand[1].line, operand[1].col, REX_8BIT_REG_ERR);

            //rex prefix must come right before escape prefix
            if(opcode[1] == 0x0f){
                section_add_data(&program.text, &opcode[0], 1);
                opcode[0] = rex;
            } else{
                section_add_data(&program.text, &rex, 1);
            }
        }
        section_add_data(&program.text, opcode, instruction_size);
    }


    if(modrm_size != 0)
        section_add_data(&program.text, modrm_sib, modrm_size);

    uint64_t lbl_displacement = program.text.size - DISPLACEMENT_SIZE;

    if(imm_index != -1){
        switch (operand[imm_index].type) {
            case OPERAND_IMM8:
                section_add_data(&program.text, &operand[imm_index].imm8, 1);
                break;
            case OPERAND_IMM16:
                section_add_data(&program.text, &operand[imm_index].imm16, 2); 
                break;
            case OPERAND_IMM32:
                section_add_data(&program.text, &operand[imm_index].imm32, 4);
                break;
            case OPERAND_IMM64:
                section_add_data(&program.text, &operand[imm_index].imm64, 8);
                break;
            case OPERAND_YMM:
            case OPERAND_XMM:{
                operand[3].registerIndex += (operand[3].isExtendedRegister) ? 8 : 0;
                uint8_t payload = (operand[3].registerIndex) << 4;
                section_add_data(&program.text, &payload, 1); 
            }
            break;
            default:
                print_instruction(instruction);
                printf("%s\n", operand_to_string(operand[imm_index].type));
                fatal_error("Unreachable: %d\n", imm_index); 
        }
    }

    if(lbl != NULL){
        if(is_rel){
            //addend is the distance from the next instruction for rel addreses
            addend -= (program.text.size - lbl_displacement);

            //for windows/macos we need to store the distance between end of lbl and next instruction inside the instruction
            //TODO: make this cleaner
            if(lbl_displacement != program.text.size - DISPLACEMENT_SIZE && program.flags.ftype != BASM_FILE_ELF){
                int32_t temp = -(program.text.size - (lbl_displacement + DISPLACEMENT_SIZE));
                memcpy(&program.text.data[lbl_displacement], &temp, 4);
            }
        }
        symbol_table_add_instance(lbl, lbl_l, lbl_c, lbl_displacement, addend, is_rel);
    }
}


static bool parse_instruction(Parser* p, TokenType prefix){
    Operand operands[4] = {0};
    int operand_count = 0;
    uint64_t instr = p->currentToken.instruction; 
    int instr_line = p->currentToken.line_number;
    int instr_col = p->currentToken.col;
    while(p->currentToken.type != TOK_NEW_LINE){
        Token op = parser_next_token(p);
        if(op.type == TOK_NEW_LINE) break;

        if(operand_count == 4){
            parser_error(p, "Instructions have a maximum of four operands\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }

        if(!parse_operand(p, &operands[operand_count++])){
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }

        parser_next_token(p);

        if(!match(p, TOK_COMMA, TOK_NEW_LINE)){
            if(p->currentToken.type > 255 || !isprint(p->currentToken.type)){
                parser_error(p, "Expected comma or new line after operand got %s\n",
                    token_to_string(p->currentToken.type));
            } else{
                parser_error(p, "Expected comma or new line after operand got \'%c\'\n",
                    p->currentToken.type);
            }
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }

    }

    const Instruction* instruction = NULL;
    if(operand_count == 0)
        instruction = find_instruction_nop(instr);
    else if (operand_count == 1)
        instruction = find_instruction_one_operand(instr, &operands[0]);
    else if (operand_count == 2)
        instruction = find_instruction_two_operands(instr, &operands[0], &operands[1]);
    else if (operand_count == 3)
        instruction = find_instruction_three_operands(instr, &operands[0], &operands[1], &operands[2]);
    else
        instruction = find_instruction_four_operands(instr,&operands[0],&operands[1], &operands[2], &operands[3]);

    if(instruction == NULL){
        if(operand_count != 0){
            for(int i = 0; i < operand_count; i++)
                scratch_buffer_fmt("%s ", operand_to_string(operands[i].type));

            char* temp = scratch_buffer_as_str();
            parser_error_loc(p,instr_line, instr_col, "Couldn't find instruction for nmemonic: %s %s\n",
                    get_keyword(instr)->name, temp);
            scratch_buffer_clear();
        } else{
            parser_error_loc(p,instr_line, instr_col, "Couldn't find instruction for nmemonic: %s\n",
                    get_keyword(instr)->name);
        }
        parser_expect_consume_token(p, TOK_NEW_LINE);
        return false;
    } 
    
    if(prefix == TOK_LOCK){
        if(!(instruction->flags & FLAG_LOCK)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use LOCK with this instruction\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        else if (!is_mem(operands[0].type)) {
            parser_error_loc(p,operands[0].line, operands[0].col,
                    "Destination Operand must be memory location when using LOCK prefix\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        uint8_t LOCK = 0xf0;
        section_add_data(&program.text, &LOCK, 1);
    }
    else if (prefix == TOK_REP) {
        if(!(instruction->flags & FLAG_REP)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use REP prefix with this instruction\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        uint8_t REP = 0xf3;
        section_add_data(&program.text, &REP, 1);
    }
    else if (prefix == TOK_REPE) {
        if(!(instruction->flags & FLAG_REPE)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use REPE prefix with this instruction\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        uint8_t REPE = 0xf3;
        section_add_data(&program.text, &REPE, 1);
    }
    else if (prefix == TOK_REPNE) {
        // REPE & REPNE support the same instructions
        if(!(instruction->flags & FLAG_REPE)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use REPNE prefix with this instruction\n");
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        uint8_t REPNE = 0xf2;
        section_add_data(&program.text, &REPNE, 1);
    }

    emit_instruction(p, instruction, operands);
    parser_expect_consume_token(p, TOK_NEW_LINE);
    return true;
}


#define TOK_IS_PREFIX(type) (type >= TOK_LOCK && type <= TOK_REPNZ)

static void parse_text_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){

        if(TOK_IS_PREFIX(p->currentToken.type)){
            TokenType prefix = p->currentToken.type;
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_INSTRUCTION)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }
            parse_instruction(p, prefix);
        }
        else if(p->currentToken.type == TOK_GLOBAL || p->currentToken.type == TOK_EXTERN){
            int section = (p->currentToken.type == TOK_GLOBAL) ? SECTION_TEXT : SECTION_EXTERN;
            parser_next_token(p);
            if(!parser_expect_token(p, TOK_IDENTIFIER)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }

            Token id = p->currentToken;
            //TODO: ALLOW MANY GLOBAL DECLARATIONS AT ONCE
            symbol_table_add(id.literal,id.line_number, id.col, 0, section, VISIBILITY_GLOBAL);
            parser_next_token(p);
            if(!parser_expect_consume_token(p, TOK_NEW_LINE)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }
        }
        else if(p->currentToken.type == TOK_IDENTIFIER){
            Token id = p->currentToken;
            parser_next_token(p);
            if(!parser_expect_consume_token(p, TOK_COLON)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            } 
            parser_next_token(p);
            symbol_table_add(id.literal, id.line_number, id.col, program.text.size, SECTION_TEXT, VISIBILITY_LOCAL);
        } 
        else if (p->currentToken.type == TOK_INSTRUCTION) {
            parse_instruction(p, TOK_MAX);
        } else if (p->currentToken.type == TOK_TIMES) {
            Token tamount = parser_next_token(p);
            if(!parser_expect_consume_token(p, TOK_INT)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }


            uint64_t amount = 0;
            if(!string_to_int(tamount.literal, &amount)){
                parser_error_loc(p, tamount.line_number, tamount.col, "Invalid Number\n");
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }

            int start = program.text.size;

            TokenType prefix = TOK_MAX;
            if(TOK_IS_PREFIX(p->currentToken.type)){
                prefix = p->currentToken.type;
                parser_next_token(p);
            }

            if(!parser_expect_token(p, TOK_INSTRUCTION)){
                parser_expect_consume_token(p, TOK_NEW_LINE);
                continue;
            }

            parse_instruction(p, prefix);
            //since section_add_data uses memcpy we have to
            //copy to a temp buffer
            uint8_t instr_opcode[15] = {0};
            int end = program.text.size;
            memcpy(instr_opcode, &program.text.data[start], end - start);

            for(uint32_t i = 0; i < amount - 1; i++)
                section_add_data(&program.text,instr_opcode, end-start);
        }
        else{
            parser_error(p, "Invalid token found in text section\n");
            parser_next_token(p);
        }
    }
}


static void parse_tokens(ArrayList* tokens){
    Parser p ={0};
    p.tokens = tokens;
    p.currentToken.type = TOK_MAX;
    array_list_create_cap(program.symTable.symbols, SymbolTableEntry, 16);

    if(p.tokens->size == 0) return;

    parser_next_token(&p);

    while(p.tokenIndex < (uint32_t)p.tokens->size){
        if(setjmp(p.jmp) == 1){
            break;
        }

        if(p.currentToken.type == TOK_SECTION){
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
                    parser_error(&p, "Expected Section name\n");
                    return;
            }
        } else{
            //just assume we are in the text section
            init_section(&program.text, 256);
            parse_text_section(&p);
        }
    }



}



bool basm_assemble_program(){
     program.ret_code = 0;
     current_fb = file_buffer_create(program.flags.input_file);

     if(current_fb == NULL) return false;
   

     ArrayList tokens = tokenize_file(); 
     tokens = preprocess_tokens(&tokens);

     if(program.ret_code != 0){
        return false;
     }
    
     parse_tokens(&tokens);

     for(int i = 0; i < program.symTable.symbols.size; i++){
         SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);
         if(e->section == SECTION_UNDEFINED && e->visibility == VISIBILITY_UNDEFINED){
             for(int j = 0; j < e->instances.size; j++){
                SymbolInstance* instance =  &array_list_get(e->instances, SymbolInstance, j);
                error_loc(instance->line, instance->col, "%s used but never defined\n", e->name);
             }
             continue;
         }

         for(int j = 0; j < e->instances.size; j++){
             SymbolInstance* instance =  &array_list_get(e->instances, SymbolInstance, j);

             //NOTE WE ONLY ALLOW USING SYMBOLS 
             //IN THE TEXT SECTION FOR NOW 
             if(instance->is_relative){
                 //want the linker to handle relocation outside of text section
                 if(e->section != SECTION_TEXT){
                     continue;
                 }
                 //rel8
                 if(instance->addend == -1){
                     int64_t next_instruction = instance->offset - instance->addend;
                     int64_t rip_addr = e->section_offset;
                     int32_t rel_addr = (int32_t)(rip_addr - next_instruction);
                     if(rel_addr > 127){
                         error_loc(instance->line, instance->col, "Error jump destination out of range\n");
                         continue;
                     }
                     int8_t rel8 = (int8_t)rel_addr;
                     memcpy(&program.text.data[instance->offset], &rel8, 1);
                 } else{
                     //assume size of 4
                     uint64_t next_instruction = instance->offset - instance->addend;
                     uint64_t rip_addr = e->section_offset;
                     int32_t rel_addr = (int32_t)(rip_addr - next_instruction);
                     memcpy(&program.text.data[instance->offset], &rel_addr, 4);
                 }
             }

         }
     }

     file_buffer_delete(current_fb);

     if(program.ret_code != 0){
        return false;
     }

     if(program.flags.ftype == BASM_FILE_ELF){
        return write_elf(program.flags.input_file, program.flags.output_file, &program);
     } else if(program.flags.ftype == BASM_FILE_PE){
        return write_pe(program.flags.input_file, program.flags.output_file, &program);
     } else if(program.flags.ftype == BASM_FILE_MACHO){
        return write_macho(program.flags.input_file, program.flags.output_file, &program);
     }
     else{
         fprintf(stderr, "Unknown output file type\n");
         return false;
     }
}




bool basm_parse_flags(int argc, char** argv){
    if(argc < 2){
        fatal_error("No input file specified\nType basm --help for more info\n");
        return false;
    }
    program.flags.output_file = "a.out";

    for(int i = 1; i < argc; i++){
        if(strcmp("-f", argv[i]) == 0){
            i++;
            if(i == argc){
                fatal_error("Invalid File Type\n");
                return false;
            }
            if(strcmp("win", argv[i]) == 0){
                program.flags.ftype = BASM_FILE_PE;
            } else if (strcmp("elf", argv[i]) == 0) { 
                program.flags.ftype = BASM_FILE_ELF;
            } else if (strcmp("macho", argv[i]) == 0) {
                program.flags.ftype = BASM_FILE_MACHO;
            } else{
                fatal_error("Invalid File Type: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp("-o", argv[i]) == 0) {
            i++;
            if(i == argc){
                fatal_error("Output file not specified\n");
                return false;
            }
            program.flags.output_file = argv[i];
             
        } else if(string_cmp_lower("--help", argv[i]) == 0){
            basm_help();
            return false;
        }else{
            program.flags.input_file = argv[i];
        }
    }
    
    if(program.flags.input_file == NULL) fatal_error("No input file\n");

    return true;
}

void basm_help(){
    printf("Usage: basm options file\n");
    printf("Flags: \n");
    printf("-f (file type)        -> win | elf | macho\n");
    printf("-o (output file name) -> output file\n");
}
