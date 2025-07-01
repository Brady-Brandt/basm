#pragma once
#include "parser.h"

typedef struct {
    char* name;
    //indices into token arraylist
    int starti; 
    int endi;
} PreprocessorSymbol;



typedef struct {
    char* name; 
    ArrayList args;
    int starti;
    int endi;
} PreprocessorMacro;



#define NO_MACRO_PARAMS -1

typedef struct{
    struct PreprocessorCtx* next;
    int macro_index;
    ArrayList arg_values;
    int starti;
    int endi;
    int index;
} PreprocessorCtx;


typedef struct {
    Parser* p;
    ArrayList* macros;
    ArrayList* symbols;
    PreprocessorCtx* stack;
    Token currentToken;
    int index;;
} Preprocessor;


ArrayList preprocess_tokens(ArrayList* tokens);
