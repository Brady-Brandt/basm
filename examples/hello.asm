section .data 
    hello: db "Hello, World!\n" ; Strings are automatically null terminated
    goodbye: db "Adios\n"

section .text

global _start

extern printf 
extern exit

_start:
    ; uses linux calling convention
    lea rdi, [hello]
    call printf

    lea rdi, [goodbye]
    call printf 


    xor rdi,rdi
    call exit
    

    
