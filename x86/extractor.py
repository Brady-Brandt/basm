import pymupdf


instructions = {}



unsupported = {}


letters = []

known_pages = [136, 141, 187, 234, 236, 238, 239, 241, 243, 245, 248, 265, 267, 268, 271, 273, 275, 279, 283, 284, 285, 286, 288, 304, 317, 319, 326, 371, 390, 391, 398, 415, 419, 424, 429, 431, 449, 456, 467, 469, 470, 473, 475, 477, 479, 481, 483, 489, 491, 492, 495, 498, 499, 501, 503, 504, 506, 509, 511, 513, 515, 517, 519, 522, 523, 525, 527, 529, 531, 532, 534, 537, 539, 541, 543, 545, 547, 549, 551, 553, 556, 559, 561, 563, 565, 567, 570, 578, 580, 582, 597, 606, 609, 613, 615, 619, 640, 642, 647, 656, 657, 658, 661, 690, 696, 703, 705, 708, 711, 713, 718, 720, 723, 726, 729, 743, 766, 768, 772, 775, 810, 837, 843, 859, 869, 879, 893, 896, 898, 899, 901, 909, 911, 963, 1125, 1132, 1134, 1141, 1163, 1249, 1254, 1259, 1260, 1270, 1273, 1275, 1283, 1285, 1287, 1288, 1291, 1314, 1325, 1326, 1334, 1337, 1344, 1345, 1349, 1350, 1361, 1364, 1376, 1378, 1380, 1393, 1394, 1395, 1398, 1401, 1406, 1418, 1420, 1423, 1426, 1429, 1436, 1452, 1631, 2197, 2198, 2204, 2218, 2223, 2227, 2229, 2231, 2261]

doc = pymupdf.open("intel.pdf")


out = open("instructions.dat", "w") 
#out = open("temp.dat", "w") # create a text output

def write_opcode_table(instruction, file, op_table):
    in_op_en = False


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
            nl = row[0].find('\n')
            padding = 25 - len(row[0][:nl])

            tmp = row[0].split('\n')

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
            



            
            padding= 25 - len(row[0])
            file.write(row[0] + padding * " " + "|" + row[1] + "\n")


    return True



#128, 2266
start = 128
page_num = start

should_add_unkown = True

prev_page = None
#for page_num, page in enumerate(doc.pages(start, 2266)):
#page_num += start
# use this loop when we already know which pages we need to parse
# use the outer for loop when we don't
for page_num in known_pages:
    should_add_unkown = False 
    page = doc.load_page(page_num)

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


        if 'CPUID' in op_table[0][3]:
            # not gonna support these instructions yet
            unsupported[inst] = op_table
            prev_page = None
            continue 


        prev_page = inst
        if write_opcode_table(inst, out, op_table):
            if should_add_unkown:
                known_pages.append(page_num)
            instructions[inst] = op_table
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
