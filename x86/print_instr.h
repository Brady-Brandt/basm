#include <stdint.h>
#include <stdio.h>
// going to be included in instruction.c
// nice debugging function
void print_instruction(const Instruction* instr){
    // finding the name of the nmemonic is hard because we don't know 
    // were in the hashtable the nmemonic lies 
    // because of this we kind of just have to brute force linear search
    for(int i = 0; i < MAX_HASH_VALUE; i++){
        if(KEYWORD_TABLE[i].type != TOK_INSTRUCTION)
            continue;
        
        int variant_table_index = KEYWORD_TABLE[i].value;
        int start = variant_table_index;
        int end = variant_table_index + INSTRUCTION_TABLE[start].count;
        if(INSTRUCTION_TABLE[start].count != instr->count)
            continue;

        for(int j = start; j < end; j++){
            Instruction instr_variant = INSTRUCTION_TABLE[j];
            if(instr_variant.encoding != instr->encoding || instr_variant.flags != instr->flags)
                continue;
            if(instr->bytes[0] != instr_variant.bytes[0] || instr->bytes[1] != instr_variant.bytes[1])
                continue;
            if(instr->bytes[2] != instr_variant.bytes[2] || instr->bytes[3] != instr_variant.bytes[3])
                continue;
 
            printf("%s: ",KEYWORD_TABLE[i].name);
            goto e_loop;
        }
    }
e_loop:
    for(uint32_t i = 0; i < (instr->flags & 0x3) + 1; i++)
        printf("%02x ", instr->bytes[i]);
    printf("\nOperand 1: %s\n", operand_to_string(instr->op1));
    printf("Operand 2: %s\n", operand_to_string(instr->op2));
    printf("Operand 3: %s\n", operand_to_string(instr->op3));
    printf("Operand Encoding: %i\n", instr->encoding);
    // TODO: PRINT OUT EACH INDIVIDUAL FLAG
    printf("Flags: 0x%x\n", instr->flags);
}
