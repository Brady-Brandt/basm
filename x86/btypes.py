from enum import IntEnum, auto

REX = 0x1
TWO_BYTE_VEX = 0x2
THREE_BYTE_VEX = 0x4

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
  "SREG": None,
  "TREG": None,
  "BNDREG": None,
  "REG": None,
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
    REL8 = auto()
    REL16 = auto()
    REL32 = auto()
    SREG = auto()
    FS = auto()
    GS = auto()
    REG = auto()
    R8 = auto()
    R16 = auto()
    R32 = auto()
    R64 = auto()
    MEM_ANY = auto()
    M8 = auto()
    M16 = auto()
    M32 = auto()
    M64 = auto()
    M128 = auto()
    M256 = auto()
    M512 = auto()
    M80 = auto()
    RM8 = auto()
    RM16 = auto()
    RM32 = auto()
    RM64 = auto()
    IMM8 = auto()
    IMM16 = auto()
    IMM32 = auto()
    IMM64 = auto()
    SIMM8 = auto()
    SIMM16 = auto()
    SIMM32 = auto()
    SIMM64 = auto()
    L8 = auto()
    L16 = auto()
    L32 = auto()
    L64 = auto()
    M = auto()
    STI = auto()
    MMM32 = auto()
    MMM64 = auto()
    MM = auto()
    XMM = auto()
    YMM = auto()
    XMMM8 = auto()
    XMMM16 = auto()
    XMMM32 = auto()
    XMMM64 = auto()
    XMMM128 = auto()
    YMMM256 = auto()
    TMM = auto()
    BND = auto()
    BNDM128 = auto()
    MOFFSET = auto()
    AL = auto()
    CL = auto()
    AX = auto()
    DX = auto()
    EAX = auto()
    RAX = auto()
    UNSUPPORTED = 255 

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
