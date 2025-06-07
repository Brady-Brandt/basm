import pymupdf


instructions = {}



unsupported = {}


letters = []

known_pages = [136, 139, 141, 143, 146, 149, 151, 153, 155, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 185, 187, 189, 190, 193, 196, 199, 204, 205, 207, 209, 211, 214, 215, 216, 217, 219, 221, 224, 226, 229, 234, 236, 238, 239, 241, 243, 245, 247, 248, 265, 266, 267, 268, 269, 271, 273, 275, 277, 279, 280, 281, 283, 284, 285, 286, 288, 290, 297, 304, 308, 312, 317, 319, 322, 324, 326, 371, 374, 385, 386, 390, 391, 392, 395, 398, 399, 401, 403, 405, 407, 409, 411, 415, 416, 419, 420, 422, 424, 429, 431, 434, 437, 440, 442, 444, 446, 449, 450, 452, 454, 455, 456, 459, 462, 465, 467, 469, 470, 473, 475, 477, 479, 481, 483, 489, 491, 492, 495, 498, 499, 501, 503, 504, 506, 509, 511, 513, 515, 517, 519, 522, 523, 525, 527, 529, 531, 532, 534, 537, 539, 541, 543, 545, 547, 549, 551, 553, 556, 559, 561, 563, 565, 567, 570, 578, 580, 582, 584, 587, 589, 591, 594, 597, 598, 600, 603, 606, 609, 613, 615, 617, 619, 622, 640, 642, 644, 647, 656, 657, 658, 661, 670, 672, 673, 674, 676, 677, 678, 680, 682, 684, 686, 687, 688, 690, 693, 695, 696, 700, 703, 705, 707, 708, 711, 713, 715, 718, 720, 723, 726, 729, 731, 741, 743, 745, 748, 751, 753, 755, 756, 759, 762, 764, 766, 768, 772, 775, 777, 781, 785, 788, 791, 794, 796, 798, 803, 804, 810, 811, 813, 815, 817, 819, 821, 823, 825, 827, 829, 831, 833, 835, 837, 838, 841, 843, 847, 850, 853, 856, 859, 861, 865, 869, 871, 879, 881, 884, 887, 889, 891, 893, 896, 898, 899, 901, 903, 906, 909, 911, 915, 916, 921, 922, 929, 934, 939, 940, 946, 950, 954, 957, 960, 963, 964, 968, 972, 975, 978, 979, 984, 987, 989, 991, 992, 997, 1000, 1002, 1004, 1010, 1012, 1014, 1017, 1020, 1024, 1026, 1028, 1031, 1033, 1036, 1038, 1041, 1044, 1045, 1051, 1056, 1060, 1065, 1069, 1074, 1078, 1080, 1081, 1089, 1090, 1091, 1099, 1102, 1106, 1110, 1114, 1118, 1122, 1125, 1132, 1134, 1138, 1141, 1143, 1145, 1149, 1153, 1157, 1160, 1163, 1164, 1168, 1170, 1171, 1172, 1182, 1183, 1192, 1194, 1195, 1196, 1206, 1207, 1214, 1217, 1221, 1225, 1227, 1229, 1230, 1239, 1240, 1249, 1254, 1256, 1259, 1260, 1264, 1266, 1268, 1270, 1272, 1273, 1275, 1277, 1279, 1281, 1283, 1285, 1287, 1288, 1291, 1305, 1308, 1310, 1312, 1314, 1316, 1318, 1320, 1325, 1326, 1330, 1332, 1334, 1337, 1341, 1343, 1344, 1345, 1347, 1349, 1350, 1352, 1354, 1355, 1356, 1357, 1359, 1360, 1361, 1364, 1367, 1372, 1376, 1378, 1380, 1382, 1385, 1388, 1390, 1392, 1393, 1394, 1395, 1397, 1398, 1401, 1403, 1405, 1406, 1408, 1411, 1414, 1416, 1418, 1420, 1423, 1426, 1429, 1432, 1434, 1436, 1438, 1439, 1441, 1442, 1443, 1444, 1446, 1448, 1450, 1452, 1453, 1455, 1457, 1459, 1463, 1467, 1471, 1480, 1482, 1483, 1486, 1488, 1489, 1496, 1498, 1500, 1502, 1504, 1506, 1508, 1510, 1512, 1514, 1518, 1520, 1522, 1524, 1528, 1530, 1532, 1534, 1536, 1538, 1542, 1544, 1546, 1549, 1551, 1553, 1555, 1557, 1558, 1560, 1561, 1562, 1563, 1564, 1566, 1567, 1569, 1573, 1575, 1577, 1579, 1581, 1583, 1585, 1587, 1589, 1591, 1593, 1594, 1595, 1596, 1597, 1599, 1601, 1603, 1605, 1607, 1609, 1611, 1613, 1615, 1617, 1619, 1622, 1624, 1625, 1627, 1629, 1631, 1633, 1639, 1645, 1648, 1650, 1653, 1655, 1659, 1663, 1666, 1669, 1676, 1677, 1682, 1688, 1691, 1694, 1697, 1698, 1704, 1709, 1716, 1722, 1723, 1728, 1734, 1737, 1740, 1743, 1744, 1750, 1755, 1762, 1763, 1769, 1775, 1778, 1781, 1782, 1788, 1794, 1797, 1800, 1803, 1806, 1808, 1810, 1811, 1813, 1817, 1821, 1824, 1827, 1830, 1833, 1837, 1839, 1841, 1843, 1847, 1851, 1854, 1856, 1858, 1860, 1864, 1868, 1871, 1873, 1875, 1877, 1879, 1881, 1882, 1884, 1885, 1887, 1889, 1891, 1894, 1897, 1898, 1906, 1908, 1911, 1914, 1917, 1920, 1923, 1925, 1927, 1930, 1932, 1934, 1936, 1938, 1940, 1942, 1944, 1947, 1949, 1950, 1955, 1960, 1965, 1968, 1971, 1974, 1976, 1981, 1984, 1986, 1988, 1992, 1995, 1999, 2002, 2005, 2007, 2009, 2012, 2015, 2019, 2023, 2026, 2030, 2034, 2038, 2042, 2044, 2047, 2051, 2055, 2059, 2062, 2065, 2068, 2071, 2072, 2077, 2082, 2087, 2090, 2093, 2096, 2100, 2103, 2106, 2109, 2111, 2113, 2115, 2117, 2119, 2120, 2123, 2126, 2128, 2130, 2132, 2134, 2137, 2140, 2143, 2145, 2147, 2149, 2151, 2153, 2155, 2157, 2159, 2160, 2163, 2165, 2168, 2170, 2172, 2174, 2178, 2183, 2185, 2186, 2188, 2189, 2192, 2193, 2194, 2197, 2198, 2200, 2202, 2204, 2206, 2208, 2210, 2212, 2218, 2220, 2223, 2225, 2227, 2229, 2231, 2233, 2236, 2239, 2240, 2245, 2249, 2252, 2255, 2258, 2261, 2263, 2264]


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

        """
        if 'CPUID' in op_table[0][3]:
            # not gonna support these instructions yet
            unsupported[inst] = op_table
            prev_page = None
            continue 
        """


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
