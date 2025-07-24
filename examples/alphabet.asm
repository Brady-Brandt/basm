; will only run on system v abi
; prints out upper and lowercase alphabet
section .text
global _start 

extern printf
extern exit

_start:  
    mov r15, 'A'
    
begin_upper:
    cmp r15, 'Z'
    jg finish_upper 

    lea rdi, [fmt]
    mov esi, r15d
    call printf

    inc r15 
    jmp begin_upper 
finish_upper:    
    lea rdi, [fmt]
    mov esi, '\n'
    call printf
    mov r13, 'z'  
begin_lower:
    cmp r13, 'a'

    jl finish_lower

    lea rdi, [fmt]
    mov esi, r13d
    call printf
    dec r13

    jmp begin_lower

finish_lower: 
    lea rdi, [fmt]
    mov esi, '\n'
    call printf
    xor rdi, rdi
    call exit

section .data
    fmt: db "%c"
