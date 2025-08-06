; prints out some cpu flags
#ifdef __ELF__
    #define first_arg rdi
    #define second_arg rsi
    #define third_arg rdx
#endif


#macro call_check_flag(reg,value,str_lbl)
    mov first_arg, reg

    mov second_arg, 1
    shl second_arg, value 

    lea third_arg, [str_lbl]
    call check_flag
#endmacro

section .text

global _start

extern exit
extern printf
extern puts 


check_flag:
    and first_arg, second_arg
    jz lbl
    mov first_arg, third_arg
    call puts
    lbl:
    ret




_start:
    ; get the processor string
    mov eax, 0
    cpuid
    
    ; print the processor string
    lea first_arg, [pstr]
    mov [first_arg], ebx
    mov [first_arg + 4], edx
    mov [first_arg + 8], ecx
    call puts

    ; feature flags 
    mov eax, 1
    cpuid 

    mov r15d, edx
    mov r14d, ecx

    call_check_flag(r15,0,fpu_str) 
    call_check_flag(r15,1,vme_str) 
    call_check_flag(r15,2, de_str) 
    call_check_flag(r15,3, pse_str) 
    call_check_flag(r15,4, tsc_str) 
    call_check_flag(r15,5, msr_str) 
    call_check_flag(r15,6, pae_str) 
    call_check_flag(r15,7, mce_str) 
    call_check_flag(r15,8, cx8_str) 
    call_check_flag(r15,9, apic_str) 
    call_check_flag(r15,11, sep_str) 
    call_check_flag(r15,12, mtrr_str) 
    call_check_flag(r15,13, pge_str) 
    call_check_flag(r15,14, mca_str) 
    call_check_flag(r15,15, cmov_str) 
    call_check_flag(r15,16, pat_str) 
    call_check_flag(r15,17, pse36_str) 
    call_check_flag(r15,18, psn_str) 
    call_check_flag(r15,19, clfsh_str) 
    call_check_flag(r15,21, debug_st_str) 
    call_check_flag(r15,22, acpi_str) 
    call_check_flag(r15,23, mmx_str) 
    call_check_flag(r15,24, fxsr_str) 
    call_check_flag(r15,25, sse_str) 
    call_check_flag(r15,26, sse2_str) 
    call_check_flag(r15,27, self_snoop_str) 
    call_check_flag(r15,28,htt_str ) 
    call_check_flag(r15,29, tm_str) 
    call_check_flag(r15,30, ia64_str) 
    call_check_flag(r15,31, pbe_str) 

    ; print line new line to seperate
    mov byte [pstr], 0
    lea first_arg, [pstr]
    call puts
    
    call_check_flag(r14,0,sse3_str) 
    call_check_flag(r14,1,pclmulqdq_str) 
    call_check_flag(r14,2, dtes64_str) 
    call_check_flag(r14,3, monitor_str) 
    call_check_flag(r14,4, dscpl_str) 
    call_check_flag(r14,5, vmx_str) 
    call_check_flag(r14,6, smx_str) 
    call_check_flag(r14,7, est_str) 
    call_check_flag(r14,8, tm2_str) 
    call_check_flag(r14,9, ssse3_str) 
    call_check_flag(r14,10, cnxtid_str) 
    call_check_flag(r14,11, sbdg_str) 
    call_check_flag(r14,12, fma_str) 
    call_check_flag(r14,13, cx16_str) 
    call_check_flag(r14,14, xptr_str) 
    call_check_flag(r14,15, pdcm_str) 
    call_check_flag(r14,17, pcid_str) 
    call_check_flag(r14,18, dca_str) 
    call_check_flag(r14,19, sse41_str) 
    call_check_flag(r14,20, sse42_str) 
    call_check_flag(r14,21, x2apic_str) 
    call_check_flag(r14,22, movbe_str) 
    call_check_flag(r14,23, popcnt_str) 
    call_check_flag(r14,24, tscdead_str) 
    call_check_flag(r14,25, aes_str) 
    call_check_flag(r14,26, xsave_str) 
    call_check_flag(r14,27, osxsave_str) 
    call_check_flag(r14,28,avx_str ) 
    call_check_flag(r14,29, f16c_str) 
    call_check_flag(r14,30, rdrnd_str) 
    call_check_flag(r14,31, hypervisor_str) 


    ; print line new line to seperate
    lea first_arg, [pstr]
    call puts


    
    mov eax, 0x7
    mov ecx, 0
    cpuid

    mov r15d, ebx
    mov r14d, ecx
    mov r13d, edx

    call_check_flag(r15,0,fsgsbase_str) 
    call_check_flag(r15,1,adjust_msr_str) 
    call_check_flag(r15,2,sgx_str) 
    call_check_flag(r15,3,bmi1_str) 
    call_check_flag(r15,4,hle_str) 
    call_check_flag(r15,5,avx2_str) 
    call_check_flag(r15,6,fdpexcpt_str) 
    call_check_flag(r15,7,smep_str) 
    call_check_flag(r15,8,bmi2_str) 
    call_check_flag(r15,9,erms_str) 
    call_check_flag(r15,10,invpcid_str) 
    call_check_flag(r15,11,rtm_str) 
    call_check_flag(r15,12,rdt_str) 
    call_check_flag(r15,13,dep_fpu_cs_str) 
    call_check_flag(r15,14,mpx_str) 
    call_check_flag(r15,15,rdt_apq_str) 
    call_check_flag(r15,16,avx512f_str) 
    call_check_flag(r15,17,avx512dq_str) 
    call_check_flag(r15,18,rdseed_str) 
    call_check_flag(r15,19,adx_str) 
    call_check_flag(r15,20,smap_str) 
    call_check_flag(r15,21,avx512ifma_str) 
    call_check_flag(r15,22,pcommit_str) 
    call_check_flag(r15,23,clfshopt_str) 
    call_check_flag(r15,24,clwb_str) 
    call_check_flag(r15,25,pt_str) 
    call_check_flag(r15,26,avx512pf_str) 
    call_check_flag(r15,27,avx512er_str) 
    call_check_flag(r15,28,avx512cd_str) 
    call_check_flag(r15,29,sha_str) 
    call_check_flag(r15,30,avx512bw_str) 
    call_check_flag(r15,31,avx512vl_str) 

    xor first_arg, first_arg
    call exit
    


section .bss
    pstr: resb 13


section .data
    fmt: db "%032B\n"
    fpu_str: db "x87 FPU"
    vme_str: db "Virtual 8086 extensions"
    de_str: db "Debuggin Extensions" 
    pse_str: db "Page Size Extension"
    tsc_str: db "Time Stamp Counter"
    msr_str: db "Model Specific Registers"
    pae_str: db "Physical Address Extension"
    mce_str: db "Machine Check Exception"
    cx8_str: db "CMPXCHG8B instruction"
    apic_str: db "Onboard APIC"
    sep_str: db "Sysenter & Sysexit"
    mtrr_str: db "Memory Type Range Registers"
    pge_str: db "Page Gobal Enable"
    mca_str: db "Machine Check architecture"
    cmov_str: db "Conditional Move"
    pat_str: db "Page Attribute Table"
    psn_str: db "Processor Serial Number Enabled"
    pse36_str: db "36 bit page size Extension"
    clfsh_str: db "CLFLUSH instruction"
    debug_st_str: db "Debug Store"
    acpi_str: db "Onboard thermal control MSRs for ACPI"
    mmx_str: db "MMX instructions"
    fxsr_str: db "FXSAVE & FXSTOR instructions"
    sse_str: db "SSE Instructions"
    sse2_str: db "SSE2 Instructions"
    self_snoop_str: db "Cache Implements Self snoop"
    htt_str: db "Max APIC IDs reserved field is Valid"
    tm_str: db "Thermal Monitor limits temperature"
    ia64_str: db "IA64 processor emulating"
    pbe_str: db "Pending Break Enable"

    sse3_str: db "SSE3 Instructions"
    pclmulqdq_str: db "PCLMULQDQ Instruction"
    dtes64_str: db "64 bit debug store"
    monitor_str: db "MONITOR & MWAIT"
    dscpl_str: db "CPL qualified debug store"
    vmx_str: db "Virtual Machine Extensions"
    smx_str: db "Safer Mode Extensions"
    est_str: db "Enhanced SpeedStep"
    tm2_str: db "Thermal Monitor 2"
    ssse3_str: db "SSE3 Supplemental Instructions"
    cnxtid_str: db "L1 Context Id"
    sbdg_str: db "Silicon Debug Interface"
    fma_str: db "Fused multiply add"
    cx16_str: db "CMPXCHG16B Instruction"
    xptr_str: db "Disable sending task priority messages"
    pdcm_str: db "Permon & debug"
    pcid_str: db "Process Context Ids"
    dca_str: db "Direct cache access"
    sse41_str: db "SSE4.1 Instructions"
    sse42_str: db "SSE4.2 Instructions"
    x2apic_str: db "Enhanced APIC"
    movbe_str: db "MOVEBE Instruction"
    popcnt_str: db "POPCNT Instruction"
    tscdead_str: db "APIC deadline"
    aes_str: db "AES Instructions"
    xsave_str: db "XSAVE Instructions"
    osxsave_str: db "XSAVE enabled by OS"
    avx_str: db "AVX Instructions"
    f16c_str: db "FP16 Instructions"
    rdrnd_str: db "RDRAND Instruction"
    hypervisor_str: db "Hypervisor support"


    fsgsbase_str: db "Access to base of %fs and %gs" 
    adjust_msr_str: db "IA32_TSC_ADJUST_MSR"
    sgx_str: db "Software Guard Extensions"
    bmi1_str: db "Bit Manipulation Instructions 1"
    hle_str: db "TSX Hardware Lock Elision"
    avx2_str: db "AVX2 Instructions"
    fdpexcpt_str: db "x87 FPU DP updated on Exceptions"
    smep_str: db "Supervisor Mode Execution Prevention"
    bmi2_str: db "Bit Manipulation Instructions 2"
    erms_str: db "Enhanced REP MOVSB/STOSB"
    invpcid_str: db "INVPCID Instruction"
    rtm_str: db "TSX Restricted Transactional Memory"
    rdt_str: db "Resource Monitoring"
    dep_fpu_cs_str: db "FPU CS and DS Deprecated"
    mpx_str: db "Intel MPX"
    rdt_apq_str: db "RDT OR QOS"
    avx512f_str: db "AVX512 Foundation"
    avx512dq_str: db "AVX512DQ"
    rdseed_str: db "RDSEED Instruction"
    adx_str: db "Intel ADX Instructions"
    smap_str: db "Supervisor Mode Access Prevention"
    avx512ifma_str: db "AVX512IFMA"
    pcommit_str: db "PCOMMIT Instruction"
    clfshopt_str: db "CLFLUSHOPT Instruction"
    clwb_str: db "CLWB Instruction"
    pt_str: db "Intel Processor Trace"
    avx512pf_str: db "AVX512PF"
    avx512er_str: db "AVX512ER"
    avx512cd_str: db "AVX512CD"
    sha_str: db "SHA Extensions"
    avx512bw_str: db "AVX512BW"
    avx512vl_str: db "AVX512VL"
