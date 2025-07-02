#pragma once
#include "util.h"
#include "entry.h"

#define SECTION_EXTERN 0
#define SECTION_TEXT 1 
#define SECTION_DATA 2 
#define SECTION_BSS 3 

#define SECTION_UNDEFINED 255

#define VISIBILITY_LOCAL 0
#define VISIBILITY_GLOBAL 1

#define VISIBILITY_UNDEFINED 255

#define MAX_OFFSET 18446744073709551615ULL



//SYMBOLS ARE ONLY VALID IN THE TEXT SECTION FOR NOW 
typedef struct {
    uint64_t offset; 
    bool is_relative;
}SymbolInstance;


typedef struct {
    char* name;
    uint8_t section;
    uint8_t visibility;
    uint64_t section_offset;
    ArrayList instances; 
} SymbolTableEntry;


//TODO: IMPLEMENT AN ACTUAL HASHMAP 
typedef struct {
    ArrayList symbols;
} SymbolTable;


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
    AssemblerFlags flags;
    int ret_code;
} Program;


extern Program program;

bool write_elf(const char* input_file, const char* output_file, Program *p);

bool write_pe(const char* input_file, const char* output_file, Program* p);
