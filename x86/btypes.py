from enum import IntEnum, auto

# 32 bit bit field
class InstructionFlags:
    # opcode size be the first 2 bits
    # allowed prefixes
    REX = 1 << 2
    TWO_BYTE_VEX = 1 << 3
    THREE_BYTE_VEX = 1 << 4
    EVEX = 1 << 5
    LOCK = 1 << 6
    REP  = 1 << 7
    REPE  = 1 << 8
    # 64 bit mode for rex
    # opcode specific for vex/evex
    REX_W  = 1 << 9
    VEX_W  = REX_W
    EVEX_W = REX_W
    # opcode extension
    VEX_P0 = 1 << 10
    VEX_P1 = 1 << 11
    EVEX_P0 = VEX_P0
    EVEX_P1 = VEX_P1
    # Length
    VEX_L = 1 << 12
    EVEX_L0 = VEX_L
    EVEX_L1 = 1 << 13
    # Opcode Map
    VEX_M0  = 1 << 14
    VEX_M1  = 1 << 15
    VEX_M2  = 1 << 16
    VEX_M3  = 1 << 17
    VEX_M4  = 1 << 18
    EVEX_M0  = VEX_M0
    EVEX_M1 =  VEX_M1
    EVEX_M2 =  VEX_M2
    # Opcode extension is reg part of modrm
    OPCODE_EXTENSION = 1 << 19
    DIGIT0 = 1 << 20
    DIGIT1 = 1 << 21
    DIGIT2 = 1 << 22
    REQURIES_SIB = 1 << 23
    # Instruction Mode Information
    VALID_64_B0  = 1 << 24
    VALID_64_B1  = 1 << 25




TOKENS = {
  "NEW_LINE": '\\n',
  "OPENING_PAREN": '(',
  "CLOSING_PAREN": ')',
  "MULTIPLY": '*',
  "DIVIDE": '/',
  "OR": '|',
  "MOD": '%',
  "XOR": '^',
  "AND": '&',
  "NEG": '~',
  "ADD": '+',
  "SUB": '-' ,
  "COMMA": ',',
  "COLON": ':',
  "OPENING_BRACKET": '[',
  "CLOSING_BRACKET": ']',
  "INSTRUCTION": 256,
  # need to stay in this order
  # ----------
  "DREG" : None,
  "CREG" : None,
  "SREG": None,
  "TREG": None,
  "BNDREG": None,
  "REG": None,
  # ----------
  "INT": None,
  "IDENTIFIER": None,
  "NSTRING": None,
  "STRING": None,
  "LSHIFT": None,
  "RSHIFT": None,
}

OPERAND_ENCODINGS = [
  "ZO",
  "I",
  "MI",
  "MR",
  "RM",
  "RVM",
  "RMI",
  "RMV",
  "RVMI",
  "VMI",
  "RM0",
  "RVMR",
  "VM",
  "O",
  "D",
  "M",
  "II",
  "FPU",
  "R",
  "FD",
  "TD",
  "OI",
  "M1",
  "MC",
  "MRI",
  "MRC",
  "MVR",
  "RVSV",
]

class OperandType(IntEnum):
    NOP = 0
    # --- Registers ---
    R8   = 1 << 0
    R16  = 1 << 1
    R32  = 1 << 2
    R64  = 1 << 3
    MM   = 1 << 4
    XMM  = 1 << 5
    YMM  = 1 << 6
    BND  = 1 << 7
    # --- Memory Addresses ---
    M8   = 1 << 8
    M16  = 1 << 9
    M32  = 1 << 10
    M64  = 1 << 11
    M128 = 1 << 12
    M256 = 1 << 13
    M512 = 1 << 14
    M80  = 1 << 15
    # --- Register Memory ---
    RM8     = R8  | M8
    RM16    = R16 | M16
    RM32    = R32 | M32
    RM64    = R64 | M64
    MMM32   = MM  | M32
    MMM64   = MM  | M64
    XMMM8   = XMM | M8
    XMMM16  = XMM | M16
    XMMM32  = XMM | M32
    XMMM64  = XMM | M64
    XMMM128 = XMM | M128
    YMMM256 = YMM | M256
    BNDM128 = BND | M128
    # --- Misc ---
    MEM_ANY   = M8  | M16 | M32 | M64 | M128 | M256 | M512 | M80
    R32R64    = R32 | R64
    R32M8     = R32 | M8
    R32M16    = R32 | M16
    R32R64M8  = M8  | R32 | R64
    R32R64M16 = M16 | R32 | R64
    R32R64M32 = M32 | R32 | R64

    # --- Lower 20 bits are a bit mask --
    REL8 = 1 << 21
    REL16 = auto()
    REL32 = auto()
    LABEL = auto() # label
    M    = auto()
    SREG  = auto()
    CREG = auto()
    CR8  = auto()
    DREG = auto()
    IMM8  = auto()
    IMM16 = auto()
    IMM32 = auto()
    IMM64 = auto()
    SIMM8 = auto()
    SIMM16 = auto()
    SIMM32 = auto()
    SIMM64 = auto()
    STI = auto()
    TMM = auto()
    MOFFSET = auto()
    FS    = auto()
    GS    = auto()
    AL = auto()
    CL = auto()
    AX = auto()
    DX = auto()
    EAX = auto()
    RAX = auto()
    UNSUPPORTED = 1 << 31 

REGISTERS = [
  "YMM0",
  "YMM1",
  "YMM2",
  "YMM3",
  "YMM4",
  "YMM5",
  "YMM6",
  "YMM7",
  "YMM8",
  "YMM9",
  "YMM10",
  "YMM11",
  "YMM12",
  "YMM13",
  "YMM14",
  "YMM15",
  "XMM0",
  "XMM1",
  "XMM2",
  "XMM3",
  "XMM4",
  "XMM5",
  "XMM6",
  "XMM7",
  "XMM8",
  "XMM9",
  "XMM10",
  "XMM11",
  "XMM12",
  "XMM13",
  "XMM14",
  "XMM15",
  "MM0",
  "MM1",
  "MM2",
  "MM3",
  "MM4",
  "MM5",
  "MM6",
  "MM7",
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

SREG = [
  "ES",
  "CS",
  "SS",
  "DS",
  "FS",
  "GS"
]

TREG = [
  "TMM0",
  "TMM1",
  "TMM2",
  "TMM3",
  "TMM4",
  "TMM5",
  "TMM6",
  "TMM7"
]

BNDREG = [
  "BND0",
  "BND1",
  "BND2",
  "BND3"
]

CREG = [
  "CR0",
  "CR1",
  "CR2",
  "CR3",
  "CR4",
  "CR5",
  "CR6",
  "CR7",
  "CR8",
]

DREG = [
  "DR0",
  "DR1",
  "DR2",
  "DR3",
  "DR4",
  "DR5",
  "DR6",
  "DR7",
]
