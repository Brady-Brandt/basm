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
section .data
    hello: db "Hello, World!" ; Double quoted strings are null terminated

section .text
#if __ELF__

global _start
extern puts
extern exit
_start:
    lea rdi, [rel hello]
    call puts

    xor edi, edi
    call exit

#elif __MACHO__

global _main
extern _puts
_main:
    sub rsp, 8
    lea rdi, [rel hello]
    call _puts
    add rsp, 8

    xor eax, eax
    ret

#elif __WIN__

global main
extern puts
main:
    ; align to 16 bytes + 32 bytes of shadow space
    sub rsp, 40
    lea rcx, [rel hello]
    call puts
    add rsp, 40

    xor eax, eax
    ret

#endif
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
 
