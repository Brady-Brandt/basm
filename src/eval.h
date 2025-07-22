#pragma once 
#include "parser.h"
#include <stdbool.h>
#include <stdint.h>


bool parse_and_eval_expression(Parser* assembler, int64_t* result);

bool string_to_int(char* string, uint64_t* result);
