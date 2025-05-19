#pragma once
#include <stdbool.h>


typedef enum {
    BASM_FILE_ELF = 0,
    BASM_FILE_PE = 1,
} BasmFileType;


typedef struct {
    const char* input_file;    
    const char* output_file;
    BasmFileType ftype; 
} AssemblerFlags;



bool basm_assemble_program(AssemblerFlags* flags);

bool basm_parse_flags(AssemblerFlags* flags, int argc, char** argv);

void basm_help();
