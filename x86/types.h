#pragma once
#include <stdint.h>
#include "gentypes.h"


typedef struct {
    union {
        uint16_t three_vex;
        uint8_t two_vex;
        uint8_t rex;
        uint8_t variant_count;
    };
    uint32_t op1;
    uint32_t op2;
    uint32_t op3;
    uint8_t bytes[4];
    uint8_t size;
    int8_t digit;
    uint8_t encoding;
    uint8_t flags;
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
