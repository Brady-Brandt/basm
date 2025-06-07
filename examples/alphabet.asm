; will only run on system v abi
; prints out upper and lowercase alphabet
section .text
global _start 

extern printf
extern exit

_start:  
    mov r15, 65
    
begin_upper:
    cmp r15, 91
    jge finish_upper 

    lea rdi, [fmt]
    mov esi, r15d
    call printf

    inc r15 
    jmp begin_upper 
finish_upper:    
    mov r13, 122
begin_lower:
    cmp r13, 97

    jl finish_lower

    lea rdi, [fmt]
    mov esi, r13d
    call printf
    dec r13

    jmp begin_lower

finish_lower: 
    xor rdi, rdi
    call exit

section .data
    fmt: db "%c\n"
