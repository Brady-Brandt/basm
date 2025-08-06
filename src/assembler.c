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



/*
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

#define is_ah_to_bh(index) (index >= (REG_AH - REG_AL) && index <= (REG_BH - REG_AL))
#define is_xy_reg(r) (r == OPERAND_XMM || r == OPERAND_YMM)
#define is_advanced_reg(r) (r >= OPERAND_MM && r <= OPERAND_YMM)
#define is_reg32_or_64(type) (type == OPERAND_R32 || type == OPERAND_R64)
#define is_immediate(i) (i >= OPERAND_IMM8 && i <= OPERAND_IMM64)
#define is_label(l) (l >= OPERAND_L8 && l <= OPERAND_L64)
#define is_general_reg(r) (r >= OPERAND_R8 && r <= OPERAND_R64)
#define is_mem(m) (m >= OPERAND_M8 && m <= OPERAND_M80 || m == OPERAND_MEM_ANY)
#define is_relative(x) (x >= OPERAND_REL8 && x <= OPERAND_REL32)


#define is_int32(n) ((int64_t)n <= INT32_MAX && (int64_t)n >= INT32_MIN)
#define is_int16(n) ((int64_t)n <= INT16_MAX && (int64_t)n >= INT16_MIN)
#define is_int8(n) ((int64_t)n <= INT8_MAX && (int64_t)n >= INT8_MIN)


FileBuffer* current_fb = NULL;

Program program = {0};


void symbol_table_add(Parser* p, char* name, uint64_t offset, uint8_t section, uint8_t visibility){
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
           parser_error(p, "Many definitions of symbol: %s\n", name); 
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

void symbol_table_add_instance(char* symbol_name, uint32_t offset, int32_t addend, bool is_relative){ 
    for(int i = 0; i < program.symTable.symbols.size; i++){
        SymbolTableEntry* e = &array_list_get(program.symTable.symbols, SymbolTableEntry, i);

        if(strcmp(e->name, symbol_name) == 0){
            if(e->instances.data == NULL){
                array_list_create_cap(e->instances, SymbolInstance, 2);
            }

            SymbolInstance current_instance = {offset,addend, is_relative};
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
    SymbolInstance c = {offset,addend, is_relative};
    array_list_append(e.instances, SymbolInstance, c); 
    array_list_append(program.symTable.symbols, SymbolTableEntry, e);
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


        symbol_table_add(p, id.literal, program.bss.size, SECTION_BSS, VISIBILITY_LOCAL);

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

            symbol_table_add(p, id.literal, program.data.size, SECTION_DATA, VISIBILITY_LOCAL);

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


#define mem_op_prefix(mem) (mem.scale & 64)
#define mem_set_prefix(mem) (mem.scale |= 64)
#define mem_get_scale(mem) ((mem.scale & 63))

#define mem_set_rel(mem) (mem.scale |= 128)
#define mem_is_rel(mem) (mem.scale & 128)

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

            uint8_t scale; //store if we need op override prefix in upper bit         

            int32_t offset;            
            char* label;
        } mem;


    };
    int line;
    int col;
} Operand;


static bool check_scale_factor(Parser* p, int scale, Operand* op){
    switch (scale) {
        case 1:
            return true;
        case 2:
            op->mem.scale |= 1; 
            return true;
        case 3:
            //[reg * 3] -> [reg + reg * 2]
            if(op->mem.base != REG_MAX) goto invalid_scale;
            op->mem.base = op->mem.index;
            op->mem.scale |= 1;
            return true;
        case 4:
            op->mem.scale |= 2;
            return true;
        case 5:
            //[reg * 5] -> [reg + reg * 4]
            if(op->mem.base != REG_MAX) goto invalid_scale;
            op->mem.base = op->mem.index;
            op->mem.scale |= 2;
            return true;
        case 8:
            op->mem.scale |= 3;
            return true;
        case 9:
            //[reg * 9] -> [reg + reg * 8]
            if(op->mem.base != REG_MAX) goto invalid_scale;
            op->mem.base = op->mem.index;
            op->mem.scale |= 3;
            return true;
        default:
            break;
    }
invalid_scale:
    parser_error(p, "Invalid Scale Factor: %i\n", scale);
    return false;

}


static bool parse_memory(Parser* p, OperandType mem_type, Operand* op){
    op->mem.base = REG_MAX;
    op->mem.index = REG_MAX;
    op->mem.rex |= REX_PREFIX(0, 0, 0, 0);
    op->type = mem_type;

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
        mem_set_rel(op->mem);
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
                        mem_set_prefix(op->mem);
                    } else{
                        //In 64 bit mode these registers need to be 32 or 64 bit
                        parser_error(p, "Invalid Register Size\n");
                        return false;
                    }

                    if(parser_peek_token(p).type == TOK_MULTIPLY){
                        if(op->mem.index != REG_MAX){
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
                        op->mem.index = reg;
                        if(!check_scale_factor(p, scale, op)) return false; 
                        index_size = size;
                    } else if(op->mem.base == REG_MAX){ 
                        base_size = size;
                        op->mem.base = reg;
                    } else if(op->mem.index == REG_MAX){ 
                        //rsp/r12 cannot be an index 
                        if(reg == (REG_RSP - REG_RAX)){
                            index_size = base_size;
                            op->mem.index = op->mem.base;
                            op->mem.base = reg;
                            base_size = size;
                        } else{
                            index_size = size;
                            op->mem.index = reg;
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
                    if(op->mem.index != REG_MAX){
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
                        mem_set_prefix(op->mem);
                    } else{
                        //In 64 bit mode these registers need to be 32 or 64 bit
                        parser_error(p, "Invalid Register Size\n");
                        return false;
                    }

                    if(reg == (REG_RSP - REG_RAX)){
                        parser_error(p, "This register cannot be an index\n");
                        return false;
                    }

                    op->mem.index = reg;
                    if(!check_scale_factor(p, temp, op)) return false; 
                    index_size = size;
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
    
    
    if(op->mem.index != REG_MAX && op->mem.base == REG_MAX){
        //converts [reg * 1] -> [reg]
        if(mem_get_scale(op->mem) == 0){
            op->mem.base = op->mem.index;
            op->mem.index = REG_MAX;
        } else if (mem_get_scale(op->mem) == 1) { 
            //converts [reg * 2] -> [reg + reg]
            op->mem.base = op->mem.index;
            //set scale factor to 0
            op->mem.scale ^= 1;
        }
        
    }

    if(is_extended_reg(op->mem.base)){
        op->mem.rex |= REX_B;
        op->mem.base -= 8;
    }

    if(is_extended_reg(op->mem.index)){
        op->mem.rex |= REX_X;
        op->mem.index -= 8;
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

    
    return true;
}





static bool parse_operand(Parser* p, Operand* op){
    op->line = p->currentToken.line_number;
    op->col = p->currentToken.col;

    switch (p->currentToken.type) {
        case TOK_REG: {
            uint8_t w = 0;
            
            if(is_r256(p->currentToken.reg)){
                op->type = OPERAND_YMM;
                op->reg.registerIndex = p->currentToken.reg - REG_YMM0;
            }
            else if(is_r128(p->currentToken.reg)){
                op->type = OPERAND_XMM;
                op->reg.registerIndex = p->currentToken.reg - REG_XMM0;
            } else if(is_mmx(p->currentToken.reg)){
                op->type = OPERAND_MM;
                op->reg.registerIndex = p->currentToken.reg - REG_MM0;
            }
            else if(is_r64(p->currentToken.reg)){
                w = 1;
                op->type = OPERAND_R64;
                op->reg.registerIndex = p->currentToken.reg - REG_RAX;
            } else if(is_r32(p->currentToken.reg)){
                op->type = OPERAND_R32;
                op->reg.registerIndex = p->currentToken.reg - REG_EAX;
            } else if(is_r16(p->currentToken.reg)){
                op->type = OPERAND_R16;
                op->reg.registerIndex = p->currentToken.reg - REG_AX;
            } else {
                op->type = OPERAND_R8;
                op->reg.registerIndex = p->currentToken.reg - REG_AL;
            }             
            op->reg.rex = REX_PREFIX(w, 0, 0, 0);
            return true;
        }
        case TOK_SREG:
            op->type = OPERAND_SREG;
            op->reg.registerIndex = p->currentToken.reg;
            op->reg.rex = REX_PREFIX(0, 0, 0, 0);
            return true;
        case TOK_TREG:
            op->type = OPERAND_TMM;
            op->reg.registerIndex = p->currentToken.reg;
            op->reg.rex = REX_PREFIX(0, 0, 0, 0);
            return true;
        case TOK_BNDREG:
            op->type = OPERAND_BND;
            op->reg.registerIndex = p->currentToken.reg;
            op->reg.rex = REX_PREFIX(0, 0, 0, 0);
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
            op->type = OPERAND_L64;
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
            op->fpu_stack_index = p->currentToken.type - TOK_ST0;
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
            op->mem.rex |= REX_W;
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



//converts an instruction of operand type register or memory to operand type RM 
#define TO_RM(input_instr, reg8_or_m8) (input_instr + (OPERAND_RM8 - reg8_or_m8))



static inline bool check_operand_certain_register(OperandType table_instr, OperandType input_instr, uint8_t reg_index){
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


static bool check_operand_type(OperandType table_instr, OperandType input_instr, uint8_t reg_index){
    if(table_instr == input_instr) return true;

    //these instructions either take a memory location or registers
    if(table_instr >= OPERAND_RM8 && table_instr <= OPERAND_RM64){
        if(table_instr == TO_RM(input_instr, OPERAND_M8)|| table_instr == TO_RM(input_instr, OPERAND_R8)){
            return true;
        }
    }

    if(table_instr == OPERAND_M && input_instr >= OPERAND_MEM_ANY && input_instr <= OPERAND_M64) return true;

    if(table_instr >= OPERAND_XMMM8 && table_instr <= OPERAND_XMMM128){
        if(input_instr == OPERAND_XMM || (input_instr + (OPERAND_XMMM8 - OPERAND_M8)) == table_instr ) return true;
    }
 
    if(table_instr == OPERAND_YMMM256 && (input_instr == OPERAND_YMM || input_instr == OPERAND_M256)) return true;

    if(table_instr == OPERAND_MMM32 || table_instr == OPERAND_MMM64){
        return input_instr == OPERAND_MM || table_instr == input_instr + (OPERAND_MMM32 - OPERAND_M32);
    }

    if(table_instr == OPERAND_BNDM128 && (input_instr == OPERAND_M128 || input_instr == OPERAND_BND)) return true;

    return check_operand_certain_register(table_instr, input_instr, reg_index); 
}

static const Instruction* find_instruction_nop(uint64_t instr_index){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;

    for(uint64_t i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op1 != OPERAND_NOP) continue;

        return (Instruction*)&INSTRUCTION_TABLE[i];

    }
    return NULL;
}

static const Instruction* find_instruction_one_operand(uint64_t instr_index, Operand* op){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;

    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op2 != OPERAND_NOP) continue;

        if(instruct_var.op1 == op->type ) return (Instruction*)&INSTRUCTION_TABLE[i];
        

        if(is_immediate(instruct_var.op1) && fit_immediate(instruct_var.op1, op)){
            return (Instruction*)&INSTRUCTION_TABLE[i];
        }

        //these instructions either take a memory location or registers
        if(instruct_var.op1 >= OPERAND_RM8 && instruct_var.op2 <= OPERAND_RM64){
            if(instruct_var.op1 == TO_RM(op->type, OPERAND_M8)|| instruct_var.op1 == TO_RM(op->type, OPERAND_R8)){
                return (Instruction*)&INSTRUCTION_TABLE[i];
            }
        }
        
        //for jmp instructions just assume rel32 for now
        if(instruct_var.op1 == OPERAND_REL32 && op->type == OPERAND_L64) return (Instruction*)&INSTRUCTION_TABLE[i];

        //check for ax,al,eax,rax operands
        if(check_operand_certain_register(instruct_var.op1, op->type, op->reg.registerIndex)){
            return (Instruction*)&INSTRUCTION_TABLE[i];
        }

        if(op->type == OPERAND_MEM_ANY){
            if(instruct_var.op1 == OPERAND_M512){
                op->type = OPERAND_M512;
                return &INSTRUCTION_TABLE[i];
            }else if(instruction_variant_count == 1 && is_mem(instruct_var.op1)) return &INSTRUCTION_TABLE[i];
            else continue;
        } 

    }

    return NULL;
}


static const Instruction* find_instruction_two_operands(uint64_t instr_index, Operand* op1, Operand* op2){ 

    //infer size of memory location if operand 2 is a register 
    if(op1->type == OPERAND_MEM_ANY){
        if(is_general_reg(op2->type)) op1->type = op2->type + (OPERAND_M8 - OPERAND_R8); 
        else if (op2->type == OPERAND_BND) op1->type = OPERAND_M128;
         ;
    }

    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;
    
    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op3 != OPERAND_NOP) continue;
        

        if(!check_operand_type(instruct_var.op1, op1->type, op1->reg.registerIndex)){
            if(!is_immediate(instruct_var.op1)){
                continue;
            }
            if(!fit_immediate(instruct_var.op1, op1)){
                continue;
            }
        }


        if(op2->type == OPERAND_MEM_ANY){
            if(is_mem(instruct_var.op2)){
                op2->type = instruct_var.op2;
                return &INSTRUCTION_TABLE[i];
            } else if (instruct_var.op2 >= OPERAND_RM8 && instruct_var.op2 <= OPERAND_RM64) {
                op2->type = OPERAND_M8 + (instruct_var.op2 - OPERAND_RM8); 
                return &INSTRUCTION_TABLE[i];
            } else if(instruct_var.op2 >= OPERAND_XMMM8 && instruct_var.op2 <= OPERAND_YMMM256){ 
                op2->type = OPERAND_M8 + (instruct_var.op2 - OPERAND_XMMM8); 
                return &INSTRUCTION_TABLE[i];
            } else if (instruct_var.op2 == OPERAND_BNDM128) {
                op2->type = OPERAND_M128;
                return &INSTRUCTION_TABLE[i]; 
            }
        } else if ((op2->type == OPERAND_IMM64 || op2->type == OPERAND_SIMM64) && 
                    fit_immediate(instruct_var.op2, op2)) {
            return &INSTRUCTION_TABLE[i];
        }

        if(check_operand_type(instruct_var.op2, op2->type, op2->reg.registerIndex)){
            return &INSTRUCTION_TABLE[i];
        }    
    }

    return NULL;
}

static bool check_operand_type_three(OperandType table_instr, OperandType input_instr){
    if(table_instr == input_instr) return true;

    //these instructions either take a memory location or registers
    if(table_instr >= OPERAND_RM16 && table_instr <= OPERAND_RM64){
        if(table_instr == TO_RM(input_instr, OPERAND_M8)|| table_instr == TO_RM(input_instr, OPERAND_R8)){
            return true;
        }
    }

    if(table_instr >= OPERAND_XMMM32 && table_instr <= OPERAND_XMMM128){
        if(input_instr == OPERAND_XMM || (input_instr + (OPERAND_XMMM8 - OPERAND_M8)) == table_instr ) return true;
    }
 
    if(table_instr == OPERAND_YMMM256 && (input_instr == OPERAND_YMM || input_instr == OPERAND_M256)) return true;

    if(table_instr == OPERAND_REG && (input_instr == OPERAND_R32 || input_instr == OPERAND_R64)) return true;

    return table_instr == OPERAND_MMM64 && (input_instr == OPERAND_MM || input_instr == OPERAND_M64);
}



static const Instruction* find_instruction_three_operands(uint64_t instr_index, Operand* op1, Operand* op2, Operand* op3){ 
    uint64_t op_table_index = get_keyword(instr_index)->value; 
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;
    
    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.encoding == OP_ENC_RVMI || instruct_var.encoding == OP_ENC_RVMR) continue;

        if(op1->type == OPERAND_MEM_ANY && check_operand_type_three(instruct_var.op2, op2->type)){
           if(fit_immediate(instruct_var.op3, op3)){
                if(is_mem(instruct_var.op1)){
                    op1->type = instruct_var.op1;
                    return &INSTRUCTION_TABLE[i];
                } else if (instruct_var.op1 >= OPERAND_RM16 && instruct_var.op1 <= OPERAND_RM64) {
                    op1->type = OPERAND_M16 + (instruct_var.op1 - OPERAND_RM16); 
                    return &INSTRUCTION_TABLE[i];
                } else if(instruct_var.op1 >= OPERAND_XMMM64 && instruct_var.op1 <= OPERAND_XMMM128){ 
                    op1->type = OPERAND_M64 + (instruct_var.op1 - OPERAND_XMMM64); 
                    return &INSTRUCTION_TABLE[i];
                }
           } 
           continue;
        } else if (op2->type == OPERAND_MEM_ANY && check_operand_type_three(instruct_var.op1, op1->type)) {
            if(instruct_var.op3 == op3->type || fit_immediate(instruct_var.op3, op3)){ 
                if(instruct_var.op2 >= OPERAND_RM16 && instruct_var.op2 <= OPERAND_RM64){
                    op2->type = OPERAND_M16 + (instruct_var.op2 - OPERAND_RM16);
                    return &INSTRUCTION_TABLE[i];
                } else if (instruct_var.op2 >= OPERAND_XMMM32 && instruct_var.op2 <= OPERAND_YMMM256) {
                    op2->type = OPERAND_M32 + (instruct_var.op2 - OPERAND_XMMM32); 
                    return &INSTRUCTION_TABLE[i];
                } else if(instruct_var.op2 == OPERAND_MMM64){ 
                    op2->type = OPERAND_M64;
                    return &INSTRUCTION_TABLE[i];
                }
           } 
            continue;
        }
       

        if(!check_operand_type_three(instruct_var.op1, op1->type)) continue;
        if(!check_operand_type_three(instruct_var.op2, op2->type)) continue;

    
        if(op3->type == OPERAND_MEM_ANY){
            if(instruct_var.op3 == OPERAND_RM32 || instruct_var.op3 == OPERAND_RM64){
                op3->type = OPERAND_M32 + (instruct_var.op3 - OPERAND_RM32);
                return &INSTRUCTION_TABLE[i];
            } else if (instruct_var.op3 == OPERAND_XMMM128 || instruct_var.op3 == OPERAND_YMMM256) { 
                op3->type = OPERAND_M128 + (instruct_var.op3 - OPERAND_XMMM128);
                return &INSTRUCTION_TABLE[i];
            }
        } else if (op3->type == OPERAND_SIMM64|| op3->type == OPERAND_IMM64) {
            if(fit_immediate(instruct_var.op3, op3)) return &INSTRUCTION_TABLE[i]; 
        } else if (instruct_var.op3 == OPERAND_CL && op3->type == OPERAND_R8 && op3->reg.registerIndex == 1) {
            return &INSTRUCTION_TABLE[i]; 
        } 
        else if (check_operand_type_three(instruct_var.op3, op3->type)) {
            return &INSTRUCTION_TABLE[i]; 
        }
            
    }

    return NULL;
}


#define TO_RM(input_instr, reg8_or_m8) (input_instr + (OPERAND_RM8 - reg8_or_m8))

static bool check_operand_type_four(OperandType table_instr, OperandType input_instr){
    if(table_instr >= OPERAND_XMMM32 && table_instr <= OPERAND_YMMM256){
        if(table_instr == input_instr + (OPERAND_XMMM32 - OPERAND_M32) || input_instr == OPERAND_XMM || input_instr == OPERAND_YMM){
            return true;
        }
    }
    if(table_instr == OPERAND_RM32 && input_instr == OPERAND_R32 || input_instr == OPERAND_M32) return true;
    if(table_instr == OPERAND_RM64 && input_instr == OPERAND_R64|| input_instr == OPERAND_M64) return true;
    return false;
}

static const Instruction* find_instruction_four_operands(uint64_t instr_index, Operand* op1, Operand* op2, Operand* op3, Operand* op4){
    uint64_t op_table_index = get_keyword(instr_index)->value;    
    int instruction_variant_count = INSTRUCTION_TABLE[op_table_index].variant_count;

    //the fourth operand is either an imm8 or a register 
    //the register has to be the same size as the first operand 
    if(op4->type == OPERAND_IMM64){
        if(!fit_immediate(OPERAND_IMM8, op4)) return NULL;
    } else if(op1->type != op4->type) {
       return NULL; 
    }
    
    // loop through each variant of the instruction check if the operands match 
    for(uint64_t i = op_table_index + 1; i < op_table_index + instruction_variant_count + 1; i++){ 
        Instruction instruct_var = INSTRUCTION_TABLE[i];

        if(instruct_var.op1 != op1->type) continue;
        if(instruct_var.op2 != op2->type) continue;

        if(op3->type == OPERAND_MEM_ANY){
            if(instruct_var.op3 == OPERAND_RM32 || instruct_var.op3 == OPERAND_RM64){
                op3->type = OPERAND_M32 + (instruct_var.op3 - OPERAND_RM32); 
            } else if (instruct_var.op3 >= OPERAND_XMMM32 && instruct_var.op3 <= OPERAND_YMMM256) { 
                op3->type = OPERAND_M32 + (instruct_var.op3 - OPERAND_XMMM32);
            }
        } else if(!check_operand_type_four(instruct_var.op3, op3->type)){
            continue;
        }


        if(instruct_var.encoding == OP_ENC_RVMR){
            if(!is_xy_reg(op4->type)) continue;
        } else if(instruct_var.encoding == OP_ENC_RVMI) {
            if(op4->type != OPERAND_IMM8) continue; 
        } else{
            continue;
        }
 
        return &INSTRUCTION_TABLE[i];
    } 
    return NULL;
}



#define MODRM_INDEX 0
#define SIB_INDEX 1
#define DISPLACEMENT_SIZE 4

#define set_mod(mod) (mod << 6)
#define set_sib(scale, index, base) ((scale << 6) | (index << 3) | base)

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

    if(mem_op_prefix(op->mem)) section_add_data(&program.text, &ADDRESS_OVERRIDE_PREFIX, 1);
        
    if(op->mem.base == REG_MAX && op->mem.index == REG_MAX){
        if(mem_is_rel(op->mem)){
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
    } else if (op->mem.base != REG_MAX && op->mem.index == REG_MAX) {
        data[MODRM_INDEX] |= op->mem.base; 

        int offset_size = 0;

        if(op->mem.label != NULL || !is_int8(op->mem.offset)){
            // we want a 32 bit displacement
            data[MODRM_INDEX] += 0x80;
            offset_size = DISPLACEMENT_SIZE;
            size += DISPLACEMENT_SIZE;
        } else if(op->mem.base == (REG_RBP - REG_RAX)){
            //can't just do [RBP] so we do [RBP + disp8]
            data[MODRM_INDEX] |= set_mod(0x01);
            size++;
            offset_size = 1;
        } else if(op->mem.offset != 0){
            //8 bit displacement 
            size += 1;
            data[MODRM_INDEX] += 0x40;
            offset_size  = 1; 
        } 
        // the rsp/r12 register requires a sib byte
        if(op->mem.base == (REG_RSP - REG_RAX)){
            if(offset_size == 0) data[MODRM_INDEX] = 0x4; 
            data[SIB_INDEX] = 0x24; 
            size += 1;
        }

        if(offset_size != 0) memcpy(data + size - offset_size, &offset, offset_size);

    } else if (op->mem.base == REG_MAX && op->mem.index != REG_MAX) {
        //need an sib byte
        data[MODRM_INDEX] |= 0x4;
        //0x5 indicates we have no base
        data[SIB_INDEX] = set_sib(op->mem.scale, op->mem.index, 0x5);
        size++;
        size += DISPLACEMENT_SIZE;
        //either copy zeros, an offset or zeros for a label
        //you need to have a disp32 if you are just using an index
        memcpy(data + 2, &offset, DISPLACEMENT_SIZE); 
    } else{
        //both a base and an index 
        int offset_size = 0;

        if(op->mem.base == (REG_RBP - REG_RAX)){
            data[SIB_INDEX] = set_sib(op->mem.scale, op->mem.index, 0x5);
            offset_size = 1;
        } else{
            data[SIB_INDEX] = set_sib(op->mem.scale, op->mem.index, op->mem.base);
        }

        data[MODRM_INDEX] |= 0x4;
        if(op->mem.label != NULL || !is_int8(op->mem.offset)){
            data[MODRM_INDEX] += 0x80;
            size += DISPLACEMENT_SIZE;
            offset_size = DISPLACEMENT_SIZE;
        } else if (op->mem.offset != 0 || op->mem.base == (REG_RBP - REG_RAX)) { 
            data[MODRM_INDEX] += 0x40;
            size += 1;
            offset_size = 1;
        } 
        size++;
        if(offset_size != 0) memcpy(data + size - offset_size, &offset, offset_size); 
    } 
    

    return size;
}

/*
Two Byte 

0xC5 R vvvv L pp 
R - > Extension of reg field
vvvv -> ~register index ones complement

Three byte
0xC4 R X B mmmmm   W vvvv L pp


*/



#define VEX_ONE_BYTE_DEFAULT 0x80
#define VEX_TWO_BYTE_R 0x8000
#define VEX_TWO_BYTE_X 0x4000
#define VEX_TWO_BYTE_B 0x2000
#define VEX_UNUSED_REG 0x78

static inline void set_b(Operand* op, uint16_t* vex, uint8_t* rex){
    if(is_extended_reg(op->reg.registerIndex)){
        op->reg.registerIndex -= 8;
        (*vex) ^= VEX_TWO_BYTE_B;
        (*rex) |= REX_B;
    }
    (*rex) |= op->reg.rex;
}

static inline void set_r(Operand* op, uint16_t* vex, uint8_t* rex){
    if(is_extended_reg(op->reg.registerIndex)){
        op->reg.registerIndex -= 8;
        (*rex) |= REX_R;
    } else{
        (*vex) |= VEX_ONE_BYTE_DEFAULT;
    }
    (*rex) |= op->reg.rex;
}


static inline void emit_operand_overide_prefix(OperandType op1, OperandType instr_op2){
    uint8_t prefix = 0x66;
    if(op1 == OPERAND_M16 || op1 == OPERAND_R16 || op1 == OPERAND_IMM16 || instr_op2 == OPERAND_AX){ 
        section_add_data(&program.text,&prefix, 1);
    }

}



//R bit gets inverted 
#define REX_TO_ONE_BYTE_VEX(rex) ((~(rex >> 2)) << 7)

//convert the rex prefix on memory operand to vex 
//perform xor on vex
#define REX_MEM_TO_VEX(rex) ((uint16_t)rex << 13)

#define ONE_VEX_TO_TWO_BYTE_VEX(vex) (((VEX_TWO_BYTE_R) & ((vex & 0x80) << 8)) | vex & 0x7F7F)
#define VEX_REGISTER(reg) (((~(reg)) & 0xF) << 3)


static void emit_instruction(Parser *p, const Instruction* instruction, Operand operand[4]){
    uint16_t vex = 0xE000;
    uint8_t rex = 0;
    uint8_t modrm_sib[6] = {0};
    uint8_t modrm_size = 0;
    char* lbl = NULL;
    int imm_index = -1;

    uint8_t opcode[4] = {0};
    int instruction_size = instruction->size;
    memcpy(opcode, instruction->bytes, instruction->size);

    int32_t addend = 0;
    bool is_rel = false;

    //indicate opcode extension in the reg portion of modrm
    if(instruction->digit != -1){
        modrm_size = 1;
        modrm_sib[MODRM_INDEX] |= (instruction->digit << 3);
    }

    bool op1_is_upper_reg = false;
    bool op2_is_upper_reg = false;

    if(operand[0].type == OPERAND_R8 && is_ah_to_bh(operand[0].reg.registerIndex)){
        op1_is_upper_reg = true;
    } else if(operand[1].type == OPERAND_R8 && is_ah_to_bh(operand[1].reg.registerIndex)){
        op2_is_upper_reg = true;
    }

    if(instruction->encoding != OP_ENC_FPU) emit_operand_overide_prefix(operand[0].type, instruction->op2);

    switch (instruction->encoding) {
        case OP_ENC_ZO:
            vex |= VEX_ONE_BYTE_DEFAULT;
            vex |= VEX_UNUSED_REG;
            break;                        
        case OP_ENC_I:
            vex |= VEX_ONE_BYTE_DEFAULT;
            vex |= VEX_UNUSED_REG;
            if(is_immediate(instruction->op1)){
                imm_index = 0;
            } else{
                imm_index = 1;
            }
            break; 
        case OP_ENC_MI:
            //fall through expected
            imm_index = 1;
        case OP_ENC_M1: // 1 imm is implicit
        case OP_ENC_MC: //cl operand is implicit
        case OP_ENC_M:
            vex |= VEX_UNUSED_REG;
            vex |= VEX_ONE_BYTE_DEFAULT;
            if(is_mem(operand[0].type)){
                vex ^= REX_MEM_TO_VEX(operand[0].mem.rex);
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset;
                is_rel = mem_is_rel(operand[0].mem);
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
            } else{
                set_b(&operand[0], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex;
                modrm_size = 1;
            }
            break;
        case OP_ENC_MRI:
            //fall through expected
            imm_index = 2;
        case OP_ENC_MRC: //cl is immplicit
        case OP_ENC_MR:
            set_r(&operand[1], &vex, &rex);
            vex |= VEX_UNUSED_REG;
            if(!is_mem(operand[0].type)){
                set_b(&operand[0], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |=(operand[1].reg.registerIndex << 3);
                modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex; 
                modrm_size = 1;
            } else{ 
                vex ^= REX_MEM_TO_VEX(operand[0].mem.rex);
                rex |= operand[0].mem.rex;
                addend = operand[0].mem.offset;
                is_rel = mem_is_rel(operand[0].mem);
                modrm_sib[MODRM_INDEX] |= (operand[1].reg.registerIndex << 3);
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
            }
            break;
        case OP_ENC_RMI:
            //fall through expected
            imm_index = 2;
        case OP_ENC_RM0: //zero indicates implicit xmm0 operand
        case OP_ENC_RM: 
            set_r(&operand[0], &vex, &rex);
            vex |= VEX_UNUSED_REG;
            modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex << 3; 
            if(!is_mem(operand[1].type)){
                set_b(&operand[1], &vex, &rex);
                modrm_size = 1;
                rex |= operand[1].reg.rex;
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].reg.registerIndex; 
            } else {
                vex ^= REX_MEM_TO_VEX(operand[1].mem.rex);
                rex |= operand[1].mem.rex;
                addend = operand[1].mem.offset;
                is_rel = mem_is_rel(operand[1].mem);
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl); 
            }
            break;  
        
        case OP_ENC_RVMR:
        case OP_ENC_RVMI:
            //fall through expected
            imm_index = 3;
        case OP_ENC_RVM:
            set_r(&operand[0], &vex, &rex);
            vex |= VEX_REGISTER(operand[1].reg.registerIndex);
            if(is_advanced_reg(operand[2].type) || is_reg32_or_64(operand[2].type)){
                set_b(&operand[2], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |=(operand[0].reg.registerIndex << 3);
                modrm_sib[MODRM_INDEX] |= operand[2].reg.registerIndex; 
                modrm_size = 1;
            } else{ 
                vex ^= REX_MEM_TO_VEX(operand[2].mem.rex);
                addend = operand[2].mem.offset; 
                is_rel = mem_is_rel(operand[2].mem);
                modrm_sib[MODRM_INDEX] |= (operand[0].reg.registerIndex << 3);
                modrm_size = modrm_sib_fields(&operand[2], modrm_sib, &lbl);
            }
            break;
        case OP_ENC_RMV: 
            set_r(&operand[0], &vex, &rex);
            vex |= VEX_REGISTER(operand[2].reg.registerIndex);
            modrm_sib[MODRM_INDEX] |= (operand[0].reg.registerIndex << 3);
            if(is_advanced_reg(operand[1].type) || is_reg32_or_64(operand[1].type) || operand[1].type == OPERAND_TMM){
                set_b(&operand[1], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].reg.registerIndex; 
                modrm_size = 1;
            } else{ 
                vex ^= REX_MEM_TO_VEX(operand[1].mem.rex);
                addend = operand[1].mem.offset; 
                is_rel = mem_is_rel(operand[1].mem);
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
            }
            break;
        case OP_ENC_VMI: 
            //fallthrough expected
            imm_index = 2;
        case OP_ENC_VM: 
            vex |= (uint8_t)VEX_ONE_BYTE_DEFAULT;
            vex |= VEX_REGISTER(operand[0].reg.registerIndex);
            if(is_advanced_reg(operand[1].type) || is_reg32_or_64(operand[1].type)){
                set_b(&operand[1], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[1].reg.registerIndex; 
                modrm_size = 1;
            } else{ 
                vex ^= REX_MEM_TO_VEX(operand[1].mem.rex);
                addend = operand[1].mem.offset;
                is_rel = mem_is_rel(operand[1].mem);
                modrm_size = modrm_sib_fields(&operand[1], modrm_sib, &lbl);
            } 
            break;
        case OP_ENC_MVR:
            set_r(&operand[2], &vex, &rex);
            vex |= VEX_REGISTER(operand[1].reg.registerIndex);
            modrm_sib[MODRM_INDEX] |= (operand[2].reg.registerIndex << 3);
            if(is_advanced_reg(operand[0].type) || is_reg32_or_64(operand[0].type)){
                set_b(&operand[0], &vex, &rex);
                modrm_sib[MODRM_INDEX] |= 192;
                modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex; 
                modrm_size = 1;
            } else{ 
                vex ^= REX_MEM_TO_VEX(operand[0].mem.rex);
                addend = operand[0].mem.offset; 
                is_rel = mem_is_rel(operand[0].mem);
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl);
            } 
            break;
        //add register to opcode
        case OP_ENC_OI:
            //fallthrough expected
            imm_index = 1;
        case OP_ENC_O: 
            if(is_extended_reg(operand[0].reg.registerIndex)){
                operand[0].reg.registerIndex -= 8;
                operand[0].reg.rex |= REX_B;
            }
            rex |= operand[0].reg.rex;
            opcode[instruction->size - 1] += operand[0].reg.registerIndex; 
            break;
        case OP_ENC_R:
            set_b(&operand[0], &vex, &rex);
            modrm_size = 1;
            modrm_sib[MODRM_INDEX] |= 192;
            modrm_sib[MODRM_INDEX] |= operand[0].reg.registerIndex;
            break;
        case OP_ENC_D:
          if(operand[0].type == OPERAND_L64){
                section_add_data(&program.text, opcode, instruction->size); 
                uint32_t zero = 0; 
                //add some temp zeros
                symbol_table_add_instance(operand[0].label, program.text.size, -DISPLACEMENT_SIZE,true); 
                section_add_data(&program.text, &zero, 4);
                return;
            } 
            break;
        case OP_ENC_FPU: 
            if(operand[0].type == OPERAND_STI){
                opcode[instruction->size - 1] += operand[0].fpu_stack_index; 
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
                is_rel = mem_is_rel(operand[0].mem);
                modrm_size = modrm_sib_fields(&operand[0], modrm_sib, &lbl); 
            }
            break;
        case OP_ENC_II:
            //the only instruction that has this encoding is enter
            section_add_data(&program.text, opcode, instruction->size); 
            section_add_data(&program.text, &operand[0].imm16, 2);
            section_add_data(&program.text, &operand[1].imm8, 1);
            return; 

        //moffsets
        case OP_ENC_FD:
            //fallthrough expected
            imm_index++;
        case OP_ENC_TD: 
            imm_index++;
            if(instruction->rex > 0x40){
                section_add_data(&program.text, (uint8_t*)&instruction->rex, 1);
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


    if(instruction->flags & INSTR_USES_REX){
        rex |= instruction->rex;
        if(rex > 0x40){
            if(op1_is_upper_reg){
                parser_error_loc(p, operand[0].line, 
                        operand[0].col, "Cannot use AH, BH, CH, or DH with rex prefix\n");
                return;
            } else if(op2_is_upper_reg){
                parser_error_loc(p, operand[1].line, 
                        operand[1].col, "Cannot use AH, BH, CH, or DH with rex prefix\n");
                return;
            }


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
    else if((instruction->flags & INSTR_USES_2VEX) && ((vex & 0xE000) == 0xE000)){
        vex |= instruction->three_vex;
        uint8_t tmp = 0xC5;
        section_add_data(&program.text, &tmp, 1); 
        section_add_data(&program.text, &vex, 1); 
        section_add_data(&program.text, opcode, instruction->size); 
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
        section_add_data(&program.text, opcode, instruction->size); 
    }

    if(modrm_size != 0) section_add_data(&program.text, modrm_sib, modrm_size);

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
                uint8_t payload = (operand[3].reg.registerIndex) << 4; 
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
        symbol_table_add_instance(lbl, lbl_displacement, addend, is_rel);
    }


}


static inline bool check_prefix(const Instruction* instruction, const uint16_t* prefix_array, int prefix_array_size){
    bool is_lock_instr = false;
    uint16_t instr_var_index = (instruction - INSTRUCTION_TABLE);
    for(int i = 0; i < prefix_array_size; i++){
        if(prefix_array[i] == instr_var_index){
            return true;
        }
    }
    return false;
}

// is_rep stores if if the prefix is the rep prefix
// rep uses the same encoding (0xf3) as repe so we use this bool 
static bool parse_instruction(Parser* p, InstructionPrefix prefix, bool is_rep){
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
            parser_error(p, "Expected comma or new line after operand got %s\n", 
                    token_to_string(p->currentToken.type));

            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }

    }

    const Instruction* instruction = NULL;
    if(operand_count == 0){
        instruction = find_instruction_nop(instr);
    } else if (operand_count == 1) { 
        instruction = find_instruction_one_operand(instr, &operands[0]);
    } else if (operand_count == 2) { 
        instruction = find_instruction_two_operands(instr, &operands[0], &operands[1]);
    } else if (operand_count == 3) {
        instruction = find_instruction_three_operands(instr, &operands[0], &operands[1], &operands[2]);
    } else{
        instruction = find_instruction_four_operands(instr,&operands[0],&operands[1], &operands[2], &operands[3]);
    }

    if(instruction == NULL){
        for(int i = 0; i < operand_count; i++){
            scratch_buffer_fmt("%s ", operand_to_string(operands[i].type));
        }
        char* temp = scratch_buffer_as_str();
        parser_error_loc(p,instr_line, instr_col, "Couldn't find instruction for nmemonic: %s %s\n", 
                get_keyword(instr)->name, temp); 
        scratch_buffer_clear();
        parser_expect_consume_token(p, TOK_NEW_LINE);
        return false;
    } 
    
    if(prefix == PREFIX_LOCK){
        if(!check_prefix(instruction, LOCK_PREFIX_INDICES, LOCK_PREFIX_TABLE_SIZE) || !is_mem(operands[0].type)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use Lock with this instruction\n"); 
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        }
        section_add_data(&program.text, &prefix, 1);
    } else if (prefix == PREFIX_REP && is_rep) {
        if(!check_prefix(instruction, REP_PREFIX_INDICES, REPE_PREFIX_TABLE_SIZE)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use Rep prefix with this instruction\n"); 
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        } 
        section_add_data(&program.text, &prefix, 1);
    } else if (prefix == PREFIX_REPE || prefix == PREFIX_REPNE) {
        if(!check_prefix(instruction, REPE_PREFIX_INDICES, REPE_PREFIX_TABLE_SIZE)){
            parser_error_loc(p,instr_line, instr_col, "Cannot use Repe/Repne prefix with this instruction\n"); 
            parser_expect_consume_token(p, TOK_NEW_LINE);
            return false;
        } 
        section_add_data(&program.text, &prefix, 1); 
    }

    emit_instruction(p, instruction, operands);
    parser_expect_consume_token(p, TOK_NEW_LINE);
    return true;
}


typedef enum {
    PREFIX_RESULT_NONE,
    PREFIX_RESULT_ERROR,
    PREFIX_RESULT_HANDLED,
} PrefixResult;

static PrefixResult instruction_handle_prefix(Parser* p){
    InstructionPrefix prefix = PREFIX_NONE;
    //need this to distinguish between rep/repe since they have the same opcode
    bool is_rep = false;
    if(p->currentToken.type == TOK_LOCK){
        prefix = PREFIX_LOCK;
    } else if (p->currentToken.type == TOK_REP) { 
        is_rep = true;
        prefix = PREFIX_REP;
    }else if (p->currentToken.type == TOK_REPE) { 
        prefix = PREFIX_REPE;
    } else if (p->currentToken.type == TOK_REPNE) {
        prefix = PREFIX_REPNE; 
    } else{
        return PREFIX_RESULT_NONE;
    }

    parser_next_token(p);
    if(!parser_expect_token(p, TOK_INSTRUCTION)){
        parser_expect_consume_token(p, TOK_NEW_LINE);
        return PREFIX_RESULT_ERROR;
    }

    if(parse_instruction(p, prefix, is_rep)){
        return PREFIX_RESULT_HANDLED;
    } 
    return PREFIX_RESULT_ERROR;
}

static void parse_text_section(Parser* p){
    while(p->currentToken.type != TOK_SECTION){

        if(instruction_handle_prefix(p) != PREFIX_RESULT_NONE){
            continue; 
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
            symbol_table_add(p, id.literal, 0, section, VISIBILITY_GLOBAL);
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
            symbol_table_add(p, id.literal, program.text.size, SECTION_TEXT, VISIBILITY_LOCAL);
        } 
        else if (p->currentToken.type == TOK_INSTRUCTION) {
            parse_instruction(p, PREFIX_NONE, false); 
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
            switch (instruction_handle_prefix(p)) {
                case PREFIX_RESULT_NONE:
                    if(!parser_expect_token(p, TOK_INSTRUCTION) || !parse_instruction(p, PREFIX_NONE, false)){
                        parser_expect_consume_token(p, TOK_NEW_LINE);
                        continue;
                    }
                    break;
                case PREFIX_RESULT_HANDLED:
                    break;
                case PREFIX_RESULT_ERROR:
                    continue; 
            }
 
            //since section_add_data uses memcpy we have to 
            //copy to a temp buffer
            uint8_t instr_opcode[15] = {0};
            int end = program.text.size;
            memcpy(instr_opcode, &program.text.data[start], end - start);

            for(uint32_t i = 0; i < amount - 1; i++){
                section_add_data(&program.text,instr_opcode, end-start);
            }
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
             fatal_error("Symbol %s used but never defined\n", e->name);
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
                 //assume size of 4   
                 uint64_t next_instruction = instance->offset - instance->addend;
                 uint64_t rip_addr = e->section_offset;
                 //not sure if this is supposed to be signed or unsigned
                 int32_t rel_addr = (int32_t)(rip_addr - next_instruction);
                 memcpy(&program.text.data[instance->offset], &rel_addr, 4);
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
