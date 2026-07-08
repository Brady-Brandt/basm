#if __ELF__
global _start
extern puts
extern exit
_start:
    lea rdi, [hello]
    call puts

    lea rdi, [goodbye]
    call puts

    xor edi, edi
    call exit
#elif __MACHO__
global _main
extern _puts
extern _exit
_main:
    lea rdi, [hello]
    call _puts

    lea rdi, [goodbye]
    call _puts

    xor edi, edi
    call _exit
#elif __WIN__
; windows implementation not supported yet
global main
extern exit
main:
    call exit
#endif

section .data
    hello: db "你好"
    goodbye: db "Adios"
