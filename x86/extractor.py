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
        "PREFETCHW" : 'M',
}

@dataclass
class InstructionVar:
    opcode: str
    operands: str
    encoding: str
    support: str
    cpuid: str | None
    description: str

@dataclass
class OperandEncRow:
    id: str
    tuple_type: str | None
    encoding: str


output_file = open("instructions.dat", "w")
#out = open("temp.dat", "w") # create a text output


def get_encoding(op1: str, op2: str, op3: str, op4: str) -> str | None:
    if 'reg' in op1:
        if 'r/m' in op2:
            if 'vex' in op3:
                return 'RMV'
            elif 'imm' in op3:
                return 'RMI'
            elif 'n/a' in op3 or 'implicit' in op3:
                return 'RM'
        elif 'vex' in op2:
            if 'r/m' in op3:
                if 'imm8[7:4]' in op4:
                    return 'RVMR'
                elif 'imm8' in op4:
                    return 'RVMI'
                elif 'n/a' in op4:
                    return 'RVM'
        elif 'vsib' in op2:
            return 'RVSV'
        elif 'n/a' in op2:
            return 'R'
    elif 'r/m' in op1:
        if 'reg' in op2:
            if 'imm' in op3:
                return 'MRI'
            elif 'n/a' in op3:
                return 'MR'
        elif 'vex' in op2:
            if 'reg' in op3:
                return 'MVR'
            # it seems there was a typo in the operand table
            # for MOVLPD based on the operands it seems
            # it should be RVM
            elif 'r/m' in op3:
                return 'RVM'
        elif 'implicit' in op2 or 'n/a' in op2:
            return 'M'
        elif 'imm' in op2:
            return 'MI'
    elif 'vex' in op1:
        if 'r/m' in op2:
            if 'imm' in op3:
                return 'VMI'
            else:
                return 'VM'
    elif 'n/a' in op1:
        return 'ZO'
    elif 'imm' in op1:
        return 'I'
    elif 'offset' in op1:
        return 'D'
    return None

def get_operand_encoding_table(tables, page_num: int) -> list[OperandEncRow]:
    operand_table = None
    for table in tables:
        if table.extract()[0][0].lower() == 'op/en':
            operand_table = table
            break

    # means the table is on the next page or the page after
    if operand_table == None:
        page = doc.load_page(page_num + 1)
        table_finder = page.find_tables()
        assert table_finder, "Error finding tables"
        tables = table_finder.tables
        return get_operand_encoding_table(tables, page_num + 1)

    op_enc_rows = []
    operand_table = operand_table.extract()
    contains_tuple = 'tuple' in operand_table[0][1].lower()
    for i in range(1,len(operand_table)):
        row = operand_table[i]
        id = row[0].strip()
        tup_type = None
        encoding = ""
        if contains_tuple:
            tup_type = row[1].strip()
            op1 = row[2].strip().lower()
            op2 = row[3].strip().lower()
            op3 = ""
            op4 = ""
            if len(row) > 4:
                op3 = row[4].strip().lower()
                op4 = row[5].strip().lower()
            encoding = get_encoding(op1, op2, op3, op4)
        else:
            op1 = row[1].strip().lower()
            op2 = row[2].strip().lower()
            op3 = row[3].strip().lower()
            op4 = row[4].strip().lower()
            encoding = get_encoding(op1, op2, op3, op4)
        if encoding == None:
            print(f"Failed to get encoding on page {page_num} with id {id}")
        else:
            op_enc_rows.append(OperandEncRow(id, tup_type, encoding))
    return op_enc_rows

def write_opcode_table(instruction: str | None, op_table, tables, page_num: int) -> bool:
    global output_file

    in_op_en = False

    pad_size = 45


    if "/" in op_table[0][0]:
        in_op_en = True


    instruction_count = 0

    opcode_table_rows = []

    # these encodings point to tables following the opcode tables
    unknown_encodings = ['A', 'B', 'C', 'D', 'E']
    encoding_follows_op_table = False

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
                if not encoding_follows_op_table and encoding in unknown_encodings:
                    encoding_follows_op_table = True
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
    
    operand_enc_table = None
    # for some reason pymupdf cannot properly extract this table
    if instruction != None and "PINSRB" in instruction:
        operand_enc_table = [
            OperandEncRow('A', None, 'RMI'),
            OperandEncRow('B', None, 'RVMI'),
            OperandEncRow('C', "Tuple1 Scalar", ''), # TODO: REPLACE WITH EVEX encoding
        ]
    elif instruction != None and ("VPERMILPD" in instruction or "VPERMILPS" in instruction):
        operand_enc_table = [
            OperandEncRow('A', None, 'RVM'),
            OperandEncRow('B', None, 'RMI'),
            OperandEncRow('C', "Full", ''), # TODO: REPLACE WITH EVEX encoding
            OperandEncRow('D', "Full", 'RMI'),
        ]
    elif encoding_follows_op_table:
        operand_enc_table = get_operand_encoding_table(tables, page_num)

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
            if encoding_follows_op_table:
                assert operand_enc_table, f"Failed to get encoding table {instruction}"
                for row in operand_enc_table:
                    if row.id == variant.encoding:
                        variant.encoding = row.encoding
                        break

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
