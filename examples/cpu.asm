; prints out some cpu flags


#macro call_check_flag(reg,value,str_lbl)
    bt reg, value
    lea arg0, [rel str_lbl]
    call check_flag
#endmacro
 

#if __ELF__
    global _start
    extern exit
    extern puts
    #define print puts
    #define arg0 rdi

    #macro exit_success()
        xor edi, edi
        call exit
    #endmacro
_start:
#elif __MACHO__
    global _main
    extern _exit
    extern _puts
    #define print _puts
    #define arg0 rdi
    #macro exit_success()
        xor edi, edi
        call _exit
    #endmacro
_main:
#endif

    ; get the processor string
    mov eax, 0
    cpuid
    
    ; print the processor string
    lea arg0, [rel pstr]
    mov [arg0], ebx
    mov [arg0 + 4], edx
    mov [arg0 + 8], ecx
    call print

    ; feature flags 
    mov eax, 1
    cpuid 

    mov r15d, edx
    mov r14d, ecx

    call_check_flag(r15d,0,fpu_str)
    call_check_flag(r15d,1,vme_str)
    call_check_flag(r15d,2, de_str)
    call_check_flag(r15d,3, pse_str)
    call_check_flag(r15d,4, tsc_str)
    call_check_flag(r15d,5, msr_str)
    call_check_flag(r15d,6, pae_str)
    call_check_flag(r15d,7, mce_str)
    call_check_flag(r15d,8, cx8_str)
    call_check_flag(r15d,9, apic_str)
    call_check_flag(r15d,11, sep_str)
    call_check_flag(r15d,12, mtrr_str)
    call_check_flag(r15d,13, pge_str)
    call_check_flag(r15d,14, mca_str)
    call_check_flag(r15d,15, cmov_str)
    call_check_flag(r15d,16, pat_str)
    call_check_flag(r15d,17, pse36_str)
    call_check_flag(r15d,18, psn_str)
    call_check_flag(r15d,19, clfsh_str)
    call_check_flag(r15d,21, debug_st_str)
    call_check_flag(r15d,22, acpi_str)
    call_check_flag(r15d,23, mmx_str)
    call_check_flag(r15d,24, fxsr_str)
    call_check_flag(r15d,25, sse_str)
    call_check_flag(r15d,26, sse2_str)
    call_check_flag(r15d,27, self_snoop_str)
    call_check_flag(r15d,28,htt_str )
    call_check_flag(r15d,29, tm_str)
    call_check_flag(r15d,30, ia64_str)
    call_check_flag(r15d,31, pbe_str)

    ; print line new line to seperate
    mov byte [rel pstr], 0
    lea arg0, [rel pstr]
    call print
    
    call_check_flag(r14d,0,sse3_str)
    call_check_flag(r14d,1,pclmulqdq_str)
    call_check_flag(r14d,2, dtes64_str)
    call_check_flag(r14d,3, monitor_str)
    call_check_flag(r14d,4, dscpl_str)
    call_check_flag(r14d,5, vmx_str)
    call_check_flag(r14d,6, smx_str)
    call_check_flag(r14d,7, est_str)
    call_check_flag(r14d,8, tm2_str)
    call_check_flag(r14d,9, ssse3_str)
    call_check_flag(r14d,10, cnxtid_str)
    call_check_flag(r14d,11, sbdg_str)
    call_check_flag(r14d,12, fma_str)
    call_check_flag(r14d,13, cx16_str)
    call_check_flag(r14d,14, xptr_str)
    call_check_flag(r14d,15, pdcm_str)
    call_check_flag(r14d,17, pcid_str)
    call_check_flag(r14d,18, dca_str)
    call_check_flag(r14d,19, sse41_str)
    call_check_flag(r14d,20, sse42_str)
    call_check_flag(r14d,21, x2apic_str)
    call_check_flag(r14d,22, movbe_str)
    call_check_flag(r14d,23, popcnt_str)
    call_check_flag(r14d,24, tscdead_str)
    call_check_flag(r14d,25, aes_str)
    call_check_flag(r14d,26, xsave_str)
    call_check_flag(r14d,27, osxsave_str)
    call_check_flag(r14d,28,avx_str )
    call_check_flag(r14d,29, f16c_str)
    call_check_flag(r14d,30, rdrnd_str)
    call_check_flag(r14d,31, hypervisor_str)


    ; print line new line to seperate
    lea arg0, [rel pstr]
    call print

    mov eax, 0x7
    mov ecx, 0
    cpuid

    mov r15d, ebx
    mov r14d, ecx
    mov r13d, edx

    call_check_flag(r15d,0,fsgsbase_str)
    call_check_flag(r15d,1,adjust_msr_str)
    call_check_flag(r15d,2,sgx_str)
    call_check_flag(r15d,3,bmi1_str)
    call_check_flag(r15d,4,hle_str)
    call_check_flag(r15d,5,avx2_str)
    call_check_flag(r15d,6,fdpexcpt_str)
    call_check_flag(r15d,7,smep_str)
    call_check_flag(r15d,8,bmi2_str)
    call_check_flag(r15d,9,erms_str)
    call_check_flag(r15d,10,invpcid_str)
    call_check_flag(r15d,11,rtm_str)
    call_check_flag(r15d,12,rdt_str)
    call_check_flag(r15d,13,dep_fpu_cs_str)
    call_check_flag(r15d,14,mpx_str)
    call_check_flag(r15d,15,rdt_apq_str)
    call_check_flag(r15d,16,avx512f_str)
    call_check_flag(r15d,17,avx512dq_str)
    call_check_flag(r15d,18,rdseed_str)
    call_check_flag(r15d,19,adx_str)
    call_check_flag(r15d,20,smap_str)
    call_check_flag(r15d,21,avx512ifma_str)
    call_check_flag(r15d,22,pcommit_str)
    call_check_flag(r15d,23,clfshopt_str)
    call_check_flag(r15d,24,clwb_str)
    call_check_flag(r15d,25,pt_str)
    call_check_flag(r15d,26,avx512pf_str)
    call_check_flag(r15d,27,avx512er_str)
    call_check_flag(r15d,28,avx512cd_str)
    call_check_flag(r15d,29,sha_str)
    call_check_flag(r15d,30,avx512bw_str)
    call_check_flag(r15d,31,avx512vl_str)

    exit_success()

; since macro local labels are not yet supported
; need to call a seperate function
check_flag:
    jnc lbl
    call print
    lbl:
    ret

section .bss
    pstr: resb 13

section .data
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
