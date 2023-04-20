; -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
;   Copyright 2023, Jonathan Hart
;   This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
;   either version 3 of the License, or (at your option) any later version.
;   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;   PURPOSE. See the GNU General Public License for more details.
;   You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
; -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------



    ; map vm registers onto real registers
    ;   Initially, this was intended to be reflective of real registers.
    ;   It was later determined that keeping repeatedly accessed items off volatile registers was preferable
    ;       (mainly, avoid rax and rdx, since x86 multiplication and division annoyingly use them as implicit registers).
    ;   Additionally, had to reserve a temp register to ensure unwanted outputs can be handled without spilling
    ;       (the temp register subsequently was found to be useful for conditional jumps)
    ;

    ;   It was later still determined that one of the registers restricted to 16 bits should use the stack pointer register (rsp),
    ;   to help further undermine return-oriented programming.  Additionally, I would like to have memory covering said addresses (0-65536+8)
    ;   guaranteed to be restricted (segfault on any attempt to access) - that way the VM can have as many unintended return instructions
    ;   as the Assembler wants to put in it, without issue...  (with ASLR, it at least *probably* won't map anything reliably exploitable into
    ;   that range)

    ;   TODO:
    ;       The "offical" runtime should host the VM in a separate process, establish an IPC interface, and do so with a minimal codebase.
    ;
    ;       This is largely to validate instructions (or to take care of the mapping of instruction codes to actual branch targets),
    ;       as otherwise an attacker with a partially compromised runtime can feasibly leverage the VM to gain control over the process
    ;       (by forcing it to jump to random positions in machine code -- HOORAY FOR x86 densely encoded variable-length instructions!).
    ;
    ;       That said, it does seem to be a fairly unlikely attack, as the compromise would to be to something that can derefernce the context
    ;       data structure, then index and dereference the program pointer, then alter items in that block of memory in a controlled manner.
    ;       If the attacker knows memory layout, then it would suffice simply to write.  If the attacker has not broken ASLR, then compromise
    ;       also needs the ability to read said memory or to apply transforms to memory (increment, decrement, xor).
    ;       --- If an attacker is able to relaibly traverse and manipulate internal data structures, then the attacker has probably already won
    ;       and does not have much use for the VM.

    ; I got tired of naming inconsistancies dealing with implicit casting, so these definitions are to switch to a naming inconsistancy
    ;   which I don't really care all that much about...
    %define rax_8 al
    %define rax_16 ax
    %define rax_32 eax
    %define rax_64 rax
    
    %define rbx_8 bl
    %define rbx_16 bx
    %define rbx_32 ebx
    %define rbx_64 rbx
    
    %define rcx_8 cl
    %define rcx_16 cx
    %define rcx_32 ecx
    %define rcx_64 rcx
    
    %define rdx_8 dl
    %define rdx_16 dx
    %define rdx_32 edx
    %define rdx_64 rdx
    
    %define rsp_8 spl
    %define rsp_16 sp
    %define rsp_32 esp
    %define rsp_64 rsp
    
    %define rbp_8 sbl
    %define rbp_16 bp
    %define rbp_32 ebp
    %define rbp_64 rbp
    
    %define rsi_8 sil
    %define rsi_16 si
    %define rsi_32 esi
    %define rsi_64 rsi
    
    %define rdi_8 dil
    %define rdi_16 di
    %define rdi_32 edi
    %define rdi_64 rdi
    
    %define r8_8 r8b
    %define r8_16 r8w
    %define r8_32 r8d
    %define r8_64 r8
    
    %define r9_8 r9b
    %define r9_16 r9w
    %define r9_32 r9d
    %define r9_64 r9
    
    %define r10_8 r10b
    %define r10_16 r10w
    %define r10_32 r10d
    %define r10_64 r10
    
    %define r11_8 r11b
    %define r11_16 r11w
    %define r11_32 r11d
    %define r11_64 r11
    
    %define r12_8 r12b
    %define r12_16 r12w
    %define r12_32 r12d
    %define r12_64 r12
    
    %define r13_8 r13b
    %define r13_16 r13w
    %define r13_32 r13d
    %define r13_64 r13
    
    %define r14_8 r14b
    %define r14_16 r14w
    %define r14_32 r14d
    %define r14_64 r14
    
    %define r15_8 r15b
    %define r15_16 r15w
    %define r15_32 r15d
    %define r15_64 r15
    
    %define rMAPPER rdi
    %define rMCSI   rsp
    %define rLWCSI  rbp
    %define rTEMPA  rax
    %define rTEMPC  rcx
    %define rTEMPD  rdx
    %define rADDR1  r10
    %define rADDR2  r11
    %define rADDR3  rsi
    %define rADDR4  rbx
    %define rA      r12
    %define rB      r13
    %define rC      r14
    %define rD      r15
    %define rE      r8
    %define rF      r9
    
    %define xTEMPA xmm14
    %define xTEMPB xmm15
    
    %define rMAPPER_8 %[rMAPPER]_8
    %define rMAPPER_16 %[rMAPPER]_16
    %define rMAPPER_32 %[rMAPPER]_32
    %define rMAPPER_64 %[rMAPPER]_64
    
    %define rLWCSI_8 %[rLWCSI]_8
    %define rLWCSI_16 %[rLWCSI]_16
    %define rLWCSI_32 %[rLWCSI]_32
    %define rLWCSI_64 %[rLWCSI]_64
    
    %define rTEMPA_8 %[rTEMPA]_8
    %define rTEMPA_16 %[rTEMPA]_16
    %define rTEMPA_32 %[rTEMPA]_32
    %define rTEMPA_64 %[rTEMPA]_64

    %define rTEMPC_8 %[rTEMPC]_8
    %define rTEMPC_16 %[rTEMPC]_16
    %define rTEMPC_32 %[rTEMPC]_32
    %define rTEMPC_64 %[rTEMPC]_64
    
    %define rTEMPD_8 %[rTEMPD]_8
    %define rTEMPD_16 %[rTEMPD]_16
    %define rTEMPD_32 %[rTEMPD]_32
    %define rTEMPD_64 %[rTEMPD]_64
    
    %define rADDR1_8 %[rADDR1]_8
    %define rADDR1_16 %[rADDR1]_16
    %define rADDR1_32 %[rADDR1]_32
    %define rADDR1_64 %[rADDR1]_64
    
    %define rADDR2_8 %[rADDR2]_8
    %define rADDR2_16 %[rADDR2]_16
    %define rADDR2_32 %[rADDR2]_32
    %define rADDR2_64 %[rADDR2]_64
    
    %define rADDR3_8 %[rADDR3]_8
    %define rADDR3_16 %[rADDR3]_16
    %define rADDR3_32 %[rADDR3]_32
    %define rADDR3_64 %[rADDR3]_64
    
    %define rADDR4_8 %[rADDR4]_8
    %define rADDR4_16 %[rADDR4]_16
    %define rADDR4_32 %[rADDR4]_32
    %define rADDR4_64 %[rADDR4]_64
    
    %define rMCSI_8 %[rMCSI]_8
    %define rMCSI_16 %[rMCSI]_16
    %define rMCSI_32 %[rMCSI]_32
    %define rMCSI_64 %[rMCSI]_64
    
    %define rA_8 %[rA]_8
    %define rA_16 %[rA]_16
    %define rA_32 %[rA]_32
    %define rA_64 %[rA]_64
    
    %define rB_8 %[rB]_8
    %define rB_16 %[rB]_16
    %define rB_32 %[rB]_32
    %define rB_64 %[rB]_64
    
    %define rC_8 %[rC]_8
    %define rC_16 %[rC]_16
    %define rC_32 %[rC]_32
    %define rC_64 %[rC]_64
    
    %define rD_8 %[rD]_8
    %define rD_16 %[rD]_16
    %define rD_32 %[rD]_32
    %define rD_64 %[rD]_64
    
    %define rE_8 %[rE]_8
    %define rE_16 %[rE]_16
    %define rE_32 %[rE]_32
    %define rE_64 %[rE]_64
    
    %define rF_8 %[rF]_8
    %define rF_16 %[rF]_16
    %define rF_32 %[rF]_32
    %define rF_64 %[rF]_64
    
    %macro dwords 1-*
        %rep %0
            .%[%1]: resd 1
            %rotate 1
        %endrep %1
    %endmacro
    
    %macro qwords 1-*
        %rep %0
            .%[%1]: resq 1
            %rotate 1
        %endrep %1
    %endmacro
    
    %macro owords 1-*
        %rep %0
            .%[%1]: reso 1
            %rotate 1
        %endrep %1
    %endmacro
    
    struc regbank
        qwords prog,lwcsi,mcsi,tempa,tempc,tempd,addr1,addr2,addr3,addr4,a,b,c,d,e,f
        owords xa,xb,xc,xd, xe,xf,xg,xh, xi,xj,xk,xl, xm,xn,xo,xp
    endstruc

    struc tvmstackentry
      qwords source_program, source_return_index, source_stack_floor, target_program, target_function_jumptable, target_jump_index
    endstruc

    %define stacksize 1024
    struc tactyk_stack
        .stack_position:        resq 1
        .stack_lock:            resq 1
        .stack:                 resb stacksize * tvmstackentry_size
    endstruc
    
    struc program_dec
      qwords instruction_count, instruction_jumptable, function_count, function_jumptable
    endstruc
    
    struc tactyk_vm
      qwords program_count, program_list
    endstruc
    
    struc microcontext
        qwords a1,b1,c1,d1,e1,f1
        qwords a2,b2,c2,d2,e2,f2
        qwords a3,b3,c3,d3,e3,f3
        qwords lwcsi, mcsi
        qwords addr1
        dwords addr1_element_bound, addr1_array_bound, addr1_memblock_index, addr1_type
        qwords addr2
        dwords addr2_element_bound, addr2_array_bound, addr2_memblock_index, addr2_type
        qwords addr3
        dwords addr3_element_bound, addr3_array_bound, addr3_memblock_index, addr3_type
        qwords addr4
        dwords addr4_element_bound, addr4_array_bound, addr4_memblock_index, addr4_type
        .xa: qwords x1a, x2a
        .xb: qwords x1b, x2b
        .xc: qwords x1c, x2c
        .xd: qwords x1d, x2d
        .xe: qwords x1e, x2e
        .xf: qwords x1f, x2f
        .xg: qwords x1g, x2g
        .xh: qwords x1h, x2h
        .xi: qwords x1i, x2i
        .xj: qwords x1j, x2j
        .xk: qwords x1k, x2k
        .xl: qwords x1l, x2l
        .xm: qwords x1m, x2m
        .xn: qwords x1n, x2n
        .xo: qwords x1o, x2o
        .xp: qwords x1p, x2p
    endstruc
    
    %define microcontext_size_bits 9
    %define microcontext_stack_size 65536
    %define lwcall_stack_size 65536
    %define lwcall_stack_max 65535
    
    struc context
        
        ; 0
        
        qwords maxip, subcontext
        qwords memblocks, memblocks_count
        
        ; 32
        
        qwords addr1
        dwords addr1_element_bound, addr1_array_bound, addr1_memblock_index, addr1_type
        qwords addr2
        dwords addr2_element_bound, addr2_array_bound, addr2_memblock_index, addr2_type
        qwords addr3
        dwords addr3_element_bound, addr3_array_bound, addr3_memblock_index, addr3_type
        qwords addr4
        dwords addr4_element_bound, addr4_array_bound, addr4_memblock_index, addr4_type

        ; 128

        qwords lwcall_stack_address, microcontext_stack_address, microcontext_stack_offset
        .floor: dwords lwcall_floor, microcontext_floor
        
        qwords vm, stack, program_map, hl_program_ref, instruction_index, status, signature, extra
        
        .registers:  resq 48
        .runtime_registers:  resq 48
        
        .diagnostic_out:resq 1024
    endstruc
    
    %unmacro dwords 1-*
    %unmacro qwords 1-*
    %unmacro twords 1-*
    
    %define STATUS_OFF 0
    %define STATUS_RUN 1
    %define STATUS_SLEEP 2
    %define STATUS_SUSPEND 3
    %define STATUS_HALT 4
    
    %define STATUS_BREAK 16
    
    %define STATUS_ROGUE_POINTER 101
    %define STATUS_ROGUE_BRANCH 102
    %define STATUS_STACK_OVERFLOW 103
    %define STATUS_STACK_UNDERFLOW 104
    %define STATUS_MEMORY_OVERFLOW 105      ; a generic overflow to be used in place of "stack" overflow when there is a conceptual mismatch
    %define STATUS_MEMORY_UNDERFLOW 106     ; a generic underflow to be used in place of "stack" underflow when there is a conceptual mismatch
    %define STATUS_UNSIGNED_CONTEXT 107
    %define STATUS_INVALID_TVMJUMP 108
    %define STATUS_INVALID_TVMJUMP_STATE 109
    %define STATUS_DIVISION_BY_ZERO 110
    
    ; runtime registers do not belong to tactyk, and so do not use internal tactyk names
    ; The only thing that matters here is that they get correctly stored and restored.
    %macro store_runtimecontext 1
        mov [%1 + context.runtime_registers + 0], rbx
        mov [%1 + context.runtime_registers + 8], rbp
        mov [%1 + context.runtime_registers + 16], rsp
        mov [%1 + context.runtime_registers + 24], r12
        mov [%1 + context.runtime_registers + 32], r13
        mov [%1 + context.runtime_registers + 40], r14
        mov [%1 + context.runtime_registers + 48], r15
        rdfsbase r12
        rdgsbase r13
        mov [%1 + context.runtime_registers + 56], r12
        mov [%1 + context.runtime_registers + 64], r13
        
        ; supposedly not needed:
        ;movdqu [%1 + context.runtime_registers + 128+0   ], xmm0
        ;movdqu [%1 + context.runtime_registers + 128+8   ], xmm1
        ;movdqu [%1 + context.runtime_registers + 128+16  ], xmm2
        ;movdqu [%1 + context.runtime_registers + 128+32  ], xmm3
        ;movdqu [%1 + context.runtime_registers + 128+40  ], xmm4
        ;movdqu [%1 + context.runtime_registers + 128+48  ], xmm5
        ;movdqu [%1 + context.runtime_registers + 128+56  ], xmm6
        ;movdqu [%1 + context.runtime_registers + 128+64  ], xmm7
        ;movdqu [%1 + context.runtime_registers + 128+72  ], xmm8
        ;movdqu [%1 + context.runtime_registers + 128+80  ], xmm9
        ;movdqu [%1 + context.runtime_registers + 128+88  ], xmm10
        ;movdqu [%1 + context.runtime_registers + 128+96  ], xmm11
        ;movdqu [%1 + context.runtime_registers + 128+104 ], xmm12
        ;movdqu [%1 + context.runtime_registers + 128+112 ], xmm13
        ;movdqu [%1 + context.runtime_registers + 128+120 ], xmm14
        ;movdqu [%1 + context.runtime_registers + 128+128 ], xmm15
    %endmacro

    %macro load_runtimecontext 1
        mov r12, [%1 + context.runtime_registers + 56]
        mov r13, [%1 + context.runtime_registers + 64]
        wrfsbase r12
        wrgsbase r13
        mov rbx, [%1 + context.runtime_registers + 0]
        mov rbp, [%1 + context.runtime_registers + 8]
        mov rsp, [%1 + context.runtime_registers + 16]
        mov r12, [%1 + context.runtime_registers + 24]
        mov r13, [%1 + context.runtime_registers + 32]

        ; supposedly not needed:
        ;movdqu xmm0,  [%1 + context.runtime_registers + 128+0   ]
        ;movdqu xmm1,  [%1 + context.runtime_registers + 128+8   ]
        ;movdqu xmm2,  [%1 + context.runtime_registers + 128+16  ]
        ;movdqu xmm3,  [%1 + context.runtime_registers + 128+32  ]
        ;movdqu xmm4,  [%1 + context.runtime_registers + 128+40  ]
        ;movdqu xmm5,  [%1 + context.runtime_registers + 128+48  ]
        ;movdqu xmm6,  [%1 + context.runtime_registers + 128+56  ]
        ;movdqu xmm7,  [%1 + context.runtime_registers + 128+64  ]
        ;movdqu xmm8,  [%1 + context.runtime_registers + 128+72  ]
        ;movdqu xmm9,  [%1 + context.runtime_registers + 128+80  ]
        ;movdqu xmm10, [%1 + context.runtime_registers + 128+88  ]
        ;movdqu xmm11, [%1 + context.runtime_registers + 128+96  ]
        ;movdqu xmm12, [%1 + context.runtime_registers + 128+104 ]
        ;movdqu xmm13, [%1 + context.runtime_registers + 128+112 ]
        ;movdqu xmm14, [%1 + context.runtime_registers + 128+120 ]
        ;movdqu xmm15, [%1 + context.runtime_registers + 128+128 ]
    %endmacro

    %macro load_context 1
        wrfsbase %1
        mov rLWCSI, fs:[context.registers + regbank.lwcsi]
        mov rMCSI, fs:[context.registers + regbank.mcsi]
        mov rADDR1, fs:[context.registers + regbank.addr1]
        mov rADDR2, fs:[context.registers + regbank.addr2]
        mov rADDR3, fs:[context.registers + regbank.addr3]
        mov rADDR4, fs:[context.registers + regbank.addr4]
        mov rA, fs:[context.registers + regbank.a]
        mov rB, fs:[context.registers + regbank.b]
        mov rC, fs:[context.registers + regbank.c]
        mov rD, fs:[context.registers + regbank.d]
        mov rE, fs:[context.registers + regbank.e]
        mov rF, fs:[context.registers + regbank.f]
        mov rTEMPA, fs:[context.microcontext_stack_address]
        add rTEMPA, fs:[context.microcontext_stack_offset]
        wrgsbase rTEMPA

        movdqu xmm0, fs:[context.registers + regbank.xa ]
        movdqu xmm1, fs:[context.registers + regbank.xb ]
        movdqu xmm2, fs:[context.registers + regbank.xc ]
        movdqu xmm3, fs:[context.registers + regbank.xd ]
        movdqu xmm4, fs:[context.registers + regbank.xe ]
        movdqu xmm5, fs:[context.registers + regbank.xf ]
        movdqu xmm6, fs:[context.registers + regbank.xg ]
        movdqu xmm7, fs:[context.registers + regbank.xh ]
        movdqu xmm8, fs:[context.registers + regbank.xi ]
        movdqu xmm9, fs:[context.registers + regbank.xj ]
        movdqu xmm10, fs:[context.registers + regbank.xk ]
        movdqu xmm11, fs:[context.registers + regbank.xl ]
        movdqu xmm12, fs:[context.registers + regbank.xm ]
        movdqu xmm13, fs:[context.registers + regbank.xn ]
        movdqu xmm14, fs:[context.registers + regbank.xo ]
        movdqu xmm15, fs:[context.registers + regbank.xp ]
    %endmacro

    %macro store_context 0
        mov fs:[context.registers + regbank.lwcsi], rLWCSI
        mov fs:[context.registers + regbank.mcsi], rMCSI
        mov fs:[context.registers + regbank.addr1], rADDR1
        mov fs:[context.registers + regbank.addr2], rADDR2
        mov fs:[context.registers + regbank.addr3], rADDR3
        mov fs:[context.registers + regbank.addr4], rADDR4
        mov fs:[context.registers + regbank.a], rA
        mov fs:[context.registers + regbank.b], rB
        mov fs:[context.registers + regbank.c], rC
        mov fs:[context.registers + regbank.d], rD
        mov fs:[context.registers + regbank.e], rE
        mov fs:[context.registers + regbank.f], rF
        ; rdgsbase rTEMPA
        ; mov fs:[context.microcontext_stack_address], rTEMPA
        
        movdqu fs:[context.registers + regbank.xa ], xmm0
        movdqu fs:[context.registers + regbank.xb ], xmm1
        movdqu fs:[context.registers + regbank.xc ], xmm2
        movdqu fs:[context.registers + regbank.xd ], xmm3
        movdqu fs:[context.registers + regbank.xe ], xmm4
        movdqu fs:[context.registers + regbank.xf ], xmm5
        movdqu fs:[context.registers + regbank.xg ], xmm6
        movdqu fs:[context.registers + regbank.xh ], xmm7
        movdqu fs:[context.registers + regbank.xi ], xmm8
        movdqu fs:[context.registers + regbank.xj ], xmm9
        movdqu fs:[context.registers + regbank.xk ], xmm10
        movdqu fs:[context.registers + regbank.xl ], xmm11
        movdqu fs:[context.registers + regbank.xm ], xmm12
        movdqu fs:[context.registers + regbank.xn ], xmm13
        movdqu fs:[context.registers + regbank.xo ], xmm14
        movdqu fs:[context.registers + regbank.xp ], xmm15
    %endmacro

    ; zero data/address registers and memory locations which augment context state.
    ; Accessing any uninitialized locations in memory which are not zeroed out as part of context reset is undefined behavior.
    ;   The undefined behavior mainly just affects data stashes (other buffers are reset by clearing index values)
    %macro reset_context 1

        mov qword [%1+context.registers+regbank.temp], 0
        mov qword [%1+context.registers+regbank.mcsi], 0
        mov qword [%1+context.registers+regbank.addr1], 0
        mov qword [%1+context.registers+regbank.addr2], 0
        mov qword [%1+context.registers+regbank.addr3], 0
        mov qword [%1+context.registers+regbank.addr4], 0
        mov qword [%1+context.registers+regbank.a], 0
        mov qword [%1+context.registers+regbank.b], 0
        mov qword [%1+context.registers+regbank.c], 0
        mov qword [%1+context.registers+regbank.d], 0
        mov qword [%1+context.registers+regbank.e], 0
        mov qword [%1+context.registers+regbank.f], 0

        mov qword [%1+context.addr1], 0
        mov qword [%1+context.addr1_element_bound], 0
        mov qword [%1+context.addr1_array_bound], 0
        mov qword [%1+context.addr1_left], 0
        mov qword [%1+context.addr1_right], 0
        mov qword [%1+context.addr2], 0
        mov qword [%1+context.addr2_element_bound], 0
        mov qword [%1+context.addr2_array_bound], 0
        mov qword [%1+context.addr2_left], 0
        mov qword [%1+context.addr2_right], 0
        mov qword [%1+context.addr3], 0
        mov qword [%1+context.addr3_element_bound], 0
        mov qword [%1+context.addr3_array_bound], 0
        mov qword [%1+context.addr3_left], 0
        mov qword [%1+context.addr3_right], 0
        mov qword [%1+context.addr4], 0
        mov qword [%1+context.addr4_element_bound], 0
        mov qword [%1+context.addr4_array_bound], 0
        mov qword [%1+context.addr4_left], 0
        mov qword [%1+context.addr4_right], 0

        mov qword [%1+context.lwcall_position], 0
        mov qword [%1+context.mctx_bufpositions], 0
        mov qword [%1+context.mctx_bufpositions+8], 0

    %endmacro

    %macro tactyk_ret 0
        mov fs:[context.status], dword STATUS_HALT
        store_context
        rdfsbase rax
        load_runtimecontext rax
        mov r14, [rax + context.runtime_registers + 40]
        mov r15, [rax + context.runtime_registers + 48]
        mov rax, STATUS_HALT
        ret
    %endmacro

    %macro error 1
        mov fs:[context.status], dword %1
        store_context
        rdfsbase rax
        load_runtimecontext rax
        mov r14, [rax + context.runtime_registers + 40]
        mov r15, [rax + context.runtime_registers + 48]
        mov rax, %1
        ret
    %endmacro

    %macro errorlt 1
        jge %%pass
            error %1
        %%pass:
    %endmacro
    %macro errorle 1
        jg %%pass
            error %1
        %%pass:
    %endmacro
    %macro erroreq 1
        jne %%pass
            error %1
        %%pass:
    %endmacro
    %macro errorneq 1
        je %%pass
            error %1
        %%pass:
    %endmacro
    %macro errorge 1
        jl %%pass
            error %1
        %%pass:
    %endmacro
    %macro errorgt 1
        jle %%pass
            error %1
        %%pass:
    %endmacro


; push the instruction pointer onto the lightweight call stack
; for readability (as a side-effect of register spill avoidance), the instruction pointer is incremented in the function in which this
;   macro is used
%macro push_lwcall 1
    mov rTEMPD_32, fs:[context.lwcall_position%1]
    mov fs:[context.lwcall_stack+rTEMPD], rTEMPC_32
    add rTEMPD_32, 4
    cmp rTEMPD_32, 512*4
    errorge STATUS_STACK_OVERFLOW
    mov fs:[context.lwcall_position%1], rTEMPD_32
    xor rTEMPC, rTEMPC
    xor rTEMPD, rTEMPD
%endmacro

; restore instruction pointer from the lightweight call stack
%macro pop_lwcall 1
    mov rTEMPD_32, fs:[context.lwcall_position%1]
    sub rTEMPD_32, 4
    cmp rTEMPD_32, 0
    errorlt STATUS_STACK_UNDERFLOW
    mov fs:[context.lwcall_position%1], rTEMPD_32
    mov rLWCSI_32, fs:[context.lwcall_stack+rTEMPD]
    xor rTEMPD, rTEMPD
%endmacro

%macro validate_context_pointer 1
  mov rTEMPC, '-TACTYK-'
  xor rTEMPC, %1
  add rTEMPC, '-CTX'
  cmp rTEMPC, [%1 + context.signature]
  je .pass
  xor rTEMPC, rTEMPC
  mov rax, dword STATUS_UNSIGNED_CONTEXT
  ret
  .pass:
  xor rTEMPC, rTEMPC
%endmacro

run:
  validate_context_pointer rdi
  store_runtimecontext rdi
  load_context rdi
  mov fs:[context.status], dword STATUS_RUN
  mov rMAPPER, fs:[context.instruction_index]
  shl rMAPPER, 3
  add rMAPPER, fs:[context.program_map]
  jmp [rMAPPER]



