#pragma once 
#include "parser.h"
#include <stdint.h>



uint64_t parse_and_eval_expression(Parser* assembler);

uint64_t string_to_int(char* string);
