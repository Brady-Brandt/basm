#include <stdio.h>
// going to be included in instruction.c
// nice debugging function
void print_instruction(const Instruction* instr){
    // finding the name of the nmemonic is hard because we don't know 
    // were in the hashtable the nmemonic lies 
    // because of this we kind of just have to brute force linear search
    for(int i = 0; i < MAX_HASH_VALUE; i++){
        if(KEYWORD_TABLE[i].type == TOK_INSTRUCTION){
            int variant_table_index = KEYWORD_TABLE[i].value;
            int start = variant_table_index + 1;
            for(int j = start; j < start + INSTRUCTION_TABLE[variant_table_index].variant_count; j++){
                Instruction instr_variant = INSTRUCTION_TABLE[j];
                if(instr->size == instr_variant.size && instr->three_vex == instr_variant.three_vex && instr->digit == instr_variant.digit){
                    int matches = 1;
                    for(int k = 0; k < instr->size; k++){
                        if(instr->bytes[k] != instr_variant.bytes[k]){
                            matches = 0; 
                            break;
                        } 
                    }
                    if(matches){
                        printf("%s: ",KEYWORD_TABLE[i].name);
                        goto e_loop;
                    }
                }
            }
        }
    }
e_loop:
    for(int i = 0; i < instr->size; i++){ 
        printf("%02x ", instr->bytes[i]);
    }
    printf("\nOperand 1: %s\n", operand_to_string(instr->op1));
    printf("Operand 2: %s\n", operand_to_string(instr->op2));
    printf("Operand 3: %s\n", operand_to_string(instr->op3));
    printf("Opcode Extension: %i\n", instr->digit);
    printf("Operand Encoding: %i\n", instr->encoding);
    if(instr->flags & INSTR_USES_REX) printf("Rex: 0x%x\n", instr->rex);
    else if(instr->flags & INSTR_USES_2VEX) printf("2 byte Vex: 0x%x\n", instr->two_vex);
    else if(instr->flags & INSTR_USES_3VEX) printf("3 byte Vex: 0x%x\n", instr->three_vex);
}
