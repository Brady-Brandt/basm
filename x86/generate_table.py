registers = [
"RAX",
"RCX",
"RDX",
"RBX",
"RSP",
"RBP",
"RSI",
"RDI",
"R8",
"R9",
"R10",
"R11",
"R12",
"R13",
"R14",
"R15",
"EAX",
"ECX",
"EDX",
"EBX",
"ESP",
"EBP",
"ESI",
"EDI",
"R8D",
"R9D",
"R10D",
"R11D",
"R12D",
"R13D",
"R14D",
"R15D",
"AX",
"CX",
"DX",
"BX",
"SP",
"BP",
"SI",
"DI",
"R8W",
"R9W",
"R10W",
"R11W",
"R12W",
"R13W",
"R14W",
"R15W",
"AL",
"CL",
"DL",
"BL",
"AH",
"CH",
"DH",
"BH",
"R8B",
"R9B",
"R10B",
"R11B",
"R12B",
"R13B",
"R14B",
"R15B"
]




nmemonic_file = open("nmemonics.h", "w")
nmemonic_file.write("// Auto generated by generate_table.py\n")
nmemonic_file.write("#include <stdint.h>\n")
nmemonic_file.write("static const char* REGISTER_TABLE[] = {\n")
for reg in registers:
    nmemonic_file.write(f"\"{reg}\",\n")
nmemonic_file.write("};\n")




   
    

def write_nmemonics_strings(f, nmemonics):
    total_size = len(nmemonics)
    f.write(f"#define NMEMONIC_TABLE_SIZE {total_size} \n")
    f.write("static const char* NMEMONIC_TABLE[] = {\n")
    for instr in nmemonics:
        f.write(f"\"{instr}\",\n")
    f.write("};\n")



#pack the values more
instr_struct = """
typedef struct {
    uint16_t instr;
    uint8_t rex;
    OperandType op1;
    OperandType op2; 
    OperandType op3; 
    uint8_t bytes[4];
    uint8_t size;
    int8_t digit;
    int8_t ib;
    uint8_t r; 
} Instruction;\n
"""


MODRM_CONTAINS_REG_AND_MEM = 0x1 
ADD_REG_TO_OPCODE = 0x2
#ADD_FPU_INDEX_TO_OPCODE = 0x4

instr_packing_macros = """
#define MODRM_CONTAINS_REG_AND_MEM 0x1 
#define ADD_REG_TO_OPCODE 0x2
"""


def write_instruction_debug(nmemonic_file):
    nmemonic_file.write("static void print_instruction(Instruction* instr){\n")
    nmemonic_file.write("    printf(\"%s: \", NMEMONIC_TABLE[instr->instr]);\n")
    nmemonic_file.write("    for(int i = 0; i < instr->size; i++) printf(\"%02x \", instr->bytes[i]);\n")
    nmemonic_file.write("    printf(\"\\nOperand 1: %s\\n\", operand_to_string(instr->op1));\n")
    nmemonic_file.write("    printf(\"Operand 2: %s\\n\", operand_to_string(instr->op2));\n")
    nmemonic_file.write("    printf(\"Operand 3: %s\\n\", operand_to_string(instr->op3));\n")
    nmemonic_file.write("    printf(\"Opcode Extension: %i\\n\", instr->digit);\n")
    nmemonic_file.write("    printf(\"Immediate Byte: %i\\n\", instr->ib);\n")
    nmemonic_file.write("    printf(\"Modrm contains Reg: %s\\n\", ((instr->r & 0x1) != 0) ? \"true\" : \"false\");\n")
    nmemonic_file.write("    printf(\"Add Register to Opcode: %s\\n\", ((instr->r & 0x2) != 0) ? \"true\" : \"false\");\n")
    nmemonic_file.write("    printf(\"Rex Prefix: %i\\n\", instr->rex);\n}\n")
    




class Instruction:
    def __init__(self, rex, opcode, digit, ib, r) -> None:
        self.instr = 0
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

        return begin + f"{self.instr}, {hex(self.rex)}, {op1}, {op2}, {op3}, {opcode}, {op_len}, {self.digit}, {self.ib}, {self.r}}},"
        




def parse_opcode(op):
    new_op = ""
    prev = ' '

    # ensuring that / and front have a space before them
    # this will just make it easier to parse
    for c in op:
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

    opcode = []

    low_op= ["rb", "rw", "rd", "ro"]


    prev = None
    for chunk in chunks:
        if chunk == '+':
            prev = '+'
            continue

        if chunk[0] == "/":
            if chunk[1].isdigit():
               digit = int(chunk[1]) 
            elif chunk[1] == "r":
                r = MODRM_CONTAINS_REG_AND_MEM
            else:
                print(f"Failed: {chunk} in {chunks}")
            assert len(chunk) == 2, f"Chunks containg / should be size 2 not {len(chunk), {chunk}}"

        elif prev == '+':
            if chunk[0] == "i":
                # should not have to do anything here 
                # if one of the operands is an fpu index 
                # we know we have to add it to the opcode
                #r = ADD_FPU_INDEX_TO_OPCODE
                assert len(chunk) == 1, f"Chunks containg +i should be size 1 not {len(chunk)}"
            elif any(chunk == op for op in low_op):
                r = ADD_REG_TO_OPCODE
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

        elif chunk == "ib":
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
                op = int(chunk, 16) 
                opcode.append(op)
            except ValueError:
                print(f"Failed: {chunk} in {chunks}")

        prev = chunk
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


operand_types["r8" ] =  iota()
operand_types["r16" ] =  iota()
operand_types["r32" ] =  iota()
operand_types["r64" ] =  iota()
#operand_types["xmm"] = iota()


operand_types["mem_any" ] =  iota()
operand_types["m8" ] =  iota()
operand_types["m16" ] =  iota()
operand_types["m32" ] =  iota()
operand_types["m64" ] =  iota()

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

operand_types["DX"] = 249
operand_types["CL"] = 249
operand_types["AL"] = 250
operand_types["AX"] = 251
operand_types["EAX"] = 252
operand_types["RAX"] = 253
operand_types["unsupported"] = 255







def check_operand(nmemonic, op):
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


def parse_operands(desc):
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
        return (nmemonic,0,0,0)
    elif len(op_format_list) == 2:
        op = op_format_list[1]
        return (nmemonic, check_operand(nmemonic,op),0,0)
    elif len(op_format_list) == 3:
        op1 = op_format_list[1]
        op2 = op_format_list[2]

        op1_type = check_operand(nmemonic, op1)
        op2_type = check_operand(nmemonic, op2)

        # some fpu instructions have only take one operand but they operate on ST(0)
        # we convert ST(0) to a no operand so we need to make sure the op2 is not an index 
        # onto the fpu stack
        if op1_type == operand_types["NOP"] and op2_type == operand_types["ST(i)"]:
            return (nmemonic, op2_type, op1_type, 0)
        else:
            return (nmemonic,op1_type,op2_type,0)
    elif len(op_format_list ) == 4:
        return (nmemonic,0,0,0)
    else:
        print(f"Error unkown operands: {op_format_list}")
        return False




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
                for instr in inst_name.split('/'):
                    if instr not in instructions:
                        instructions[instr] = []
            else:
                if inst_name not in instructions:
                        instructions[inst_name] = []
        else:
            op_row = line.split('|')
            opcode = op_row[0].strip()
            operands= op_row[1].strip()
            parsed_operands = parse_operands(operands)
            if parsed_operands != False:
                parsed_instruction= parse_opcode(opcode)
                parsed_instruction.op1 = parsed_operands[1]
                parsed_instruction.op2 = parsed_operands[2]
                parsed_instruction.op3 = parsed_operands[3]

                try:
                    instructions[parsed_operands[0]].append(parsed_instruction)
                except KeyError:
                    instructions[parsed_operands[0]] = [parsed_instruction]


    sorted_instructions = sorted(instructions)

    write_nmemonics_strings(nmemonic_file, sorted_instructions)



    operand_enum = []
    
    nmemonic_file.write("typedef enum {\n")
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
        nmemonic_file.write(field_name + " = " + str(operand_types[op_type]) + ",\n")

    nmemonic_file.write("}OperandType;\n")

    nmemonic_file.write(instr_struct)
    nmemonic_file.write(instr_packing_macros)

    # DEBUG FUNCTIONS FOR Instruction struct and enum
    nmemonic_file.write("#ifdef DEBUG\n#include<stdio.h>\n")
    nmemonic_file.write("static const char* operand_to_string(OperandType type){\n")
    nmemonic_file.write("    switch(type){\n")

    for operand_field in operand_enum:
        # cl has the same enum value as another
        if operand_field == "OPERAND_CL":
            continue
        nmemonic_file.write(f"    case {operand_field}: return \"{operand_field}\";\n")
        
    nmemonic_file.write("    default: return \"INVALID OPERAND\";\n}}\n")
    
    write_instruction_debug(nmemonic_file)



    nmemonic_file.write("#else\nstatic void print_instruction(Instruction* instr){}\n")
    nmemonic_file.write("static const char* operand_to_string(OperandType type){return 0;}\n")
    
    nmemonic_file.write("#endif\n")


    nmemonic_file.write("static const Instruction INSTRUCTION_TABLE[] = {\n")


    instr_variant_lookup = []
    current_index = 0
    instruction_table_size = 0
    for i, instr in enumerate(sorted_instructions):
        instr_variant_lookup.append(current_index)
        for instr_variant in instructions[instr]:
            instr_variant.instr = i
            nmemonic_file.write(instr_variant.to_c_struct() + '\n')
            current_index += 1
            instruction_table_size += 1


    nmemonic_file.write("};\n")


    
    nmemonic_file.write(f"#define INSTRUCTION_TABLE_SIZE {instruction_table_size}\n")


    nmemonic_file.write("static const uint16_t INSTRUCTION_TABLE_LOOK_UP[] = {\n")
    for index in instr_variant_lookup:
        nmemonic_file.write(f"{index},\n")

    nmemonic_file.write("};\n")       
