import pymupdf


instructions = {}



unsupported = {}

doc = pymupdf.open("intel.pdf") # open a document
out = open("output.txt", "w") # create a text output
#128,638
for page in doc.pages(128,638): # iterate the document pages
    words = page.get_text("words") # get plain text (is in UTF-8)

    inst = words[0][4]

    
    if chr(0x2014) not in inst:
        continue

    if inst not in instructions and inst not in unsupported:
        print(page)
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
            continue 

        out.write(inst + "\n")

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
                out.write(row[0] + row[1] + "\n")
            else:
                row[1] = row[1].replace("*", "")
                row[1] = row[1].replace("\n", "")
                padding= 25 - len(row[0])
                out.write(row[0] + padding * " " + "|" + row[1] + "\n")


            """
            flag = "null"

            if exten:
                flag = row[3] 

            f_pad = 15 - len(row[1])
            """


        instructions[inst] = op_table
out.close()
