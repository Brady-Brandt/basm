from dataclasses import dataclass
import pymupdf


instructions = {}

unsupported = []

letters = []

known_pages = []


doc = pymupdf.open("intel.pdf")

encodings = []


# operand encoding for these instructions as stated in the intel manual is incorrect
correct_encodings = { 
        "VPCMPISTRM": 'RMI',
        "PCMPISTRM" : 'RMI',
        "PTWRITE": 'M',
        "PCMPISTRI": 'RMI',
        "VPCMPISTRI": 'RMI',
        "SHA256RNDS2": 'RM0',
        "HRESET": 'I',
}

@dataclass
class InstructionVar:
    opcode: str
    operands: str
    encoding: str
    support: str
    cpuid: str | None
    description: str


output_file = open("instructions.dat", "w")
#out = open("temp.dat", "w") # create a text output


def get_encoding(nm, encoding_val, tables, page_num):
    operand_table = None
    try:
        operand_table = tables[1]
    except IndexError:
        # operand table should be on the next page
        page = doc.load_page(page_num + 1)
        table_finder = page.find_tables()
        tables = table_finder.tables


    found_operand_table = False

    for i in range(0, len(tables)):
        operand_table = tables[i]
        if operand_table.extract()[0][0].lower() == 'op/en':
            found_operand_table = True
            break

    if not found_operand_table:
        # check the next page
        page = doc.load_page(page_num + 2)
        table_finder = page.find_tables()
        tables = table_finder.tables
        return get_encoding(nm, encoding_val, tables, page_num + 2)
        
    op1 = 2
    op2 = 3
    op3 = 4
    op4 = 5

    if 'tuple' not in operand_table.header.names[1].lower():
        op1 -= 1
        op2 -= 1
        op3 -= 1
        op4 -= 1

    operand_table = operand_table.extract()

    for i in range(1,len(operand_table)):
        row = operand_table[i]

        # PREFETCHW—Prefetch table the encoding_val is wrong  
        if row[0] == encoding_val or i == len(operand_table) - 1:
            if 'reg' in row[op1].lower():
                if 'r/m' in row[op2].lower():
                    if 'vex' in row[op3].lower():
                        return 'RMV'
                    elif 'imm' in row[op3].lower():
                        return 'RMI'
                    elif 'n/a' in row[op3].lower() or 'implicit' in row[op3].lower(): 
                        # operanding encoding for these instruction is incorrect
                        if nm != None and ('vpermilps' in nm.lower() or 'vpermilpd' in nm.lower()):
                            return 'RMI'
                        return 'RM'
                elif 'vex' in row[op2].lower():
                    if 'r/m' in row[op3].lower():
                        try:
                            if 'imm8[7:4]' in row[op4].lower():
                                return 'RVMR'
                            elif 'imm8' in row[op4].lower(): 
                                return 'RVMI'
                            elif 'n/a' in row[op4].lower():
                                return 'RVM'
                        except IndexError:
                            # PINSRB
                            # pymupdf can't get the fourth operand for this instruction for some reason
                            # so we have to manually check it
                            if encoding_val == 'A':
                                return 'RMI'
                            else:
                                return 'RVMI' 
                elif 'vsib' in row[op2].lower():
                    return 'RVSV'
                elif 'n/a' in row[op2].lower():
                    return 'R'
            elif 'r/m' in row[op1].lower():
                if 'reg' in row[op2].lower():
                    if 'imm' in row[op3].lower():
                        return 'MRI'
                    elif 'n/a' in row[op3].lower():
                        return 'MR'
                elif 'vex' in row[op2].lower():
                    if 'reg' in row[op3].lower():
                        return 'MVR'
                    # it seems there was a typo in the operand table
                    # for MOVLPD based on the operands it seems
                    # it should be RVM
                    elif 'r/m' in row[op3].lower():
                        return 'RVM'
                elif 'implicit' in row[op2].lower() or 'n/a' in row[op2].lower():
                    return 'M'
                elif 'imm' in row[op2].lower():
                    return 'MI'
            elif 'vex' in row[op1].lower():
                if 'r/m' in row[op2].lower():
                    if 'imm' in row[op3].lower():
                        return 'VMI'
                    else:
                        return 'VM'
            elif 'n/a' in row[op1].lower():
                return 'ZO'
            elif 'imm' in row[op1].lower():
                return 'I'
            elif 'offset' in row[op1].lower():
                return 'D'

    print("Failure: ",nm, operand_table, encoding_val)

def write_opcode_table(instruction: str | None, op_table, tables, page_num: int):
    global output_file

    in_op_en = False

    pad_size = 45


    if "/" in op_table[0][0]:
        in_op_en = True


    instruction_count = 0

    opcode_table_rows = []

    for i in range(1,len(op_table)):
        row = op_table[i]

        if row[1] == None:
            break

        if in_op_en:
            # only support 64 bit instructions
            if row[2][0] != 'V':
                continue
            if 'AVX512' in row[3]:
                continue
            opcode, *operands = row[0].split('\n')
            operands = ' '.join(operands).strip()
            if 'fxtract' in operands.lower():
                opcode_table_rows.append(
                        InstructionVar(opcode,operands,"FPU", row[1], "FPU", row[3]))
            else:
                encoding = row[1].replace('\n', '')
                support = row[2]
                cpuid = row[3]
                description = row[4]
                opcode_table_rows.append(
                        InstructionVar(opcode,operands,encoding, support, cpuid, description))

        else:
            if row[3][0] != 'V':
                continue

            opcode = row[0]
            operands = row[1].replace('*', '')
            operands = operands.strip()
            operands = operands.strip('\n')
            # most fpu instructions don't have encodings specified in the tables
            if operands[0].lower() == 'f':
                opcode_table_rows.append(
                        InstructionVar(opcode,operands,"FPU", row[2], "FPU", row[4]))
            else:
                encoding = row[2].rstrip('\n')
                opcode_table_rows.append(
                        InstructionVar(opcode, operands,encoding, row[3], None, row[5]))

        instruction_count += 1


    if instruction_count == 0:
        return False

    if inst != None:
        output_file.write(inst + "\n")
    # these encodings point to tables following the opcode tables
    unknown_encodings = ['A', 'B', 'C', 'D', 'E']
    enc_padding_size = 5

    for variant in opcode_table_rows:
        if in_op_en:
            variant.description = variant.description.lower()
            if 'sign extend' in variant.description or 'sign-extend' in variant.description:
                variant.operands = variant.operands.replace('imm', 'simm')

            # fix the instructions with incorrect encodings
            if instruction != None:
                for right_enc in correct_encodings:
                    if right_enc.lower() in instruction.lower():
                        variant.encoding = correct_encodings[right_enc]
                        break

            # for instructions that have evex encoding in the opcode table
            # we need to parse the operand encoding table afterwards
            # They use A,B,C,D as keys in an operand table instead of the more 
            # descriptive names that most other instructions use
            if variant.encoding in unknown_encodings:
                variant.encoding = get_encoding(instruction,variant.encoding, tables, page_num)
 
            if variant.encoding not in encodings:
                encodings.append(variant.encoding)
            
            encoding = variant.encoding
            enc_padding = enc_padding_size - len(encoding)
            padding = pad_size - len(variant.opcode) - len(encoding) - enc_padding - 1
 
            # trying to get rid of superscripts 
            # pymupdf doesn't have an easy way to determine if 
            # a piece of text is a superscript
            variant.operands = variant.operands.replace("81", "8").replace("82", "8")
            variant.operands = variant.operands.replace("*", "").replace("41", "4")

            output_file.write(encoding + enc_padding * " " + '|' +
                              variant.opcode + padding * " " + "|" + variant.operands + "\n")
        else:
            # performing these manual replaces is not ideal, but
            # the added run time is neglible with them
            variant.opcode  = variant.opcode.replace("/r1", "/r")

            variant.operands = variant.operands.replace("UD01", "UD0")
            variant.operands = variant.operands.replace("FNSTSW1", "FNSTSW")
            variant.operands = variant.operands.replace("FNSTCW1", "FNSTCW")
            variant.operands = variant.operands.replace("FNCLEX1", "FNCLEX")
            variant.operands = variant.operands.replace("FNINIT1", "FNINIT")
            variant.operands = variant.operands.replace("FNSAVE1", "FNSAVE")
            variant.operands = variant.operands.replace("FNSTENV1", "FNSTENV")

            variant.operands = variant.operands.replace("81", "8")
            variant.operands = variant.operands.replace("82", "8")
            variant.operands = variant.operands.replace("g2", "g")
            variant.operands = variant.operands.replace("83", "8")
            variant.operands = variant.operands.replace("63", "6")
            variant.operands = variant.operands.replace("23", "2")
            variant.operands = variant.operands.replace("43", "4")
            variant.operands = variant.operands.replace("42", "4")
            variant.operands = variant.operands.replace("62", "6")
            variant.operands = variant.operands.replace("61", "6")

            variant.description = variant.description.lower()
            if 'sign extend' in variant.description or 'sign-extend' in variant.description:
                variant.operands = variant.operands.replace('imm', 'simm')

            if variant.encoding not in encodings:
                encodings.append(variant.encoding)

            encoding = variant.encoding
            enc_padding = enc_padding_size - len(encoding)
            padding= pad_size - len(variant.opcode) - len(encoding) - enc_padding - 1

            output_file.write(encoding + enc_padding * ' ' +
                              '|' + variant.opcode + padding * " " + "|" + variant.operands + "\n")
    return True



#128, 2266
start = 128
page_num = start

should_add_unkown = True

prev_page = None
for page_num, page in enumerate(doc.pages(start, 2266)):
    page_num += start
# use this loop when we already know which pages we need to parse
# use the outer for loop when we don't
#for page_num in known_pages:
    #should_add_unkown = False 
    page = doc.load_page(page_num)

    words = page.get_text("words")

    inst = words[0][4]

    
    if chr(0x2014) not in inst:
        continue

    if inst not in instructions and inst not in unsupported:
        if inst[0] not in letters:
            print(inst[0])
            letters.append(inst[0])


        table_finder = page.find_tables()
        assert table_finder, "Error finding tables"

        if table_finder.tables == []:
           continue 

        op_table = table_finder.tables[0]
        op_table = op_table.extract()

        if write_opcode_table(inst, op_table, table_finder.tables, page_num):
            prev_page = inst
            if should_add_unkown:
                known_pages.append(page_num)
            instructions[inst] = op_table
        else:
            unsupported.append(inst)
    elif prev_page != None:
        # some tables go to the next page
        table_finder = page.find_tables()
        assert table_finder, "Error finding tables"
        if table_finder.tables == []:
           continue 
        op_table = table_finder.tables[0].extract()

        # check if its an opcode table
        if "Opcode" in op_table[0][0]:
            if should_add_unkown:
                known_pages.append(page_num)
            write_opcode_table(None, op_table, table_finder.tables, page_num)

    else:
        prev_page = None
output_file.close()

if should_add_unkown:
    print(known_pages)
print(encodings)
