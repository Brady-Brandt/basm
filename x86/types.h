#pragma once
#include <stdint.h>
#include "gentypes.h"

    
typedef enum{
    // allowed prefixes
    FLAG_REX = 1 << 2,
    FLAG_TWO_BYTE_VEX = 1 << 3,
    FLAG_THREE_BYTE_VEX = 1 << 4,
    FLAG_EVEX = 1 << 5,
    FLAG_LOCK = 1 << 6,
    FLAG_REP  = 1 << 7,
    FLAG_REPE  = 1 << 8,
    // 64 bit mode for rex
    // opcode specific for vex/evex
    FLAG_REX_W  = 1 << 9,
    FLAG_VEX_W  = FLAG_REX_W,
    FLAG_EVEX_W = FLAG_REX_W,
    // opcode extension
    FLAG_VEX_P0 = 1 << 10,
    FLAG_VEX_P1 = 1 << 11,
    FLAG_EVEX_P0 = FLAG_VEX_P0,
    FLAG_EVEX_P1 = FLAG_VEX_P1,
    // Length
    FLAG_VEX_L = 1 << 12,
    FLAG_EVEX_L0 = FLAG_VEX_L,
    FLAG_EVEX_L1 = 1 << 13,
    // Opcode Map
    FLAG_VEX_M0  = 1 << 14,
    FLAG_VEX_M1  = 1 << 15,
    FLAG_VEX_M2  = 1 << 16,
    FLAG_VEX_M3  = 1 << 17,
    FLAG_VEX_M4  = 1 << 18,
    FLAG_EVEX_M0 = FLAG_VEX_M0,
    FLAG_EVEX_M1 = FLAG_VEX_M1,
    FLAG_EVEX_M2 = FLAG_VEX_M2,
    // Opcode extension is reg part of modrm
    FLAG_OPCODE_EXTENSION = 1 << 19,
    FLAG_DIGIT0 = 1 << 20,
    FLAG_DIGIT1 = 1 << 21,
    FLAG_DIGIT2 = 1 << 22,
    FLAG_REQURIES_SIB = 1 << 23,
    // Instruction Mode Information
    FLAG_VALID_64_B0  = 1 << 24,
    FLAG_VALID_64_B1  = 1 << 25,
} InstructionFlags;

#define GET_INSTR_SIZE(flags)           ((flags & 0x3) + 1)
#define GET_REX_W(flags)                (((flags >> 9)  & 0x1)  << 3)
#define GET_VEX_OPCODE_MAP(flags)       (((flags >> 14) & 31))
#define GET_TWO_VEX_PAYLOAD(flags)      ((flags  >> 10) & 0x7)
#define GET_THREE_VEX_PAYLOAD(flags)    ((flags  >> 10) & 0x7) | ((flags >> 9 & 0x1) << 7)
#define GET_OP_DIGIT(flags)             (((flags >> 20) & 0x7)  << 3)



typedef struct{
    uint32_t op1;
    uint32_t op2;
    uint32_t op3;
    uint8_t bytes[4];
    uint32_t flags;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t encoding;
    uint8_t count;
} Instruction;


typedef enum  {
    PREFIX_NONE  =  0x00,
    PREFIX_LOCK  =  0xf0,
    PREFIX_REP   =  0xf3,
    PREFIX_REPE  =  0xf3,
    PREFIX_REPNE = 0xf2,
} InstructionPrefix;

struct Keyword {
    const char* name; 
    TokenType type; 
    uint16_t value;
};

const char* token_to_string(TokenType type);

const char* operand_to_string(OperandType type);

const struct Keyword* get_keyword(uint64_t index);

uint64_t keyword_get_index(const struct Keyword* kw);

extern void print_instruction(const Instruction* instr);

extern const Instruction INSTRUCTION_TABLE[];

extern const int KEYWORD_TABLE_SIZE;

extern const uint16_t LOCK_PREFIX_TABLE_SIZE;
extern const uint16_t REP_PREFIX_TABLE_SIZE;
extern const uint16_t REPE_PREFIX_TABLE_SIZE; 

extern const uint16_t LOCK_PREFIX_INDICES[];
extern const uint16_t REP_PREFIX_INDICES[];
extern const uint16_t REPE_PREFIX_INDICES[];
