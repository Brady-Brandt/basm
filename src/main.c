#include "entry.h"


int main(int argc, char** argv){
    if(!basm_parse_flags(argc, argv)){
        return 1;
    } 
    if(basm_assemble_program()){ 
        return 0; 
    }
    return 1;
}
