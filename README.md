# Basm 
Basm is an x86_64 compiler that uses syntax similiar to NASM. It currently supports around 1100 instructions and can only assemble instructions 
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
#if __ELF__
    #define p1 rdi
    #define __puts__ puts
    #define __exit__ exit
    #define entry _start
#elif __WIN__
    #define p1 rcx
    #define __puts__ puts
    #define __exit__ exit
    #define entry main
#elif __MACHO__
    #define p1 rdi
    #define __puts__ _puts
    #define __exit__ _exit
    #define entry _start
#endif

extern __puts__ 
extern __exit__
global entry

entry:
    ; use relative addressing because macos
    ; won't allow 64 bit absolute addresses
    lea p1, [rel hello]
    call __puts__ 

    xor p1,p1
    #if __WIN__
        push p1
    #endif
    call __exit__

section .data 
    hello: db "Hello, World!" ; Double quoted strings are null terminated
```
### Assemble
The -f flag specifies the output file. 
Right now we only support elf, windows, and macos object files. 
```sh
 bin/basm -f elf hello.asm -o hello.o
```
### Linking
To link the above program with libc on Linux
We can use the following command (Tested on Arch Only): 
```sh
ld hello.o -o hello -dynamic-linker /lib64/ld-linux-x86-64.so.2 -lc -m elf_x86_64
```

## Extra Info
Basm is able to assemble some code but there are still a lot of incomplete features and bugs. 
It should only be used for simple, hobby projects right now. 

### Known Issues
- Using the times psuedoinstruction with a multiline macro will only repeat the first instruction of the macro
- Moving a label as in (mov rax, label) does not move the address of the label into rax
- Currently assume all jump addresses are rel32

### Planned Features
- [x] Macho File Support (Macos Support)
- [x] SSE/AVX instructions (Most have been added, not going to support AVX512 instructions)
- [ ] Preprocessor
- [x] Support for instruction prefixes
- [ ] Implementing Hashmaps for the Symbol Tables
- [ ] Allow multiple assembler passes
- [ ] Debug Symbols
- [ ] 16 & 32 bit Instructions
 
