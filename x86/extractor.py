import pymupdf


instructions = {}



unsupported = {}


letters = []

doc = pymupdf.open("intel.pdf") # open a document


out = open("instructions.dat", "w") # create a text output

def write_opcode_table(file, op_table):
    in_op_en = False

    if "/" in op_table[0][0]:
        in_op_en = True

    for i in range(1,len(op_table)):
        row = op_table[i]

        if row[1] == None:
            break

        if in_op_en:
            nl = row[0].find('\n')
            padding = 25 - len(row[0][:nl])
            row[0] = row[0].replace("\n", padding * " " + "|")
            file.write(row[0] + row[1] + "\n")
        else:
            row[1] = row[1].replace("*", "")
            row[1] = row[1].replace("\n", "")
            padding= 25 - len(row[0])
            file.write(row[0] + padding * " " + "|" + row[1] + "\n")



#128,638
#638,2266

prev_page = None
for page in doc.pages(128, 2266): # iterate the document pages
    words = page.get_text("words") # get plain text (is in UTF-8)

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

        exten = False 
        #if "CPUID Feature Flag"
        if 'CPUID' in op_table[0][3]:
            # not gonna support these instructions yet
            unsupported[inst] = op_table
            prev_page = None
            continue 


        prev_page = inst

        out.write(inst + "\n")

        write_opcode_table(out, op_table)
        instructions[inst] = op_table
    elif prev_page != None:
        # some tables go to the next page
        tables = page.find_tables()
        if tables.tables == []:
           continue 
        op_table = tables.tables[0].extract()

        # check if its an opcode table
        if "Opcode" in op_table[0][0]:
            write_opcode_table(out, op_table) 

    else:
        prev_page = None
out.close()
