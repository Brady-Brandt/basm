section .text
global _start 



print:
    mov rax,1
    mov rdi, 1
    mov rdx,1
    syscall
    ret

exit:
    mov rax, 60 
    xor rdi, rdi 
    syscall 

_start:  
    mov r15, 65
    
begin_upper:
    cmp r15, 91
    jge finish_upper 

    mov [p], r15b
    lea rsi, [p]
    call print
    inc r15 
    jmp begin_upper 
finish_upper:

    ;print newline
    lea rsi, [nl]
    call print


    mov r13, 122
begin_lower:
    cmp r13, 97

    jl finish_lower

    mov [p], r13b
    lea rsi, [p]
    call print
    dec r13

    jmp begin_lower

finish_lower:
    ;print newline
    lea rsi, [nl]
    call print 
    call exit


section .data
    p: db 0
    ; don't support chars/escape chars yet
    nl: db 10

