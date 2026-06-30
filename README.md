# Basm 
Basm is an x86_64 assembler that uses syntax similiar to NASM. It currently supports around 1100 instructions and can only assemble instructions 
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
3. Run Make (Use either gcc or clang)
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
The -f flag specifies the output file type.

Options are:
- win   (64 bit Windows COFF file)
- elf   (64 bit elf object file)
- macho (64 bit Macos Macho file)

To assemble the above program for Linux the following command can be used.
```sh
 bin/basm -f elf hello.asm -o hello.o
```
### Linking
Since Basm is only capable of generating object files, a linker is needed to
generate an executable. The program above relies on libc for `puts` and `exit`.
To link the above program with libc on Linux use the following command.
```sh
ld hello.o -o hello -dynamic-linker /lib64/ld-linux-x86-64.so.2 -lc
```
Where:
- hello.o (assembled program)
- /lib64/ld-linux-x86-64.so.2 (path to the dynamic linker. Needed since libc is a shared object)
- -lc (link with libc)

## Extra Info
Basm is able to assemble some code but there are still a lot of incomplete features and bugs. 
It should only be used for simple, hobby projects right now. 

### Known Issues
- Using the times psuedoinstruction with a multiline macro will only repeat the first instruction of the macro

### Planned Features
- [x] Macho File Support (Macos Support)
- [x] SSE/AVX2 instructions
- [ ] Preprocessor
- [x] Support for instruction prefixes
- [ ] Implementing Hashmaps for the Symbol Tables
- [ ] Allow multiple assembler passes
- [ ] Debug Symbols
- [ ] 16 & 32 bit Instructions
- [ ] AVX512 Instructions
- [ ] APX Instructions
 
