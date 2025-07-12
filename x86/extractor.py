import pymupdf


instructions = {}



unsupported = []


letters = []

known_pages = []


doc = pymupdf.open("intel.pdf")

encodings = []


out = open("instructions.dat", "w") 
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
                        return 'RM'
                elif 'vex' in row[op2].lower():
                    if 'r/m' in row[op3].lower():
                        # PINSRB
                        # pymupdf can't get the fourth operand for this instruction for some reason
                        # so we have to manually check it
                        try:
                            row[op4]
                        except:
                            if encoding_val == 'A':
                                return 'RMI'
                            else:
                                return 'RVM'

                        if 'imm8[7:4]' in row[op4]:
                            return 'RVMR'
                        elif 'imm8' == row[op4].lower(): 
                            return 'RVMI'
                        else:
                            return 'RVM'
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
                    # for MOVLPD
                    elif 'r/m' in row[op3].lower():
                        return 'MVR'
                elif 'implicit' in row[op2].lower() or 'n/a' in row[op2].lower():
                    return 'M'
                elif 'imm' in row[op2].lower():
                    return 'MI'
            elif 'vex' in row[op1].lower():
                if 'r/m' in row[op2].lower():
                    return 'VM'
            elif 'n/a' in row[op1].lower():
                return 'ZO'
            elif 'imm' in row[op1].lower():
                return 'I'
            elif 'offset' in row[op1].lower():
                return 'D'

    print("Failure: ",nm, operand_table, encoding_val)

def write_opcode_table(instruction, file, op_table, tables, page_num):
    in_op_en = False

    pad_size = 45


    if "/" in op_table[0][0]:
        in_op_en = True


    instruction_count = 0

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

        else:
            if row[3][0] != 'V':
                continue
        instruction_count += 1


    if instruction_count == 0:
        return False


    if inst != None:
        file.write(inst + "\n")

    
    # this encodings point to tables following the opcode tables
    # I don't want to parse them so we will have generate_table.py figure
    # out the operand encodings for the instructions that contain these values
    unknown_encodings = ['A', 'B', 'C', 'D', 'E']
    enc_padding_size = 5

    for i in range(1,len(op_table)):
        row = op_table[i]

        if row[1] == None:
            break
        
        if in_op_en:
            # only support 64 bit instructions
            if row[2][0] != 'V':
                continue
            # don't support EVEX instructions
            if 'AVX512' in row[3]:
                continue

            nl = row[0].find('\n')
            
            tmp = row[0].split('\n')
            # make sure the entire instruction is on one line
            if len(tmp) > 2:
                for i in range(2, len(tmp)):
                    tmp[1] += tmp[i]
            

            opcode = tmp[0]
            operands = tmp[1]
            operands = operands.strip()

            encoding = row[1]
            encoding = encoding.replace('\n','')

            # the operand encoding for this instruction 
            # is wrong
            if instruction != None and 'sha256rnds2' in instruction.lower():
                encoding = 'RM0'
            
            # fxtract has a different opcode table format then
            # than the rest of the fpu instructions
            if encoding == 'Valid':
                encoding = 'FPU'


            # for instructions that have evex encoding in the opcode table
            # we need to parse the operand encoding table afterwards
            # They use A,B,C,D as keys in an operand table instead of the more 
            # descriptive names that most other instructions use
            if encoding in unknown_encodings:
                encoding = get_encoding(instruction,encoding, tables, page_num)

            
          
            if encoding not in encodings:
                encodings.append(encoding)

            enc_padding = enc_padding_size - len(encoding)
            padding = pad_size - len(row[0][:nl]) - len(encoding) - enc_padding - 1
 
            # trying to get rid of superscripts 
            # pymupdf doesn't have an easy way to determine if 
            # a piece of text is a superscript
            operands = operands.replace("81", "8")
            operands = operands.replace("82", "8")
            operands = operands.replace("*", "")
            operands = operands.replace("41", "4")

            file.write(encoding + enc_padding * " " + '|' + opcode + padding * " " + "|" + operands + "\n")
        else:
            # only support 64 bit instructions
            if row[3][0] != 'V':
                continue    

            encoding = row[2]

            row[1] = row[1].replace("*", "")
            row[1] = row[1].replace("\n", "")
            row[1] = row[1].strip()
            # try to get rid of the superscripts

            row[0] = row[0].replace("/r1", "/r")
            row[1] = row[1].replace("81", "8")
            row[1] = row[1].replace("82", "8")
            row[1] = row[1].replace("g2", "g")
            row[1] = row[1].replace("83", "8")
            row[1] = row[1].replace("63", "6")
            row[1] = row[1].replace("23", "2")
            row[1] = row[1].replace("43", "4")
            row[1] = row[1].replace("42", "4")
            row[1] = row[1].replace("62", "6")
            row[1] = row[1].replace("61", "6")
          
            # fpu instructions don't have encodings specified
            # in the tables 
            if row[1][0].lower() == "f":
                encoding = "FPU"

            encoding = encoding.replace('\n','')

            if encoding in unknown_encodings:
                pass

            if encoding not in encodings:
                encodings.append(encoding)

            
            enc_padding = enc_padding_size - len(encoding)
            padding= pad_size - len(row[0]) - len(encoding) - enc_padding - 1

            file.write(encoding + enc_padding * ' ' + '|' + row[0] + padding * " " + "|" + row[1] + "\n")


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

        if table_finder.tables == []:
           continue 
        op_table = table_finder.tables[0]


        op_table = op_table.extract()

        if write_opcode_table(inst, out, op_table, table_finder.tables, page_num):
            prev_page = inst
            if should_add_unkown:
                known_pages.append(page_num)
            instructions[inst] = op_table
        else:
            unsupported.append(inst)
    elif prev_page != None:
        # some tables go to the next page
        table_finder = page.find_tables()
        if table_finder.tables == []:
           continue 
        op_table = table_finder.tables[0].extract()

        # check if its an opcode table
        if "Opcode" in op_table[0][0]:
            if should_add_unkown:
                known_pages.append(page_num)
            write_opcode_table(None, out, op_table, table_finder.tables, page_num) 

    else:
        prev_page = None
out.close()

if should_add_unkown:
    print(known_pages)
print(encodings)

