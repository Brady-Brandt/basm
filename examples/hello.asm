section .data 
    hello: db "Hello, World!"
    goodbye: db "Adios"

section .text

global _start

_start:
    mov rax, 1 
    mov rdi, 1 
    lea esi, [hello]
    mov rdx, 13
    syscall


    mov rax, 1 
    mov rdi, 1 
    lea esi, [goodbye]
    mov rdx, 5
    syscall
    

    mov rax, 60
    xor rdi, rdi
    syscall 
