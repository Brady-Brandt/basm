/* 
 * As of right now this is not a comprehensive, fool proof test suite.
 * However it has been very beneficial for making changes and verifying correctness for most instructions.
 * The basic idea here is that we generate assembly for every instruction variant.
 * Then run the outputted assembly through basm to generate an object file.
 * Then run the generated object file through objdump and compare the output with our original assembly.
 * This is by no means perfect, but it can actually succesfully verify a large number of instructions.
 * All instructions that can not be auto-verified will be outputted into a file tests/output.txt
 * Using something like INTEL XED would be more ideal
 *
 *
 * Depending on the rng seed, there is somewhere around 300 out of 2100 cases that fail auto-verification
 * Most of these cases are false positives that fall into one of these categories
 * 1. Bit shifting/rotating instructions
 *    When shifting/rotating by 1, I accept the 1 an implicit operand. So
 *    sal rax == sal rax, 1
 *    Objdump will print out this implicit 1 and so the verification will fail
 *
 * 2. Implicit Operands 
 *    Very similiar to the first issue there are a lot of instructions that have operands that are 
 *    defined within the opcode and are therefore implicit. Once again objdump will print out these implicit
 *    operands. This is very common with FPU instructions where the top of the FPU stack will be an implicit 
 *    operand.
 *
 * 3. Memory Addresses
 *    An optimization that I make is converting [rax*2] -> [rax + rax]. This ends up saving 4 bytes in the output.
 *    Even though [rax*2] == [rax + rax] for pretty much all pratical purposes, this test suite will still produce
 *    a false positive.
 */


#include "x86/types.h"
#include "constants.h"
#include "src/util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_TESTS 10000

// tweak this number to only test a few instructions
#define TOTAL_TESTS MAX_TESTS

const char* input_file = "tests/test.asm";
const char* output_file = "tests/output.txt";
#ifdef _WIN32
    const char *basm_cmd =
        "bin\\basm.exe -f elf tests\\test.asm -o tests\\btest.o";
    const char *objdump_cmd =
        "objdump.exe -M intel -d --no-show-raw-insn --no-addresses tests\\btest.o";
#else
    const char *basm_cmd =
        "bin/basm -f elf tests/test.asm -o tests/btest.o";
    const char *objdump_cmd =
        "objdump -M intel -d --no-show-raw-insn --no-addresses tests/btest.o";
#endif



const char* TEST_LABEL = "lbl";
const char* TEST_MEM = "data";

//temporary buffers
#define BUF_SIZE 256 
static char BUFFER1[BUF_SIZE] = {0};
static char BUFFER2[BUF_SIZE] = {0};
static char BUFFER3[BUF_SIZE] = {0};

static inline int rand_in_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

// returns a memory address operand
const char* get_mem(char* buffer, const char* size){
    memset(buffer, 0, BUF_SIZE);
    int index = rand_in_range(0, sizeof(MEMORY_ADDRESSES) / sizeof(MEMORY_ADDRESSES[0]) - 1);
    if(strcmp(size, "") == 0)
        snprintf(buffer, BUF_SIZE - 1, "%s",MEMORY_ADDRESSES[index]);
    else
        snprintf(buffer, BUF_SIZE - 1, "%s %s",size, MEMORY_ADDRESSES[index]);
    return buffer; 
}


//returns a register or memory address operand
const char* get_rm(char* buffer, const char* size, int min, int max){
    int is_register = rand_in_range(0, 1);
    if(is_register)
        return registers[rand_in_range(min, max)];
    
    return get_mem(buffer, size);
}

const char* operand_to_value(FILE* output_stream, const char* instr_name, char* buffer, OperandType opt){
    switch (opt) {
        case OPERAND_NOP:
            return NULL;
            break;
        case OPERAND_REL8:
            break;
        case OPERAND_REL16:
            break;
        case OPERAND_REL32:
            return TEST_LABEL;
            break;
        case OPERAND_FS:
            return "fs";
        case OPERAND_GS:
            return "gs";
        case OPERAND_CR8:
            return "cr8";
        case OPERAND_R8:
            return registers[rand_in_range(REG_R8B, REG_R15B)];
        case OPERAND_R16:
            return registers[rand_in_range(REG_AX, REG_R15W)];
        case OPERAND_R32:
            return registers[rand_in_range(REG_EAX, REG_R15D)];
        case OPERAND_R64:
            return registers[rand_in_range(REG_RAX, REG_R15)];
        case OPERAND_R32R64:
            return registers[rand_in_range(REG_RAX, REG_R15D)];
        case OPERAND_SREG:
            return sregs[rand_in_range(0, 5)];
        case OPERAND_MEM_ANY:
            return get_mem(buffer,"");
        case OPERAND_M8:
            return get_mem(buffer,"byte");
        case OPERAND_M16:
            return get_mem(buffer,"word");
        case OPERAND_M32:
            return get_mem(buffer,"dword");
        case OPERAND_M64:
            return get_mem(buffer,"qword");
        case OPERAND_M128:
            return get_mem(buffer,"dqword");
        case OPERAND_M256:
            return get_mem(buffer,"yword");
        case OPERAND_M512:
            return get_mem(buffer,"");
        case OPERAND_M80:
            return get_mem(buffer,"tword");
        case OPERAND_RM8:
            return get_rm(buffer,"byte", REG_R8B, REG_R15B);
        case OPERAND_RM16:
            return get_rm(buffer,"word",REG_AX, REG_R15W);
        case OPERAND_RM32:
            return get_rm(buffer,"dword",REG_EAX, REG_R15D);
        case OPERAND_RM64:
            return get_rm(buffer,"qword",REG_RAX, REG_R15);
        case OPERAND_R32M8:
            return get_rm(buffer,"byte",REG_EAX, REG_R15D);
        case OPERAND_R32M16:
            return get_rm(buffer,"word",REG_EAX, REG_R15D);
        case OPERAND_R32R64M8:
            return get_rm(buffer,"byte",REG_RAX, REG_R15D);
        case OPERAND_R32R64M16:
            return get_rm(buffer,"word",REG_RAX, REG_R15D);
        case OPERAND_R32R64M32:
            return get_rm(buffer,"dword",REG_RAX, REG_R15D);
        case OPERAND_SIMM8:
        case OPERAND_IMM8:
            snprintf(buffer, BUF_SIZE - 1, "0x%x%c", rand() & 0xFF, 0);
            return buffer; 
        case OPERAND_SIMM16:
        case OPERAND_IMM16:
            snprintf(buffer, BUF_SIZE - 1, "0x%x%c", rand() & 0xFFFF, 0);
            return buffer; 
        case OPERAND_SIMM32:
        case OPERAND_IMM32:
            snprintf(buffer, BUF_SIZE - 1, "0x%x%c", rand() & 0xFFFFFF, 0);
            return buffer; 
        case OPERAND_SIMM64:
        case OPERAND_IMM64:
            snprintf(buffer, BUF_SIZE - 1, "0x%lx%c", rand() & 0xFFFFFFFFFFFF, 0);
            return buffer;
        case OPERAND_LABEL:
            break;
        case OPERAND_M:
            return get_mem(buffer, "");
        case OPERAND_STI:
            return fpu_regs[rand_in_range(0, 7)];
        case OPERAND_MMM32:
            return get_rm(buffer,"dword",REG_MM0, REG_MM7);
        case OPERAND_MMM64:
            return get_rm(buffer,"qword",REG_MM0, REG_MM7);
        case OPERAND_MM:
            return registers[rand_in_range(REG_MM0, REG_MM7)];
        case OPERAND_XMM:
            return registers[rand_in_range(REG_XMM0, REG_XMM15)];
        case OPERAND_YMM:
            return registers[rand_in_range(REG_YMM0, REG_YMM15)];
        case OPERAND_XMMM8:
            return get_rm(buffer,"byte",REG_XMM0, REG_XMM15);
        case OPERAND_XMMM16:
            return get_rm(buffer,"word",REG_XMM0, REG_XMM15);
        case OPERAND_XMMM32:
            return get_rm(buffer,"dword",REG_XMM0, REG_XMM15);
        case OPERAND_XMMM64:
            return get_rm(buffer,"qword",REG_XMM0, REG_XMM15);
        case OPERAND_XMMM128:
            return get_rm(buffer,"dqword", REG_XMM0, REG_XMM15);
        case OPERAND_YMMM256:
            return get_rm(buffer,"yword", REG_YMM0, REG_YMM15);
        case OPERAND_BND:
            return bndregs[rand_in_range(0, 3)];
        case OPERAND_CREG:
            return cregs[rand_in_range(0, 7)];
        case OPERAND_DREG:
            return dregs[rand_in_range(0, 7)];
        case OPERAND_BNDM128:
            if(rand_in_range(0, 1))
                return bndregs[rand_in_range(0, 3)]; 
            return get_mem(buffer, "dqword");
        case OPERAND_TMM: {
            //tmm instructions don't allow the same register to be used twice
            static int count = 0;
            if(count == 7) count = 0;
            return tmmregs[count++];
        }
        case OPERAND_MOFFSET:
            return "moffset 0x400000";
        case OPERAND_AL:
            return "al";
        case OPERAND_CL:
            return "cl";
        case OPERAND_AX:
            return "ax";
        case OPERAND_DX:
            return "dx";
        case OPERAND_EAX:
            return "eax";
        case OPERAND_RAX:
            return "rax";
        case OPERAND_UNSUPPORTED:
            break; 
    }

    fprintf(output_stream,"Operand %s not supported yet for %s\n", operand_to_string(opt), instr_name);
    return NULL;

}




typedef struct {
    const char *alias;
    const char *canonical;
} AliasMap;

static const AliasMap alias_table[] = {
    {"setz",   "sete"},
    {"sete",   "sete"},
    {"setnz",  "setne"},
    {"setne",  "setne"},
    {"setb",   "setb"},
    {"setnae", "setb"},
    {"setc",   "setb"},
    {"setae",  "setae"},
    {"setnb",  "setae"},
    {"setnc",  "setae"},
    {"setbe",  "setbe"},
    {"setna",  "setbe"},
    {"seta",   "seta"},
    {"setnbe", "seta"},
    {"setl",   "setl"},
    {"setnge", "setl"},
    {"setge",  "setge"},
    {"setnl",  "setge"},
    {"setle",  "setle"},
    {"setng",  "setle"},
    {"setg",   "setg"},
    {"setnle", "setg"},
    {"seto",   "seto"},
    {"setno",  "setno"},
    {"sets",   "sets"},
    {"setns",  "setns"},
    {"setp",   "setp"},
    {"setpe",  "setp"},
    {"setpo",  "setpo"},
    {"setnp",  "setpo"},
    {"cmovz",   "cmove"},
    {"cmove",   "cmove"},
    {"cmovnz",  "cmovne"},
    {"cmovne",  "cmovne"},
    {"cmovb",   "cmovb"},
    {"cmovnae", "cmovb"},
    {"cmovc",   "cmovb"},
    {"cmovae",  "cmovae"},
    {"cmovnb",  "cmovae"},
    {"cmovnc",  "cmovae"},
    {"cmovbe",  "cmovbe"},
    {"cmovna",  "cmovbe"},
    {"cmova",   "cmova"},
    {"cmovnbe", "cmova"},
    {"cmovl",   "cmovl"},
    {"cmovnge", "cmovl"},
    {"cmovge",  "cmovge"},
    {"cmovnl",  "cmovge"},
    {"cmovle",  "cmovle"},
    {"cmovng",  "cmovle"},
    {"cmovg",   "cmovg"},
    {"cmovnle", "cmovg"},
    {"cmovo",   "cmovo"},
    {"cmovno",  "cmovno"},
    {"cmovs",   "cmovs"},
    {"cmovns",  "cmovns"},
    {"cmovp",   "cmovp"},
    {"cmovpe",  "cmovp"},
    {"cmovpo",  "cmovpo"},
    {"cmovnp",  "cmovpo"},

    {"ja",    "ja"},
    {"jnbe",  "ja"},

    {"jae",   "jae"},
    {"jnb",   "jae"},
    {"jnc",   "jae"},

    {"jb",    "jb"},
    {"jnae",  "jb"},
    {"jc",    "jb"},

    {"jbe",   "jbe"},
    {"jna",   "jbe"},

    {"jcxz",  "jcxz"},
    {"jecxz", "jcxz"},
    {"jrcxz", "jcxz"},

    {"je",    "je"},
    {"jz",    "je"},

    {"jne",   "jne"},
    {"jnz",   "jne"},

    {"jg",    "jg"},
    {"jnle",  "jg"},

    {"jge",   "jge"},
    {"jnl",   "jge"},

    {"jl",    "jl"},
    {"jnge",  "jl"},

    {"jle",   "jle"},
    {"jng",   "jle"},

    {"jo",    "jo"},
    {"jno",   "jno"},

    {"js",    "js"},
    {"jns",   "jns"},

    {"jp",    "jp"},
    {"jpe",   "jp"},

    {"jpo",   "jpo"},
    {"jnp",   "jpo"},
};


static const char* normalize_instr(const char *instr) {
    size_t num_aliases = sizeof(alias_table) / sizeof(alias_table[0]);
    for (size_t i = 0; i < num_aliases; i++) {
        if (strcmp(instr, alias_table[i].alias) == 0)
            return alias_table[i].canonical;
    }
    return instr;
}


static char* get_literal(FileBuffer* fb, bool should_clear){
    if(should_clear) scratch_buffer_clear();
    while(true){
        char next = file_buffer_peek_char(fb);
        if(!isalnum(next) && next != '_' && next != '.' && next != '(' && next != ')' && next != ':')
            break;
        char c = file_buffer_get_char(fb);
        scratch_buffer_append_char(c); 
    }
    return scratch_buffer_as_str();
}

static inline void skip_spaces(FileBuffer* fb){
    while(file_buffer_peek_char(fb) == ' ' || file_buffer_peek_char(fb) == '{')
        file_buffer_get_char(fb);
}

static inline char* get_memory(FileBuffer* fb){
    scratch_buffer_clear();
    while(true){
        char c = file_buffer_get_char(fb);
        scratch_buffer_append_char(c);
        if(c == ']') break;
    }
    return scratch_buffer_as_str();
}


static int go_to_first_instruction(FileBuffer* fb, const char* start_label){
    int line_count = 1;
    //loop until start
    for (;;) {
        char c = file_buffer_get_char(fb);
        if(c == '_'){ 
            scratch_buffer_clear();
            scratch_buffer_append_char(c);
            if(strcmp(get_literal(fb, false), start_label) == 0){
                //go to the first line of instructions
                while(true){
                    c = file_buffer_get_char(fb);
                    if(c == '\n'){
                        line_count++;
                        break;
                    } 
                }
                return line_count;
            }
        } else if (c == '\n') {
            line_count++;
        
        } 
    }
}


static const char* convert_mem_size(char* mem_size){
    if (strcmp("YMMWORD", mem_size) == 0) return "yword ";  
    else if(strcmp("XMMWORD", mem_size) == 0) return "dqword "; 
    else if (strcmp("TBYTE", mem_size) == 0) return "tword "; 
    else if (strcmp("QWORD", mem_size) == 0) return "qword "; 
    else if (strcmp("DWORD", mem_size) == 0) return "dword "; 
    else if (strcmp("WORD", mem_size) == 0) return "word "; 
    else if (strcmp("BYTE", mem_size) == 0) return "byte "; 
    else{
        return mem_size;
    }
}

static char* get_line_from_objdump(FileBuffer* fb){
    memset(BUFFER1, 0, BUF_SIZE);

    int line_size = 0;
    while(!file_buffer_eof(fb)){
        while(isspace(file_buffer_peek_char(fb)))
            file_buffer_get_char(fb);

        char* instruction = NULL;
        if(file_buffer_peek_char(fb) == '{'){
            //skip over {vex}
            while(file_buffer_get_char(fb) != '}');
            skip_spaces(fb);
            instruction = get_literal(fb, true);
        } else{
            instruction = get_literal(fb, true);
            /*
            if(strcmp(instruction, "data16") == 0){
               skip_spaces(fb); 
               instruction = get_literal(fb, true);
            }
            */
        }
        line_size = snprintf(BUFFER1, BUF_SIZE - 1, "%s ",instruction);
        for(;;){
            skip_spaces(fb);
            
            // jmp <_start> instruction or something similiar
            if(file_buffer_peek_char(fb) == '<'){
                while(file_buffer_get_char(fb) != '\n');
                break;
            }

            if(file_buffer_peek_char(fb) == '\n'){
                file_buffer_get_char(fb);
                break;
            } 

            char* operand = NULL;
            if(file_buffer_peek_char(fb) == '[')
                operand = get_memory(fb); 
            else
                operand = get_literal(fb, true);

            if(operand == NULL){
                fprintf(stderr, "Operand for %s is NULL\n", instruction);
                exit(1);
            } else{
                if(strcmp(operand, "PTR") == 0) continue;
                line_size += snprintf(BUFFER1 + line_size, BUF_SIZE - line_size - 1,
                        "%s",convert_mem_size(operand)); 
            }

            skip_spaces(fb);


            // end of the current instruction/line
            if(file_buffer_peek_char(fb) == '\n'){
                file_buffer_get_char(fb);
                break;
            }
            // have another operand
            if(file_buffer_peek_char(fb) == ','){
                line_size += snprintf(BUFFER1 + line_size, BUF_SIZE - line_size - 1, ", "); 
                file_buffer_get_char(fb);
            }
        }
        return BUFFER1;
    }
    return BUFFER1;
}

static char* get_line_from_input_file(FileBuffer* fb, int* variant){
    int line_size = 0;
    while(!file_buffer_eof(fb)){
        skip_spaces(fb);
        char* instruction = get_literal(fb, true);
        line_size = snprintf(BUFFER2, BUF_SIZE - 1, "%s ",instruction); 
        for(;;){
            skip_spaces(fb);
            
            if(file_buffer_peek_char(fb) == '\n'){
                file_buffer_get_char(fb);
                break;
            } 

            if(file_buffer_peek_char(fb) == ';') goto get_variant;

            char* operand = NULL;
            if(file_buffer_peek_char(fb) == '['){
                line_size += snprintf(BUFFER2 + line_size, BUF_SIZE - line_size - 1, " "); 
                operand = get_memory(fb); 
            } else{  
                operand = get_literal(fb, true);
            }
            if(operand == NULL){
                fprintf(stderr, "Operand is NULL\n");
                exit(1);
            } else{
                line_size += snprintf(BUFFER2 + line_size, BUF_SIZE - line_size - 1, "%s",operand); 
            }

            skip_spaces(fb);

            if(file_buffer_peek_char(fb) == ','){
                line_size += snprintf(BUFFER2 + line_size, BUF_SIZE - line_size - 1, ", "); 
                file_buffer_get_char(fb);
            }

            get_variant:
            if(file_buffer_peek_char(fb) == ';'){
                file_buffer_get_char(fb);
                scratch_buffer_clear();
                while(true){
                    char c = file_buffer_get_char(fb);
                    if(c == '\n'){
                        (*variant) = strtol(scratch_buffer_as_str(), NULL, 10);
                        return BUFFER2;
                    }
                    scratch_buffer_append_char(c);
                }
            }

            if(file_buffer_peek_char(fb) == '\n'){
                file_buffer_get_char(fb);
                break;
            }
        }
        return BUFFER2;
    }
    return BUFFER2;
}


int strcmp_ignore_whitespace(const char *s1, const char *s2) {
    while (*s1 != '\0' || *s2 != '\0') {
        // Skip whitespace in s1
        while (*s1 && isspace((unsigned char)*s1)) s1++;

        // Skip whitespace in s2
        while (*s2 && isspace((unsigned char)*s2)) s2++;

        //remove +0x0 in memory addresses
        if(*s2 == '+' && *(s2 + 1) == '0'){
           s2+=4; 
        }

        // If characters differ, return 0
        if (*s1 != *s2) return 0;

        // Advance if not at end
        if (*s1) s1++;
        if (*s2) s2++;
    }

    return 1; // Strings match (ignoring whitespace)
}

char *dup_n(const char *s, size_t n){
    char *copy = malloc(n + 1);
    if (copy == NULL){
        fprintf(stderr, "FAILED TO ALLOCATE MEMORY\n");
        return NULL;
    }

    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

static bool strcmp_check_alias(const char* s1, const char* s2){
    // get the instruction
    size_t instr1_size = 0;
    for(;instr1_size < strlen(s1); instr1_size++){
       if(s1[instr1_size] == ' ') break;
    }  

    char* new_instr1 = dup_n(s1, instr1_size);

    const char* normal_instr1 = normalize_instr(new_instr1);

    size_t instr2_size = 0;
    for(;instr2_size < strlen(s2); instr2_size++){
       if(s2[instr2_size] == ' ') break;
    }

    char* new_instr2 = dup_n(s2, instr2_size);
    const char* normal_instr2 = normalize_instr(new_instr2);
 
    
    if(strcmp(normal_instr1, normal_instr2) != 0){
        free(new_instr1);
        free(new_instr2);
        return false;
    }

    //if the instructions are aliases then we 
    //check to see if the rest of the operands match
    s1 += instr1_size + 1;
    s2 += instr2_size + 1;


    free(new_instr1);
    free(new_instr2);
    return strcmp_ignore_whitespace(s1, s2);
}

// these instructions currently do no work yet
// they will produce errors when assembling
// want to ignore them for now and test everything else
const char* currently_not_working[] = {
    "SGDT",
    "SIDT",
    "INVLPG",
    "HRESET",
    "CMPXCHG8B", //issue with REX.W
    "FWAIT", "WAIT" //same instruction objdump sometimes doesn't recognize it
};


static bool skip_instruction(const char* name){
    for(size_t i = 0; i < sizeof(currently_not_working) / sizeof(currently_not_working[0]); i++){
        if(strcmp(currently_not_working[i],name) == 0)
            return true;
    }
    return false;
} 


typedef struct{
    int line;
    char* asmLine;
    char* objLine;
} ErrorLine;

void new_error_line(ArrayList* lines, int line, char* asm_line, char* obj_line){
    char* asmLine = strdup(asm_line);
    char* objLine = strdup(obj_line);
    ErrorLine l = {line,asmLine, objLine};
    array_list_append((*lines), ErrorLine, l);
}

int compare_error_lines(const void *a, const void *b){
    const ErrorLine *ea = a;
    const ErrorLine *eb = b;
    return strcmp(ea->asmLine, eb->asmLine);
}

int main(){
    srand(69); 
 
    FILE* assembly_file = fopen(input_file, "w");
    if(assembly_file == NULL){
        printf("Failed to open %s\n", input_file);
        return 1;
    }


    FILE* error_file = fopen(output_file, "w");
    if(error_file  == NULL){
        printf("Failed to open %s\n", output_file);
        return 1;
    }

 
    fprintf(error_file, "--------------------------\n");
    fprintf(error_file, "ASSEMBLY GENERATION ERRORS\n");
    fprintf(error_file, "--------------------------\n");


    // Generate Psuedorandom assembly
    fprintf(assembly_file, "section .bss\n");
    fprintf(assembly_file, "data: resb 1000\n");
    fprintf(assembly_file, "section .text\n");
    fprintf(assembly_file, "global _start\n");
    fprintf(assembly_file, "%s:\n", TEST_LABEL);
    fprintf(assembly_file, "_start:\n");

    int tests = TOTAL_TESTS;
    int instructions_tested = 0;
    int instruction_variants_tested = 0;
    int total_variants = 0;
    int total_instructions = 0;
    
    for(int i = 0; i < KEYWORD_TABLE_SIZE; i++){
        const struct Keyword* kw = get_keyword(i);
        if(kw->type != TOK_INSTRUCTION) continue;

        total_instructions++;
        total_variants += INSTRUCTION_TABLE[kw->value].count;

        if(skip_instruction(kw->name))
            continue;
 
        int variant_count = INSTRUCTION_TABLE[kw->value].count;
        int start = kw->value;

        //convert the name to lowercase
        char name[64];
        size_t kw_len = strlen(kw->name);
        name[kw_len] = 0;
        for(size_t i = 0; i < kw_len; i++)
            name[i] = tolower(kw->name[i]);
        
        for(int j = start; j < start + variant_count; j++){
            const Instruction variant = INSTRUCTION_TABLE[j];
            if(variant.op1 == OPERAND_REL8 || variant.op1 == OPERAND_REL16 || variant.op1 == OPERAND_REL32){
                fprintf(error_file, "Not checking relative jumps for %s\n", kw->name);
                continue;
            }

            if(variant.op1 == OPERAND_NOP)
                fprintf(assembly_file, "    %s ;%d\n", name,j);
            else if (variant.op2 == OPERAND_NOP) {
                const char* op_str = operand_to_value(error_file, name, BUFFER1,variant.op1);
                if(op_str == NULL)
                    continue;
                fprintf(assembly_file, "    %s %s;%d\n", name, op_str, j);
            } else if (variant.op3 == OPERAND_NOP) {
                const char* op1_str = operand_to_value(error_file, name, BUFFER1, variant.op1);
                if(op1_str == NULL)
                    continue;
                const char* op2_str = operand_to_value(error_file, name, BUFFER2, variant.op2);
                if(op2_str == NULL)
                    continue;
                fprintf(assembly_file, "    %s %s, %s;%d\n", name, op1_str, op2_str, j);
            }else {
                const char* op1_str = operand_to_value(error_file, name, BUFFER1, variant.op1);
                if(op1_str == NULL)
                    continue;
                const char* op2_str = operand_to_value(error_file, name, BUFFER2, variant.op2);
                if(op2_str == NULL)
                    continue;
                const char* op3_str = operand_to_value(error_file, name, BUFFER3, variant.op3);
                if(op3_str == NULL)
                    continue;

                // four operand instruction
                if(variant.encoding == OP_ENC_RVMI || variant.encoding == OP_ENC_RVMR){
                    fprintf(assembly_file, "    %s %s, %s, %s, ", name, op1_str, op2_str, op3_str);
                    OperandType opt = OPERAND_IMM8;
                    if(variant.encoding == OP_ENC_RVMR)
                        opt = variant.op1;
                    const char* op4_str = operand_to_value(error_file, name, BUFFER3, opt);
                    fprintf(assembly_file, "%s;%d\n", op4_str, j);
                } else
                    fprintf(assembly_file, "    %s %s, %s, %s;%d\n", name, op1_str, op2_str, op3_str, j);
            } 
            instruction_variants_tested++;
        }
        instructions_tested++;
        if(instructions_tested == tests) break;
    }
   
    printf("Generated %d out of %d instructions\n", instructions_tested, total_instructions);
    printf("Generated %d out of %d instruction variants\n", instruction_variants_tested, total_variants);
     
    fclose(assembly_file);
    
    printf("Command: %s\n", basm_cmd);

    FILE* basm_stream = popen(basm_cmd, "r");
    if(basm_stream ==  NULL){
        printf("Failed to execute %s\n", basm_cmd);
        return 1;
    }

    //reading any error messages
    memset(BUFFER1, 0, BUF_SIZE);
    while(fgets(BUFFER1, BUF_SIZE - 1, basm_stream) != NULL) {
        fprintf(stderr,"%s", BUFFER1);
        memset(BUFFER1, 0, BUF_SIZE);
    }

    if(pclose(basm_stream) != 0){
        fprintf(stderr, "Assembly Failed terminating tests\n");
        return 1;
    }


    FILE* objd_stream = popen(objdump_cmd, "r");
    
    FileBuffer obj_buff = {0};
    obj_buff.file = objd_stream;
    obj_buff.name = "objdump";
    obj_buff.data = malloc(FILE_BUFFER_CAPACITY);

    if(obj_buff.data == NULL){
        fprintf(stderr, "Failed to alloc memory\n");
        return 1;
    }

    fprintf(error_file, "-------------------\n");
    fprintf(error_file, "VERIFICATION ERRORS\n");
    fprintf(error_file, "-------------------\n");

    ArrayList error_lines = {0};
    array_list_create_cap(error_lines, ErrorLine, 64);

    init_scratch_buffer();
    FileBuffer* asm_buff = file_buffer_create(input_file);
    int line_count = go_to_first_instruction(asm_buff, "_start:");

    go_to_first_instruction(&obj_buff, "_start");
 
    int fail_count = 0;
    int total = 0;
    while(!file_buffer_eof(&obj_buff)){
        int variant = 0;
        char* obj_line = get_line_from_objdump(&obj_buff);
        char* input_line = get_line_from_input_file(asm_buff, &variant);
        
        if(!strcmp_ignore_whitespace(input_line, obj_line)){
            //objdump prints out implicit operands
            if(!strcmp_check_alias(input_line, obj_line)){
                new_error_line(&error_lines, line_count, input_line, obj_line);
                fail_count++;
            }
        }
        line_count++;
        total++;
    }

    qsort(error_lines.data, error_lines.size, sizeof(ErrorLine), compare_error_lines); 
    for(int i = 0; i < error_lines.size; i++){
        ErrorLine el = array_list_get(error_lines, ErrorLine, i);
        fprintf(error_file, "Line: %04d -> %s | %s\n", el.line, el.asmLine, el.objLine);
    }


    printf("Could not automatically verify: %d out of %d instruction variants\n", fail_count, total);
    file_buffer_delete(asm_buff);
    fclose(error_file);
    pclose(objd_stream);
}
