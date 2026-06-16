from dataclasses import dataclass
from btypes import OperandType
from btypes import InstructionFlags


@dataclass
class Instruction:
    opcode: list[int]
    imm_size: int
    has_modrm: bool
    uses_rex: int
    two_byte_vex: int
    three_byte_vex: int
    evex: int
    rve_w: int
    pp: int
    ll: int
    mmmmm: int
    has_opcode_ext: int
    digit: int
    requires_sib: int
    op1: int = 0
    op2: int = 0
    op3: int = 0
    encoding: str = ''
    lock = 0
    rep = 0
    repe = 0
    valid_64_b0  = 0
    valid_64_b1  = 0

    def get_size(self) -> int:
        size = len(self.opcode)
        size += self.imm_size
        if self.uses_rex:
            size += 1
        if self.has_opcode_ext or self.has_modrm:
            size += 1
        if self.two_byte_vex:
            size += 2
        if self.three_byte_vex:
            size += 3
        return size

    def to_c_struct(self, variant_count):
        # hreset has an implicit operand eax which is already stored in the modrm byte in the manual
        # because of this, this actually comes out to be a 5 byte opcode
        # But since it already is in the modrm byte I am just going to treat it as an opcode extension
        if len(self.opcode) > 4:
            print("Warning hreset won't work right now")
            self.opcode = self.opcode[:4]

        op_len = len(self.opcode)
        if op_len > 4:
            assert op_len <= 5, f"Opcode Too long {self.opcode}"
            op_len = 4
        opcode = "{"
        for i in range(4):
            if i < len(self.opcode):
                opcode += hex(self.opcode[i]) + ','
            else:
                opcode += "0x00" + ','
        # remove trailing comma
        opcode = opcode[:-1]
        opcode += '}'

        operands = '{' + f"(OperandType){self.op1}, (OperandType){self.op2}, (OperandType){self.op3}"

        # button 2 bits is size
        flags = (op_len - 1)
        if self.uses_rex:
            flags |= InstructionFlags.REX
        if self.two_byte_vex:
            flags |= InstructionFlags.TWO_BYTE_VEX
        if self.three_byte_vex:
            flags |= InstructionFlags.THREE_BYTE_VEX
        if self.evex:
            flags |= InstructionFlags.EVEX
        # TODO STORE WHETHER A PREFIX IS ALLOWED HERE
        #LOCK = 1 << 6
        #REP  = 1 << 7
        #REPE  = 1 << 8
        if self.rve_w:
            flags |= InstructionFlags.REX_W

        if self.pp & 1:
            flags |= InstructionFlags.VEX_P0
        if self.pp & 2:
            flags |= InstructionFlags.VEX_P1

        if self.ll & 1:
            flags |= InstructionFlags.EVEX_L0
        if self.ll & 2:
            flags |= InstructionFlags.EVEX_L1

        if self.mmmmm & 1:
            flags |= InstructionFlags.VEX_M0
        if self.mmmmm & 2:
            flags |= InstructionFlags.VEX_M1
        if self.mmmmm & 4:
            flags |= InstructionFlags.VEX_M2
        if self.mmmmm & 8:
            flags |= InstructionFlags.VEX_M3
        if self.mmmmm & 16:
            flags |= InstructionFlags.VEX_M4

        if self.has_opcode_ext:
            flags |= InstructionFlags.OPCODE_EXTENSION

        if self.digit & 1:
            flags |= InstructionFlags.DIGIT0
        if self.digit & 2:
            flags |= InstructionFlags.DIGIT1
        if self.digit & 4:
            flags |= InstructionFlags.DIGIT2

        if self.requires_sib:
            flags |= InstructionFlags.REQURIES_SIB

        # STORE DEFAULT OPERAND SIZE IN THE FLAGS
        #VALID_64_B0  = 1 << 24
        #VALID_64_B1  = 1 << 25

        reserved0 = 0
        reserved1 = 0

        return f"{operands}, {opcode}, {flags}, {reserved0}, {reserved1}, {self.encoding}, {variant_count}" + '},'


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
    #VEX.128.66.0F 38.WIG 35 /r VPMOVZXDQ xmm1, xmm2/m64
    #another unwanted space
    new_op = new_op.replace("66.0F 38.","66.0F38.")
    
    chunks = new_op.split(' ')
   
    
    opcode: list[int] = []
    imm_size: int = 0
    has_modrm: bool = False
    uses_rex: int  = 0
    two_byte_vex: int = 0
    three_byte_vex: int = 0
    evex: int = 0
    rve_w: int = 0
    pp: int = 0
    ll: int = 0
    mmmmm: int = 0
    has_opcode_ext: bool = False
    digit: int = 0
    requires_sib: int = 0



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
                    digit = int(chunk[1:]) 
                    has_opcode_ext = True
                elif chunk[1] == "r":
                    has_modrm = True
                elif chunk[1:] == 'is4' or chunk[1:] == 'ib':
                    imm_size = 1
                else:
                    print(f"Failed: {chunk} in {chunks}")
            except IndexError:
                if chunks[1] == '6E':
                    # again more inconsistency 
                    has_modrm = True
                    pass
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
            uses_rex = 1
            if chunk[-1].lower() == "w":
                rve_w = 1

        elif "VEX" in chunk:
            chunk = chunk.strip()
            vex_encoding = chunk.split('.')
            vex_encoding = vex_encoding[1:]

            is_three_byte = False
            
            for enc in vex_encoding:
                if enc == "128" or enc == "LZ" or enc == "L0" or enc == "LIG":
                    ll = 0
                elif enc == "256" or enc == "L1":
                    ll = 1
                elif enc == "66":
                    pp = 1
                elif enc == "F3":
                    pp = 2
                elif enc == "F2":
                    pp = 3
                elif enc == "0F":
                    is_three_byte = True
                    mmmmm = 1
                elif enc == "0F38":
                    is_three_byte = True
                    mmmmm = 2
                elif enc == "0F3A":
                    is_three_byte = True
                    mmmmm = 3
                elif enc == "W0" or enc == "0":
                    is_three_byte = True
                    rve_w = 0
                elif enc == "W1":
                    is_three_byte = True
                    rve_w = 1
                elif enc == "660F":
                    #missing period between 66 & 0F
                    pp = 1
                    is_three_byte = True
                    mmmmm = 1
                elif enc == "WIG":
                    pass 
                elif enc == "NP":
                    #ignore this for now
                    pass
                else:
                    print(f"INVALID VEX PREFIX: {new_op}")
            
            if is_three_byte:
                three_byte_vex = 1
            else:
                two_byte_vex = 1

        elif chunk == "ib" or chunk == "ib1" or chunk == "imm8":
            imm_size = 1
        elif chunk == "iw":
            imm_size = 2
        elif chunk == "id":
            imm_size = 4
        elif chunk == "io":
            imm_size = 8
        elif chunk == "cb":
            imm_size = 1
        elif chunk == "cw":
            imm_size = 2
        elif chunk == "cd":
            imm_size = 4
        elif chunk == "cp":
            imm_size = 6
        elif chunk == "co":
            imm_size = 8
        elif chunk == "ct":
            imm_size = 10

        elif ':' in chunk:
            # just going to ignore the mod portion 
            # of modrm since that information is implied within the operands
            modrm_layout = chunk.split(':')
            reg_portion = modrm_layout[1]
            rm_portion = modrm_layout[2]

            try:
                # this is essentially just an opcode extension
                digit = int(reg_portion, 2)
                has_opcode_ext = True
            except ValueError:
                pass

            try:
                int(rm_portion, 2)

                if rm_portion == "000":
                    pass
                # not really sure what to do here yet
                print("Here Failed", chunk)
                return None
            except ValueError:
                pass
        #this is implied within the operands
        elif chunk == "(mod=11)":
            continue
 
        else:
            try:
                #VCVTTSS2S variants of this instruction have a 
                # superscript that gets interpreted as part of the opcode when its not
                if has_modrm:
                    continue
                #PAVGB unwanted comma
                chunk = chunk.replace(',', '')
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
                # RSTORSSP, SAVEPREVSSP
                # technically RSTORSSP has a modrm but it doesn't matter
                # since we only use that for sorting and there is only one RSTORSSP instr
                if chunk == "(mod!=11":
                    break
                print(f"Failed: {chunk} in {chunks}")
                return None

        prev = chunk

    return Instruction(
            opcode,
            imm_size,
            has_modrm,
            uses_rex,
            two_byte_vex,
            three_byte_vex,
            evex,
            rve_w,
            pp,
            ll,
            mmmmm,
            has_opcode_ext,
            digit,
            requires_sib
        )



def check_operand(nmemonic, op): 
    if op == 'm2byte':
        return OperandType.M16

    # for the registers xmm, mm, ymm,zmm
    # there doesn't need to be a number at the end
    # these instructions can take in any register of that type
    if op[0] != 'i' and op[0] != 's':
        op = op.replace("mm1", "mm")
        op = op.replace("mm2", "mm")
        op = op.replace("mm3", "mm")
        op = op.replace("mm4", "mm")

    # the a's and b's don't mean anything to us and so we get rid of them 
    if op == 'r32a' or op == 'r32b' or op == 'r64a' or op == 'r64b':
        op = op[:-1]
    if op.startswith("m80"):
        op = op[:3]
    if op.startswith("bnd1"):
        op = op.replace("bnd1", "bnd") 
    if op.startswith("bnd2"):
        op = op.replace("bnd2", "bnd")

    if op == 'mem':
        if nmemonic == 'LDDQU':
            return OperandType.M128
        else:
            # XSTOR,XSAVE instructions 
            return OperandType.MEM_ANY 

    if 'moff' in op:
        return OperandType.MOFFSET

    if op == "r64/m64":
        return OperandType.RM64
    if op == "r32/m32":
        return OperandType.RM32
    if op == "r16/m16":
        return OperandType.RM16
    if op == "m384" or op == 'm14/28byte' or op == 'm94/108byte' or op == 'm512byte':
        return OperandType.MEM_ANY 
    # implicit defined in instruction encoding so we 
    # treat them like no operand
    if op.startswith("<XMM") or op == "<YMM0>" or op.lower() == '<eax>' or op=='<edx>':
        return OperandType.NOP
    if op == "ST(0)": 
        # these instructions operate on the fpu stack
        # meaning they don't have operands
        return OperandType.NOP
    # convert fpu memory types to just regular memory types 
    # since there really is no difference between them
    elif op == "m16int":
        return OperandType.M16
    elif op == "m32fp" or op == "m32int":
        return OperandType.M32
    elif op == "m64fp" or op == "m64int":
        return OperandType.M64
    if op == "ST(i)":
        return OperandType.STI

    # handles mov & smsw
    # Ignore the m16 portion of r64/m16.
    # The memory form is encoded by a previous opcode variant.
    if op == "r64/m16":
        return OperandType.R64
    # handles mov same reason as above
    if op == "r16/r32/m16":
        return OperandType.R32

    # hanldes PEXTRB & VPEXTRB
    if op == "reg/m8":
        return OperandType.R32R64M8

    # hanldes PEXTRW & VPEXTRW
    if op == "reg/m16":
        return OperandType.R32R64M16

    if nmemonic == "SENDUIPI":
        return OperandType.R64

    if op == "reg/m32":
        return OperandType.R32R64M32

    if nmemonic == "LAR" and op == "reg":
        return OperandType.R32

    # MOVMSKPD, VMOVMSKPD, MOVMSKPS, VMOVMSKPS, PEXTRW, VPEXTRW, PMOVMSKB
    if op == "reg":
        return OperandType.R32R64

    if op == "CR0–CR7":
        return OperandType.CREG

    if op == "DR0–DR7":
        return OperandType.DREG

    op = op.replace('/', '')
    try:
        return OperandType[op.upper()]
    except KeyError:
        print(f"Operand Not Supported for instruction {nmemonic}: {op}")
        return OperandType.UNSUPPORTED

@dataclass
class ParsedOperands:
    nmemonic: str
    op1: int
    op2: int
    op3: int
    op4: int
    
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

    # skip over rep prefix
    for x in op_format_list:
        if "REP" in x:
            return None
 
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
        if op1_type == OperandType.NOP and op2_type == OperandType.STI:
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
