; computes the the sqrt of every number from 0 - INT32_MAX
; optionally prints them out

#macro print(offset)
    lea rdi, [fmt]
    mov al, 1
    movsd xmm0, [data + offset]
    call printf
#endmacro

;max value is int32 max
#define MAX_SQRT 2147483647

global _start 

extern printf
extern exit

_start:
xor r15d, r15d

vmovdqu xmm14, [st_nums]
vmovdqu xmm15, [step]
begin: 
    VCVTDQ2PD ymm1, xmm14
    VSQRTPD ymm0, ymm1

    VPADDD xmm14, xmm14, xmm15
    add r15d, 4


    ; uncomment if you want to print output 
    ;vmovupd yword [data], ymm0
    ;print(0)
    ;print(8)
    ;print(16)
    ;print(24)

    cmp r15, MAX_SQRT
    jl begin
 
    xor rdi, rdi
    call exit


section .data
    fmt: db "%lf\n"
    step: dd 4,4,4,4
    st_nums: dd 0,1,2,3

section .bss
    data: resq 4
