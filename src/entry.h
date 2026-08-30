#pragma once
#include <stdbool.h>


typedef enum {
    BASM_FILE_ELF = 0,
    BASM_FILE_PE = 1,
    BASM_FILE_MACHO = 2,
} BasmFileType;


typedef struct {
    const char* input_file;    
    const char* output_file;
    BasmFileType ftype; 
    bool debugSymbols;
} AssemblerFlags;



bool basm_assemble_program();

bool basm_parse_flags(int argc, char** argv);

void basm_help();
