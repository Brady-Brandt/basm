#pragma once
#include "util.h"

typedef struct {
    char* name;
    bool is_active;
    int line_number;
    int col;
    ArrayList tokens;
} PreprocessorSymbol;



typedef struct {
    char* name; 
    bool is_active;
    int line_number;
    int col;
    ArrayList args;
    ArrayList body;
} PreprocessorMacro;



#define NO_MACRO_PARAMS -1

typedef enum{
    CTX_BUILTIN,
    CTX_ARG,
    CTX_MACRO,
    CTX_SYMBOL,
} PreprocessorCtxType;

typedef struct{
    struct PreprocessorCtx* next;
    bool has_args;
    PreprocessorCtxType type;
    int list_index;
    ArrayList arg_values; //values of the parameters
    ArrayList body;
    int index; //index into body list
} PreprocessorCtx;



ArrayList preprocess_tokens(ArrayList* tokens);
