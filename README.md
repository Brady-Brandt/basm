# Basm 
Basm is an x86_64 compiler that uses syntax similiar to NASM. It currently supports around 300 instructions and can only assemble instructions 
in long mode (64 bit mode). 

## Getting Started
1. Clone the Repo
```sh
git clone https://github.com/Brady-Brandt/basm.git
```
2. Make the bin folder
```sh
mkdir bin
```
3. Run Make (Uses GCC but any compiler should work for now)
```sh
make
```

## First Program
### Hello World
```asm
; file hello.asm
section .data 
    hello: db "Hello, World!\n" ; Strings are automatically null terminated
section .text
; every statement must be inside section block 
global _start
extern printf 
extern exit
_start:
    ; Uses System V ABI (Linux)
    lea rdi, [hello]  
    call printf

    xor rdi,rdi
    call exit
```
### Assemble
The -f flag specifies the output file. 
Right now we only support elf and windows object files. 
```sh
 bin/basm -f elf hello.asm -o hello.o
```
### Linking
Linking can be done with any linker. 
To link the above program with libc on Linux
We can use the following command (Tested on Arch Only): 
```sh
ld hello.o -o hello -dynamic-linker /lib64/ld-linux-x86-64.so.2 -lc -m elf_x86_64
```

## Extra Info
Basm is able to assemble some code but there are still a lot of incomplete features and bugs. 
It should only be used for simple, hobby projects right now. 

### Known Issues
- Can't use both a label and an offset when addressing (ex. mov rax, [label + 100] won't work yet)
- Moving a label as in (mov rax, label) does not move the address of the label into rax
- Windows doesn't support file names larger than 18 chars or labels larger than 8
- Currently assume all jump addresses are rel32
- Some instructions cannot infer the size of the memory operand
- Issues regarding immediate sizes 

### Planned Features (Not in a Particular Order) 
- [ ] Mach File Support (Macos Support)
- [x] SSE/AVX instructions (Most have been added, not supporting EVEX right now)
- [ ] Preprocessor
- [ ] Support for instruction prefixes
- [ ] Implementing Hashmaps for the Symbol Tables
- [ ] Allow multiple assembler passes
 
