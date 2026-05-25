; Recreating the sleep command for linux systems
; Accepts an integer and sleeps for that amount of seconds
global _start

#macro exit(exit_code)
    mov eax, 60
    mov edi, exit_code
    syscall
#endmacro


#macro write(fd, ptr, len)
    mov eax, 1
    mov edi, fd
    lea rsi, [ptr]
    mov edx, len
    syscall
#endmacro


#define STDOUT 1
#define STDERR 2
    

atol:
    xor eax, eax ; result
    mov esi, 0 ;index into string
    

    start_loop:
        mov bl, byte [rdi + rsi]

        ; ensure its ascii
        cmp bl, '0'
        jl end
        cmp bl, '9'
        jg end

        imul rax, rax, 10

        ; convert it into a number and add it to the result
        sub bl, 48
        add rax, rbx
        inc esi
        inc byte [length]
        jmp start_loop 

    end: 
    ret


_start:
    mov ebp, 0
    mov rax, [rsp] ; rsp -> argc, rsp + 8 -> argv

    cmp rax, 2
    jge parse_args

    ; write the usage to stderr and exit 
    write(STDERR, usage, 33)
    exit(1)

parse_args: 
    mov rdi, [rsp + 16] ; argv[1]
    call atol
    cmp byte [length], 0
    je invalid_time

    mov qword [time_struct], rax ; sleep time in seconds


    ; write the amount of time we are sleeping for
    write(STDOUT, sleep_time, 13)
    mov qword [temp], [rsp + 16]
    write(1, temp, [length])
    write(STDOUT, seconds, 9)


    mov eax, 35 ; sleep syscall
    lea rdi, [time_struct], ;timespec struct
    mov rsi, 0, ;
    syscall
    
    exit(0)

    invalid_time:
        write(STDERR, invalid, 13)
        write(STDERR, usage, 33)
        exit(1)

section .bss
    time_struct: resq 2
    length: resb 1
    temp: resq 1

section .data
    usage: db "Usage: ./sleep (time in seconds)\n"
    sleep_time: db 'Sleeping for '
    seconds: db ' Seconds\n'
    invalid: db 'Invalid Time\n'
