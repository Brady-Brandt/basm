import pymupdf


instructions = {}



unsupported = []


letters = []

known_pages = []


doc = pymupdf.open("intel.pdf")


out = open("instructions.dat", "w") 
#out = open("temp.dat", "w") # create a text output

def write_opcode_table(instruction, file, op_table):
    in_op_en = False

    pad_size = 40


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
            padding = pad_size - len(row[0][:nl])



            tmp = row[0].split('\n')
            # make sure the entire instruction is on one line
            if len(tmp) > 2:
                for i in range(2, len(tmp)):
                    tmp[1] += tmp[i]
                

            opcode = tmp[0]
            operands = tmp[1]
 
            operands = operands.strip()
            # trying to get rid of superscripts 
            # pymupdf doesn't have an easy way to determine if 
            # a piece of text is a superscript
            operands = operands.replace("81", "8")
            operands = operands.replace("82", "8")
            operands = operands.replace("*", "")
            operands = operands.replace("41", "4")

            file.write(opcode + padding * " " + "|" + operands + "\n")
        else:
            # only support 64 bit instructions
            if row[3][0] != 'V':
                continue    


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
            



            
            padding= pad_size - len(row[0])
            file.write(row[0] + padding * " " + "|" + row[1] + "\n")


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
    #page = doc.load_page(page_num)

    words = page.get_text("words") 

    inst = words[0][4]

    
    if chr(0x2014) not in inst:
        continue

    if inst not in instructions and inst not in unsupported:
        if inst[0] not in letters:
            print(inst[0])
            letters.append(inst[0])


        tables = page.find_tables()

        if tables.tables == []:
           continue 
        op_table = tables.tables[0]


        op_table = op_table.extract()

        if write_opcode_table(inst, out, op_table):
            prev_page = inst
            if should_add_unkown:
                known_pages.append(page_num)
            instructions[inst] = op_table
        else:
            unsupported.append(inst)
    elif prev_page != None:
        # some tables go to the next page
        tables = page.find_tables()
        if tables.tables == []:
           continue 
        op_table = tables.tables[0].extract()

        # check if its an opcode table
        if "Opcode" in op_table[0][0]:
            if should_add_unkown:
                known_pages.append(page_num)
            write_opcode_table(None, out, op_table) 

    else:
        prev_page = None
out.close()

if should_add_unkown:
    print(known_pages)
