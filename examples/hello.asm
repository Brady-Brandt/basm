section .data 
    hello: db "Hello, World!" ; Strings are automatically null terminated
    goodbye: db "Adios"

section .text

global _start

extern puts
extern exit

_start:
    #ifdef __LINUX__
        lea rdi, [hello]
        call puts
        lea rdi, [goodbye]
        call puts

        xor rdi, rdi
        call exit
    #endif

    call exit
