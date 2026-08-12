# x86_64 Instruction Tables
This directory is responsible for extracting instruction metadata from the official intel
documentation and building an opcode table as well as a hashmap.

## Files
- `extractor.py`
    - Attempts to download the Official Intel Instruction Set Reference A-Z
    - Extracts instructions and instruction metadata from `intel.pdf` organizes it into a 
    human readable, easy to parse file `instructions.dat`
    - Requires Pymupdf to read the pdf
    - Not very fast. Not recommended to be reran after instructions.dat has been generated
- `generate_table.py`
    - Generates C enums for tokens, keywords, registers, operand encodings, and operands
    - Defines to_string functions for tokens and operands for debug messages
    - Parses `instructions.dat`
    - Uses Gperf to generate a perfect static hashmap for all keywords, registers, instructions, etc.
    - `bkeywords.py`
        - Holds a list of all keywords
    - `btypes.py`
        - Defines tokens, registers, operand encodings, and operands
    - `parse_instr.py`
        - Defines functions to parse `instructions.dat` and convert that information to C structs
    - `gentypes.h`
        - File that contains the C enums for the registers, operands, tokens, and operand encodings
    - `print_instr.h`
        - Useful debug function that will print out the instruction struct
        - Gets included at the end of `instructions.c`
    - `instructions.c`
        - File that contains generated perfect hashmap as well as the to_string functions along 
        with any other constant defined in types.h
- `types.h`
    - Holds instruction struct declarations along with function/constant definitions 
    that will be used in the assembler

## Notes
- `extractor.py` and `generate_table.py` are the only 2 files that should be ran directly
    - `extractor.py` relies on [PyMuPDF](https://pypi.org/project/PyMuPDF/)
        - Only Needs to be reran if new instructions are added (AVX512)
    - `generate_table.py` relies on [gperf](https://www.gnu.org/software/gperf/) and is expected to be in PATH
        - Needs to be reran if a new keyword is added

> [!IMPORTANT]
> If the download fails its likely due to a dead link.
> You should be able to find the updated documentation here [Intel Docs](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html).
> Only the `Instruction Set Reference A-Z (Volume 2)` is required.
> Downloading a newer reference may introduce breaking changes to the script. The script was last updated with June 2026 Reference.


> [!IMPORTANT]  
> `generate_table.py` will produce ~50 error messages saying operand not supported. This is fine right now. You just won't be able to use these variants of those instructions. The reason I have not implemented these operands is because I am not sure what kind of syntax I want to use for some of these niche operand types.
