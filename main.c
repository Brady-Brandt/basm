#include "entry.h"


int main(int argc, char** argv){
    AssemblerFlags flags = {0};
    if(!basm_parse_flags(&flags, argc, argv)){
        return 1;
    } 
    basm_assemble_program(&flags);
}
