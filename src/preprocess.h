#pragma once
#include "util.h"

typedef struct {
    char* name;
    ArrayList tokens;
} PreprocessorSymbol;



typedef struct {
    char* name; 
    ArrayList args;
    ArrayList body;
} PreprocessorMacro;



#define NO_MACRO_PARAMS -1

typedef struct{
    struct PreprocessorCtx* next;
    int macro_index; //index into macros list if a macro has parameters
    ArrayList arg_values; //values of the parameters
    ArrayList body;
    int index; //index into body list
} PreprocessorCtx;



ArrayList preprocess_tokens(ArrayList* tokens);
