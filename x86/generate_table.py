import os
import subprocess
registers = [
"YMM0","YMM1","YMM2","YMM3","YMM4","YMM5","YMM6","YMM7","YMM8","YMM9","YMM10","YMM11","YMM12","YMM13","YMM14","YMM15",
"XMM0","XMM1","XMM2","XMM3","XMM4","XMM5","XMM6","XMM7","XMM8","XMM9","XMM10","XMM11","XMM12","XMM13","XMM14","XMM15",
"MM0","MM1","MM2","MM3","MM4","MM5","MM6","MM7",
"RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI","R8","R9","R10","R11","R12","R13","R14","R15",
"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI","R8D","R9D","R10D","R11D","R12D","R13D","R14D","R15D",
"AX","CX","DX","BX","SP","BP","SI","DI","R8W","R9W","R10W","R11W","R12W","R13W","R14W","R15W",
"AL","CL","DL","BL","AH","CH","DH","BH","R8B","R9B","R10B","R11B","R12B","R13B","R14B","R15B"]

tokens = """
typedef enum {
    TOK_NEW_LINE = '\\n',
    TOK_MULTIPLY = '*',
    TOK_ADD = '+',
    TOK_COMMA = ',',
    TOK_COLON = ':',
    TOK_OPENING_BRACKET = '[',
    TOK_CLOSING_BRACKET = ']',
    TOK_INSTRUCTION = 256,
    TOK_REG = 257, 
    TOK_GLOBAL,
    TOK_EXTERN,
    TOK_SECTION,
    TOK_TEXT,
    TOK_DATA,
    TOK_BSS,
    TOK_RESB,
    TOK_RESW,
    TOK_RESD,
    TOK_RESQ,
    TOK_REST,
    TOK_RESDQ,
    TOK_RESY,
    TOK_DB,
    TOK_DW,
    TOK_DD,
    TOK_DQ,
    TOK_DT,
    TOK_ST0,
    TOK_ST1,
    TOK_ST2,
    TOK_ST3,
    TOK_ST4,
    TOK_ST5,
    TOK_ST6,
    TOK_ST7,
    TOK_BYTE,
    TOK_WORD, 
    TOK_DWORD,
    TOK_QWORD,
    TOK_TWORD,
    TOK_DQWORD,
    TOK_YWORD,
    TOK_END_KEYWORDS,
    TOK_INT,
    TOK_UINT,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_MAX,
} TokenType;
"""

INPUT_FILE_NAME = "gerf_input_nmemonic.dat"
OUTPUT_FILE_NAME = "nmemonics.h"

gperf_input_file = open(INPUT_FILE_NAME, "w")
gperf_input_file.write("%{#include <stdint.h>\n")

gperf_input_file.write("typedef enum  {\n")
for reg in registers:
    gperf_input_file.write(f"REG_{reg},\n")
gperf_input_file.write(f"REG_MAX,\n")
gperf_input_file.write("} RegisterType;\n")


gperf_input_file.write(f"{tokens}\n")

gperf_input_file.write("const char* token_to_string(TokenType type){\n switch(type){\n")
for t in tokens.split('\n'):
    if 'TOK_' in t:
        t = t.lstrip() 
        new_tok = ""
        for c in t:
            if c == ' ' or c == '=' or c == ',':
                break
            new_tok += c
            
        gperf_input_file.write(f"    case {new_tok}: return \"{new_tok}\";\n")

gperf_input_file.write("}\n}\n")




#pack the values more
instr_struct = """
typedef struct {
    union {
        uint16_t three_vex;
        uint8_t two_vex;
        uint8_t rex;
        uint8_t variant_count;
    };
    uint8_t op1;
    uint8_t op2; 
    uint8_t op3; 
    uint8_t bytes[4];
    uint8_t size;
    int8_t digit;
    int8_t ib;
    uint8_t r; 
} Instruction;\n
"""

MODRM_CONTAINS_REG_AND_MEM = 0x1 
ADD_REG_TO_OPCODE = 0x2
REX = 0x4
TWO_BYTE_VEX = 0x8
THREE_BYTE_VEX = 0x10



# encode the fourth parameter in the immediate byte of the struct
# only valid in vex instructions
INSTR_OP4_IS_REG = 0x2

instr_packing_macros = """
#define MODRM_CONTAINS_REG_AND_MEM 0x1 
#define ADD_REG_TO_OPCODE 0x2
#define INSTR_USES_REX 0x4
#define INSTR_USES_2VEX 0x8
#define INSTR_USES_3VEX 0x10
#define INSTR_OP4_IS_REG 0x2
"""

def write_instruction_debug(gperf_input_file):
    gperf_input_file.write("static void print_instruction(Instruction* instr){\n")
    # finding the name of the nmemonic is hard because we don't know 
    # were in the hashtable the nmemonic lies 
    # because of this we kind of just have to brute force linear search
    gperf_input_file.write("""
for(int i = 0; i < MAX_HASH_VALUE; i++){
    if(KEYWORD_TABLE[i].type == TOK_INSTRUCTION){
        int variant_table_index = KEYWORD_TABLE[i].value;
        int start = variant_table_index + 1;
        for(int j = start; j < start + INSTRUCTION_TABLE[variant_table_index].variant_count; j++){
            Instruction instr_variant = INSTRUCTION_TABLE[j];
            if(instr->size == instr_variant.size && instr->three_vex == instr_variant.three_vex && instr->digit == instr_variant.digit){
                int matches = 1;
                for(int k = 0; k < instr->size; k++){
                   if(instr->bytes[k] != instr_variant.bytes[k]){
                        matches = 0; 
                        break;
                   } 
                }
                if(matches){
                    printf(\"%s: ",KEYWORD_TABLE[i].name);
                    goto e_loop;
                }
            }
        }
    }
}
e_loop:
""")
    gperf_input_file.write("    for(int i = 0; i < instr->size; i++) printf(\"%02x \", instr->bytes[i]);\n")
    gperf_input_file.write("    printf(\"\\nOperand 1: %s\\n\", operand_to_string(instr->op1));\n")
    gperf_input_file.write("    printf(\"Operand 2: %s\\n\", operand_to_string(instr->op2));\n")
    gperf_input_file.write("    printf(\"Operand 3: %s\\n\", operand_to_string(instr->op3));\n")
    gperf_input_file.write("    printf(\"Opcode Extension: %i\\n\", instr->digit);\n")
    gperf_input_file.write("    printf(\"Immediate Byte: %i\\n\", instr->ib);\n")
    gperf_input_file.write("    printf(\"Modrm contains Reg: %s\\n\", ((instr->r & 0x1) != 0) ? \"true\" : \"false\");\n")
    gperf_input_file.write("    printf(\"Add Register to Opcode: %s\\n\", ((instr->r & 0x2) != 0) ? \"true\" : \"false\");\n")
    gperf_input_file.write("    printf(\"Uses 2 byte Vex: %s\\n\", ((instr->r & 0x8) != 0) ? \"true\" : \"false\");\n")
    gperf_input_file.write("    printf(\"Uses 3 byte Vex: %s\\n\", ((instr->r & 0x10) != 0) ? \"true\" : \"false\");\n")
    gperf_input_file.write("    printf(\"Rex Prefix: %i\\n\", instr->rex);\n}\n")
    




class Instruction:
    def __init__(self, rex, opcode, digit, ib, r) -> None:
        self.rex = rex
        self.opcode = opcode 
        self.digit =  digit
        self.ib = ib
        self.r = r

        self.op1 = 0
        self.op2 = 0
        self.op3 = 0

    def to_c_struct(self): 
        begin = "{" 
        opcode = "{"
        op_len = len(self.opcode)
        for i in range(4):
            if i < len(self.opcode):
                opcode += hex(self.opcode[i]) + ','
            else:
                opcode += "0x00" + ','
        opcode = opcode[:-1]
        opcode += '}'

        op1 = f"(OperandType){self.op1}"
        op2 = f"(OperandType){self.op2}"
        op3 = f"(OperandType){self.op3}"

        return begin + f"(uint16_t){hex(self.rex)}, {op1}, {op2}, {op3}, {opcode}, {op_len}, {self.digit}, {self.ib}, {self.r}}},"
        




def parse_opcode(op):
    new_op = ""
    prev = ' '

    # ensuring that / and + have a space before them
    # this will just make it easier to parse
    for c in op:
        # VEX.LZ. 0F38.W1 F2 /r ANDN
        # there is an unwanted space in this instruction opcode
        # we need to remove here is we can parse it properly
        if c == ' ' and prev == '.':
            continue
        if (c == '+' or c == '/') and prev != ' ':
            new_op += ' ' + c
        else:
            if prev == '+' and c != ' ':
                new_op += ' ' 
            new_op += c 

        prev = c


    chunks = new_op.split(' ')
   
    digit = -1
    r = 0
    ib = -1
    rex = 0

    vex = 0
    two_vex = 0

    opcode = []

    low_op= ["rb", "rw", "rd", "ro"]

    prev = None
    for chunk in chunks:
        chunk = chunk.strip()
        if chunk == '+':
            prev = '+'
            continue

        if chunk[0] == "/":
            try:
                if chunk[1].isdigit():
                   digit = int(chunk[1]) 
                elif chunk[1] == "r":
                    r |= MODRM_CONTAINS_REG_AND_MEM
                elif chunk[1:] == 'is4':
                    ib = INSTR_OP4_IS_REG 
                elif chunk[1:] == 'ib':
                    ib = 1
                else:
                    print(f"Failed: {chunk} in {chunks}")
            except IndexError:
                if chunks[1] == '6E':
                    # again more inconsistency 
                    r |= MODRM_CONTAINS_REG_AND_MEM
                else:
                    print(f"Failed: {chunk} in {chunks}")
                    return None

        elif prev == '+':
            if chunk[0] == "i":
                # should not have to do anything here 
                # if one of the operands is an fpu index 
                # we know we have to add it to the opcode
                assert len(chunk) == 1, f"Chunks containg +i should be size 1 not {len(chunk)}"
            elif any(chunk == op for op in low_op):
                r |= ADD_REG_TO_OPCODE
                assert len(chunk) == 2, f"Chunks containg +rx should be size 2 not {len(chunk)}"
            else:
                try:
                    op = int(chunk, 16) 
                    opcode.append(op)
                except ValueError:
                    print(f"Failed: {chunk} in {chunks}")

        elif chunk == "NP" or chunk == "NFx":
            pass
 
        elif "REX" in chunk:
            rex = 0x40
            if chunk[-1] == "W":
                rex |= 0x8

        elif "VEX" in chunk:
            chunk = chunk.strip()
            vex_encoding = chunk.split('.')
            vex_encoding = vex_encoding[1:]

            is_three_byte = False
            
            for enc in vex_encoding:
                if enc == "128" or enc == "LZ" or enc == "L0" or enc == "LIG":
                    vex |= 0
                elif enc == "256" or enc == "L1":
                    vex |= 0x4 
                elif enc == "66":
                    vex |= 1
                elif enc == "F3":
                    vex |= 2
                elif enc == "F2":
                    vex |= 3
                elif enc == "0F38":
                    is_three_byte = True
                    two_vex |= 2
                elif enc == "0F3A":
                    is_three_byte = True
                    two_vex |= 3
                elif enc == "W0":
                    is_three_byte = True
                elif enc == "W1":
                    is_three_byte = True
                    vex |= 128
                elif enc == "Wig":
                    pass 
            
            if is_three_byte:
                r |= THREE_BYTE_VEX
            else:
                r |= TWO_BYTE_VEX 

        elif chunk == "ib" or chunk == "ib1" or chunk == "imm8":
            ib = 1
        elif chunk == "iw":
            ib = 2
        elif chunk == "id":
            ib = 4
        elif chunk == "io":
            ib = 8
        elif chunk == "cb":
            ib = 1
        elif chunk == "cw":
            ib = 2
        elif chunk == "cd":
            ib = 4
        elif chunk == "cp":
            ib = 6
        elif chunk == "co":
            ib = 8
        elif chunk == "ct":
            ib = 10
        
        else:
            try:
                # some instructions the opcode 0x0f38 is mushed into one
                if len(chunk) == 4:
                    high = int(chunk[:2], 16)
                    low = int(chunk[2:], 16)
                    opcode.append(high)
                    opcode.append(low)
                else:
                    op = int(chunk, 16) 
                    opcode.append(op)
            except ValueError:
                print(f"Failed: {chunk} in {chunks}")
                return None

        prev = chunk

    if (r & TWO_BYTE_VEX) or (r & THREE_BYTE_VEX):
        return Instruction((two_vex << 8) | vex, opcode, digit, ib, r)
    else:
        r |= REX
        return Instruction(rex, opcode, digit, ib, r)



enum_var = 0

def iota():
    global enum_var 
    enum_var += 1
    return enum_var


operand_types = {}


operand_types["NOP"] = 0
operand_types["rel8" ] =  iota()
operand_types["rel16" ] =  iota()
operand_types["rel32" ] =  iota()


operand_types["ES" ] =  iota()
operand_types["CS" ] =  iota()
operand_types["SS" ] =  iota()
operand_types["DS" ] =  iota()
operand_types["FS" ] =  iota()
operand_types["GS" ] =  iota()

operand_types["reg"] = iota()
operand_types["r8" ] =  iota()
operand_types["r16" ] =  iota()
operand_types["r32" ] =  iota()
operand_types["r64" ] =  iota()


operand_types["mem_any" ] =  iota()
operand_types["m8" ] =  iota()
operand_types["m16" ] =  iota()
operand_types["m32" ] =  iota()
operand_types["m64" ] =  iota()
operand_types["m128" ] =  iota()
operand_types["m256" ] =  iota()

# put this after because it doesn't really fit in with the rest
operand_types["m80" ] =  iota()

operand_types["r/m8"] = iota()
operand_types["r/m16"] = iota()
operand_types["r/m32"] = iota()
operand_types["r/m64"] = iota()

operand_types["imm8" ] =  iota()
operand_types["imm16" ] =  iota()
operand_types["imm32" ] =  iota()
operand_types["imm64" ] =  iota()

operand_types["signed"] = iota()

#represent labels
operand_types["l8" ] =  iota()
operand_types["l16" ] =  iota()
operand_types["l32" ] =  iota()
operand_types["l64" ] =  iota()
operand_types["m" ] =  iota()
operand_types["ST(i)"] = iota()
operand_types["DR0–DR7"] =  iota()



operand_types["mm/m32"] = iota()
operand_types["mm/m64"] = iota()


operand_types["mm"] = iota()
operand_types["xmm"] = iota()
operand_types["ymm"] = iota()

operand_types["xmm/m8"] = iota()
operand_types["xmm/m16"] = iota()
operand_types["xmm/m32"] = iota()
operand_types["xmm/m64"] = iota()
operand_types["xmm/m128"] = iota()


operand_types["ymm/m256"] = iota()


operand_types["bnd"] = iota()

operand_types["AL"] = 248
operand_types["CL"] = 249
operand_types["AX"] = 250
operand_types["DX"] = 251
operand_types["EAX"] = 252
operand_types["RAX"] = 253
operand_types["unsupported"] = 255






def check_operand(nmemonic, op):

    # for the registers xmm, mm, ymm,zmm
    # there doesn't need to be a number at the end
    # these instructions can take in any register of that type
    if op[0] != 'i':
        op = op.replace("mm1", "mm")
        op = op.replace("mm2", "mm")
        op = op.replace("mm3", "mm")
        op = op.replace("mm4", "mm")

    # the a's and b's don't mean anything to us and so we get rid of them 
    if op == 'r32a' or op == 'r32b' or op == 'r64a' or op == 'r64b':
        op = op[:-1]
    if op.startswith("m80"):
        op = op[:3]

    if op == "r64/m64":
        return operand_types["r/m64"]
    if op == "r32/m32":
        return operand_types["r/m32"]
    
    # implicit defined in instruction encoding so we 
    # treat them like no operand
    if op == "<XMM0>" or op == "<YMM0>" or op.lower() == '<eax>' or op=='<edx>':
        return operand_types["NOP"]
    if op == "ST(0)": 
        # these instructions operate on the fpu stack
        # meaning they don't have operands
        return operand_types["NOP"]
    # convert fpu memory types to just regular memory types 
    # since there really is no difference between them
    elif op == "m16int":
        return operand_types["m16"]
    elif op == "m32fp" or op == "m32int":
        return operand_types["m32"]
    elif op == "m64fp" or op == "m64int":
        return operand_types["m64"]
    try:
        return operand_types[op]
    except KeyError:
        print(f"Operand Not Supported for instruction {nmemonic}: {op}")
        return operand_types["unsupported"]



class ParsedOperands:
    def __init__(self, nmemonic: str, op1: int, op2: int, op3: int, op4: int):
        self.nmemonic = nmemonic
        self.op1= op1 
        self.op2= op2 
        self.op3= op3 
        self.op4= op4 



def parse_operands(desc):
    # some inconsistency in the intel pdf 
    desc = desc.replace('ymm3 /m256', 'ymm3/m256')
    desc = desc.replace('ymm3/.m256', 'ymm3/m256')

    op_format_list= []

    temp = ""
    for c in desc:
        if c == ' ' or c == ',':
            if temp != '':
                op_format_list.append(temp)
                temp = ""
        else:
            temp += c

    if temp != '':
        op_format_list.append(temp)


 
    nmemonic = op_format_list[0]

    if op_format_list[-1].isdigit():
        op_format_list = op_format_list[:-1]
    # remove repeat prefix
    for x in op_format_list:
        if "REP" in x:
            op_format_list.remove(x)
 
    if len(op_format_list) == 1:
        return ParsedOperands(nmemonic, 0,0,0,0)
    elif len(op_format_list) == 2:
        op = op_format_list[1]
        return ParsedOperands(nmemonic, check_operand(nmemonic, op),0,0,0)
    elif len(op_format_list) == 3:
        op1 = op_format_list[1]
        op2 = op_format_list[2]

        op1_type = check_operand(nmemonic, op1)
        op2_type = check_operand(nmemonic, op2)

        # some fpu instructions have only take one operand but they operate on ST(0)
        # we convert ST(0) to a no operand so we need to make sure the op2 is not an index 
        # onto the fpu stack
        if op1_type == operand_types["NOP"] and op2_type == operand_types["ST(i)"]:
            return ParsedOperands(nmemonic, op2_type,op1_type,0,0)
        else:
            return ParsedOperands(nmemonic,op1_type,op2_type,0, 0)
    elif len(op_format_list ) == 4:
        op1 = op_format_list[1]
        op2 = op_format_list[2]
        op3 = op_format_list[3]
        return ParsedOperands(nmemonic,check_operand(nmemonic, op1),check_operand(nmemonic,op2),check_operand(nmemonic, op3), 0)
    elif len(op_format_list) == 5:
        op1 = op_format_list[1]
        op2 = op_format_list[2]
        op3 = op_format_list[3]
        op4 = check_operand(nmemonic, op_format_list[4])
        return ParsedOperands(nmemonic,check_operand(nmemonic, op1),check_operand(nmemonic,op2),check_operand(nmemonic, op3), op4)
    else:
        print(f"Error unkown operands: {op_format_list}")
        return None 




with open("instructions.dat", "r") as f:
    instructions = {}
    op_code_table = []
    lines = f.readlines() 

    for line in lines:
        dash = line.find(chr(0x2014))
        if dash != -1:
            inst_name = line[:dash].strip()
            # many instructions that are seperated by a dash
            if "/" in inst_name:
                if ',' in inst_name:
                    if 'MOVDQU' in inst_name:
                        instructions['MOVDQU'] = []
                        instructions['VMOVDQU'] = []
                    elif 'MOVDQA' in inst_name:
                        instructions['MOVDQA'] = []
                        instructions['VMOVDQA'] = []
                    else:
                        print(f"Unkown instruction: {inst_name}")
                    continue

                

                for instr in inst_name.split('/'):
                    if instr not in instructions and not instr[0].isdigit():
                        instructions[instr] = []
            else:
                if inst_name not in instructions:
                    instructions[inst_name] = []
        else:
            op_row = line.split('|')
            opcode = op_row[0].strip()
            operands= op_row[1].strip()

            # there is a formatting issue with ROUNDPS in instructions.dat
            if operands.startswith('/r ib'):
                opcode += ' ' + operands[:7]
                operands = operands[6:]

            parsed_instruction= parse_opcode(opcode)
            if parsed_instruction == None:
                    continue

            parsed_operands = parse_operands(operands)
            if parsed_operands != None:
                parsed_instruction.op1 = parsed_operands.op1 
                parsed_instruction.op2 = parsed_operands.op2
                parsed_instruction.op3 = parsed_operands.op3
 
                try:
                    instructions[parsed_operands.nmemonic].append(parsed_instruction)
                except KeyError:
                    instructions[parsed_operands.nmemonic] = [parsed_instruction]


    sorted_instructions = sorted(instructions)

                                
    operand_enum = []
    
    gperf_input_file.write("typedef enum {\n")
    for op_type in operand_types:
        op_name = op_type
        if "/" in op_name:
            op_name = op_name.replace("/", "")
        elif chr(0x2013) in op_name:
            op_name = op_name.replace(chr(0x2013), "")
        elif op_name == "ST(i)":
            op_name = op_name = "STI"

        field_name = "OPERAND_" + op_name.upper()
        operand_enum.append(field_name)
        gperf_input_file.write(field_name + " = " + str(operand_types[op_type]) + ",\n")

    gperf_input_file.write("}OperandType;\n")

    gperf_input_file.write(instr_struct)
    gperf_input_file.write(instr_packing_macros)

    gperf_input_file.write("static const char* operand_to_string(OperandType type){\n")
    gperf_input_file.write("    switch(type){\n")

    for operand_field in operand_enum:
        gperf_input_file.write(f"    case {operand_field}: return \"{operand_field}\";\n")
        
    gperf_input_file.write("    default: return \"INVALID OPERAND\";\n}}\n")
    
    gperf_input_file.write("static const Instruction INSTRUCTION_TABLE[] = {\n")


    instr_variant_lookup = []
    current_index = 0
    instruction_table_size = 0
    for i, instr in enumerate(sorted_instructions):
        # before each instruction variant we just have a struct
        # that tells you have many variants there are
        variant_count = Instruction(len(instructions[instr]), [0,0,0,0], 0, 0,0)
        gperf_input_file.write(variant_count.to_c_struct() + '\n')
        instr_variant_lookup.append(current_index)
        current_index += 1
        for instr_variant in instructions[instr]:
            gperf_input_file.write(instr_variant.to_c_struct() + '\n')
            current_index += 1
            instruction_table_size += 1

    gperf_input_file.write("};\n")
    gperf_input_file.write(f"#define INSTRUCTION_TABLE_SIZE {instruction_table_size}\n")
    gperf_input_file.write("%}\n")

    # write the gperf output
    gperf_input_file.write("""
%ignore-case
%includes
%language=C
%define lookup-function-name find_keyword 
%readonly-tables
%global-table
%readonly-tables
%define word-array-name KEYWORD_TABLE
%null-strings
%struct-type
struct Keyword {const char* name; TokenType type; uint16_t value;};
%%
""")

    for index, reg in enumerate(registers):
        gperf_input_file.write(f"{reg}, TOK_REG, {index}\n")


    # write the regular keywords
    tok_enum = tokens.split('\n')
    index = 0
    for entry in tok_enum:
        if 'TOK_REG' in entry:
            index += 1
            break
        index += 1
    start_value = 258

    current_token = tok_enum[index].strip()
    current_token = current_token.replace(',', '')
    current_token = current_token.replace('\n', '')
    while current_token != 'TOK_END_KEYWORDS':
        # for keywords the value is just going to be the same as the type
        # remove TOK_
        kw = current_token[4:]
        if kw.lower() == 'text' or kw.lower() == 'data' or kw.lower() == 'bss':
            kw = '.' + kw
        gperf_input_file.write(f"{kw}, {current_token}, {current_token}\n")
        start_value += 1
        index += 1
        current_token = tok_enum[index].strip()
        current_token = current_token.replace(',', '')
        current_token = current_token.replace('\n', '')



    for index, instr in enumerate(sorted_instructions):
        gperf_input_file.write(f"{instr}, TOK_INSTRUCTION, {instr_variant_lookup[index]}\n")


    # write debug functions
    gperf_input_file.write("%%\n")
    gperf_input_file.write("#ifdef DEBUG\n#include<stdio.h>\n#include <string.h>\n")
    write_instruction_debug(gperf_input_file)
    gperf_input_file.write("#else\nstatic void print_instruction(Instruction* instr){}\n") 
    gperf_input_file.write("#endif\n")


gperf_input_file.close()

# call gperf on our input file, check for success return code,and delete the input file
p_result = subprocess.run(["gperf",INPUT_FILE_NAME,f"--output-file={OUTPUT_FILE_NAME}"])

try:
    p_result.check_returncode()
except subprocess.CalledProcessError:
    print("Failed to execute gperf: {p_result.returncode}")
finally:
    os.remove(INPUT_FILE_NAME)
