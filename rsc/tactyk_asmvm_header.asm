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
        
    %define rA~0    rA
    %define rA~1    rB
    %define rA~2    rC
    %define rA~3    rD
    %define rA~4    rE
    %define rA~5    rF

    %define rB~0    rB
    %define rB~1    rC
    %define rB~2    rD
    %define rB~3    rE
    %define rB~4    rF
    %define rB~5    rA

    %define rC~0    rC
    %define rC~1    rD
    %define rC~2    rE
    %define rC~3    rF
    %define rC~4    rA
    %define rC~5    rB

    %define rD~0    rD
    %define rD~1    rE
    %define rD~2    rF
    %define rD~3    rA
    %define rD~4    rB
    %define rD~5    rC

    %define rE~0    rE
    %define rE~1    rF
    %define rE~2    rA
    %define rE~3    rB
    %define rE~4    rC
    %define rE~5    rD

    %define rF~0    rF
    %define rF~1    rA
    %define rF~2    rB
    %define rF~3    rC
    %define rF~4    rD
    %define rF~5    rE
  
    %define xA      xmm0
    %define xB      xmm1
    %define xC      xmm2
    %define xD      xmm3
    %define xE      xmm4
    %define xF      xmm5
    %define xG      xmm6
    %define xH      xmm7
    %define xI      xmm8
    %define xJ      xmm9
    %define xK      xmm10
    %define xL      xmm11
    %define xM      xmm12
    %define xN      xmm13
    %define xO      xmm14
    %define xTEMPA  xmm15
    %define xTEMPB  xmm16

    %define xA~0    xA
    %define xA~1    xB
    %define xA~2    xC
    %define xA~3    xD
    %define xA~4    xE
    %define xA~5    xF
    %define xA~6    xG
    %define xA~7    xH
    %define xA~8    xI
    %define xA~9    xJ
    %define xA~10   xK
    %define xA~11   xL
    %define xA~12   xM
    %define xA~13   xN
    
    %define xB~0    xB
    %define xB~1    xC
    %define xB~2    xD
    %define xB~3    xE
    %define xB~4    xF
    %define xB~5    xG
    %define xB~6    xH
    %define xB~7    xI
    %define xB~8    xJ
    %define xB~9    xK
    %define xB~10   xL
    %define xB~11   xM
    %define xB~12   xN
    %define xB~13   xA

    %define xC~0    xC
    %define xC~1    xD
    %define xC~2    xE
    %define xC~3    xF
    %define xC~4    xG
    %define xC~5    xH
    %define xC~6    xI
    %define xC~7    xJ
    %define xC~8    xK
    %define xC~9    xL
    %define xC~10   xM
    %define xC~11   xN
    %define xC~12   xA
    %define xC~13   xB

    %define xD~0    xD
    %define xD~1    xE
    %define xD~2    xF
    %define xD~3    xG
    %define xD~4    xH
    %define xD~5    xI
    %define xD~6    xJ
    %define xD~7    xK
    %define xD~8    xL
    %define xD~9    xM
    %define xD~10   xN
    %define xD~11   xA
    %define xD~12   xB
    %define xD~13   xC

    %define xE~0    xE
    %define xE~1    xF
    %define xE~2    xG
    %define xE~3    xH
    %define xE~4    xI
    %define xE~5    xJ
    %define xE~6    xK
    %define xE~7    xL
    %define xE~8    xM
    %define xE~9    xN
    %define xE~10   xA
    %define xE~11   xB
    %define xE~12   xC
    %define xE~13   xD

    %define xF~0    xF
    %define xF~1    xG
    %define xF~2    xH
    %define xF~3    xI
    %define xF~4    xJ
    %define xF~5    xK
    %define xF~6    xL
    %define xF~7    xM
    %define xF~8    xN
    %define xF~9    xA
    %define xF~10   xB
    %define xF~11   xC
    %define xF~12   xD
    %define xF~13   xE

    %define xG~0    xG
    %define xG~1    xH
    %define xG~2    xI
    %define xG~3    xJ
    %define xG~4    xK
    %define xG~5    xL
    %define xG~6    xM
    %define xG~7    xN
    %define xG~8    xA
    %define xG~9    xB
    %define xG~10   xC
    %define xG~11   xD
    %define xG~12   xE
    %define xG~13   xF

    %define xH~0    xH
    %define xH~1    xI
    %define xH~2    xJ
    %define xH~3    xK
    %define xH~4    xL
    %define xH~5    xM
    %define xH~6    xN
    %define xH~7    xA
    %define xH~8    xB
    %define xH~9    xC
    %define xH~10   xD
    %define xH~11   xE
    %define xH~12   xF
    %define xH~13   xG

    %define xI~0    xI
    %define xI~1    xJ
    %define xI~2    xK
    %define xI~3    xL
    %define xI~4    xM
    %define xI~5    xN
    %define xI~6    xA
    %define xI~7    xB
    %define xI~8    xC
    %define xI~9    xD
    %define xI~10   xE
    %define xI~11   xF
    %define xI~12   xG
    %define xI~13   xH

    %define xJ~0    xJ
    %define xJ~1    xK
    %define xJ~2    xL
    %define xJ~3    xM
    %define xJ~4    xN
    %define xJ~5    xA
    %define xJ~6    xB
    %define xJ~7    xC
    %define xJ~8    xD
    %define xJ~9    xE
    %define xJ~10   xF
    %define xJ~11   xG
    %define xJ~12   xH
    %define xJ~13   xI

    %define xK~0    xK
    %define xK~1    xL
    %define xK~2    xM
    %define xK~3    xN
    %define xK~4    xA
    %define xK~5    xB
    %define xK~6    xC
    %define xK~7    xD
    %define xK~8    xE
    %define xK~9    xF
    %define xK~10   xG
    %define xK~11   xH
    %define xK~12   xI
    %define xK~13   xJ

    %define xL~0    xL
    %define xL~1    xM
    %define xL~2    xN
    %define xL~3    xA
    %define xL~4    xB
    %define xL~5    xC
    %define xL~6    xD
    %define xL~7    xE
    %define xL~8    xF
    %define xL~9    xG
    %define xL~10   xH
    %define xL~11   xI
    %define xL~12   xJ
    %define xL~13   xK

    %define xM~0    xM
    %define xM~1    xN
    %define xM~2    xA
    %define xM~3    xB
    %define xM~4    xC
    %define xM~5    xD
    %define xM~6    xE
    %define xM~7    xF
    %define xM~8    xG
    %define xM~9    xH
    %define xM~10   xI
    %define xM~11   xJ
    %define xM~12   xK
    %define xM~13   xL

    %define xN~0    xN
    %define xN~1    xA
    %define xN~2    xB
    %define xN~3    xC
    %define xN~4    xD
    %define xN~5    xE
    %define xN~6    xF
    %define xN~7    xG
    %define xN~8    xH
    %define xN~9    xI
    %define xN~10   xJ
    %define xN~11   xK
    %define xN~12   xL
    %define xN~13   xM
    
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
    
    %define xA xmm0
    %define xB xmm1
    %define xC xmm2
    %define xD xmm3
    %define xE xmm4
    %define xF xmm5
    %define xG xmm6
    %define xH xmm7
    %define xI xmm8
    %define xJ xmm9
    %define xK xmm10
    %define xL xmm11
    %define xM xmm12
    %define xN xmm13
    %define xTEMPA xmm14
    %define xTEMPB xmm15
    
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
    
    struc rbtactyk
        qwords prog,lwcsi,mcsi,tempa,tempc,tempd,addr1,addr2,addr3,addr4,a,b,c,d,e,f
        owords xa,xb,xc,xd, xe,xf,xg,xh, xi,xj,xk,xl, xm,xn,xo,xp
        qwords, fs,gs
        dwords mxcsr
        unused:  resd 27
    endstruc

    struc rbnative
        qwords rax,rbx,rcx,rdx, rbp,rsp, rsi,rdi, r8,r9,r10,r11,r12,r13,r14,r15
        owords xmm0,xmm1,xmm2,xmm3, xmm4,xmm5,xmm6,xmm7, xmm8,xmm9,xmm10,xmm11, xmm12,xmm13,xmm14,xmm15
        qwords, fs,gs
        dwords mxcsr
        unused:  resd 27
    endstruc

    struc tvmstackentry
      qwords  source_program, source_memblocks, source_memblocks_count
              .source_exec_position: dwords source_return_index, source_max_iptr
              .source_stack_floor: dwords source_lwcallstack_floor, source_mctxstack_floor
              dwords source_lwcallstack_position, source_mctxstack_position
      qwords  target_program, target_memblocks, target_memblocks_count, target_function_jumptable
              .target_exec_position: dwords target_jump_index, dest_max_iptr
    endstruc

    %define stacksize 1024
    struc tactyk_stack
        .stack_position:        resq 1
        .stack_lock:            resq 1
        .stack:                 resb stacksize * tvmstackentry_size
    endstruc
    
    struc program_dec
      qwords instruction_count, instruction_jumptable, function_count, function_jumptable, memblocks, memblocks_count
    endstruc
    
    struc tactyk_vm
      qwords program_count, program_list
    endstruc
    
    struc microcontext
        .a: qwords al, ah
        .b: qwords bl, bh
        .c: qwords cl, ch
        .d: qwords dl, dh
        .e: qwords el, eh
        .f: qwords fl, fh
        .g: qwords gl, gh
        .h: qwords hl, hh
        .i: qwords il, ih
        .j: qwords jl, jh
        .k: qwords kl, kh
        .l: qwords ll, lh
        .m: qwords ml, mh
        .n: qwords nl, nh
        .o: qwords ol, oh
        .p: qwords pl, ph
        .q: qwords ql, qh
        .r: qwords rl, rh
        .s: qwords sl, sh
        .t: qwords tl, th
        .u: qwords ul, uh
        .v: qwords vl, vh
        .w: qwords wl, wh
        .x: qwords xl, xh
        .y: qwords yl, yh
        .z: qwords zl, zh
        .s26: qwords s26l, s26h
        .s27: qwords s27l, s27h
        .s28: qwords s28l, s28h
        .s29: qwords s29l, s29h
        .s30: qwords s30l, s30h
        .s31: qwords s31l, s31h
    endstruc
    
    %define microcontext_size_bits 9
    %define microcontext_stack_size 65536
    %define lwcall_stack_size 65536
    %define lwcall_stack_max 65535
    
    struc context
        
        ; 0
        
        qwords instruction_count, subcontext
        qwords memblocks, memblocks_count
        
        ; 32
        
        qwords addr1
        dwords addr1_element_bound, addr1_array_bound, addr1_memblock_index, addr1_offset
        qwords addr2
        dwords addr2_element_bound, addr2_array_bound, addr2_memblock_index, addr2_offset
        qwords addr3
        dwords addr3_element_bound, addr3_array_bound, addr3_memblock_index, addr3_offset
        qwords addr4
        dwords addr4_element_bound, addr4_array_bound, addr4_memblock_index, addr4_offset

        ; 128

        qwords lwcall_stack_address, microcontext_stack_address, microcontext_stack_offset
        .floor: dwords lwcall_floor, microcontext_floor
        
        qwords vm, stack, program_map, hl_program_ref, instruction_index, status, signature, extra
        
        owords fpu_a, fpu_b, fpu_c, fpu_d,  fpu_e, fpu_f, fpu_g, fpu_h

        .registers:  resq 64
        .runtime_registers:  resq 64
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
        mov [%1 + context.runtime_registers + rbnative.rbx], rbx
        mov [%1 + context.runtime_registers + rbnative.rbp], rbp
        mov [%1 + context.runtime_registers + rbnative.rsp], rsp
        mov [%1 + context.runtime_registers + rbnative.r12], r12
        mov [%1 + context.runtime_registers + rbnative.r13], r13
        mov [%1 + context.runtime_registers + rbnative.r14], r14
        mov [%1 + context.runtime_registers + rbnative.r15], r15
        rdfsbase r12
        rdgsbase r13
        mov [%1 + context.runtime_registers + rbnative.fs], r12
        mov [%1 + context.runtime_registers + rbnative.gs], r13
        stmxcsr [%1 + context.runtime_registers + rbnative.mxcsr]
        
        ; supposedly not needed:
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm0  ], xmm0
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm1  ], xmm1
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm2  ], xmm2
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm3  ], xmm3
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm4  ], xmm4
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm5  ], xmm5
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm6  ], xmm6
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm7  ], xmm7
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm8  ], xmm8
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm9  ], xmm9
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm10 ], xmm10
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm11 ], xmm11
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm12 ], xmm12
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm13 ], xmm13
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm14 ], xmm14
        ;movdqu [%1 + context.runtime_registers + rbnative.xmm15 ], xmm15
    %endmacro

    %macro load_runtimecontext 1
        mov r12, [%1 + context.runtime_registers + rbnative.fs]
        mov r13, [%1 + context.runtime_registers + rbnative.gs]
        wrfsbase r12
        wrgsbase r13
        mov rbx, [%1 + context.runtime_registers + rbnative.rbx]
        mov rbp, [%1 + context.runtime_registers + rbnative.rbp]
        mov rsp, [%1 + context.runtime_registers + rbnative.rsp]
        mov r12, [%1 + context.runtime_registers + rbnative.r12]
        mov r13, [%1 + context.runtime_registers + rbnative.r13]
        ldmxcsr [%1 + context.runtime_registers + rbnative.mxcsr]

        ; supposedly not needed:
        ;movdqu xmm0,  [%1 + context.runtime_registers + rbnative.xmm0  ]
        ;movdqu xmm1,  [%1 + context.runtime_registers + rbnative.xmm1  ]
        ;movdqu xmm2,  [%1 + context.runtime_registers + rbnative.xmm2  ]
        ;movdqu xmm3,  [%1 + context.runtime_registers + rbnative.xmm3  ]
        ;movdqu xmm4,  [%1 + context.runtime_registers + rbnative.xmm4  ]
        ;movdqu xmm5,  [%1 + context.runtime_registers + rbnative.xmm5  ]
        ;movdqu xmm6,  [%1 + context.runtime_registers + rbnative.xmm6  ]
        ;movdqu xmm7,  [%1 + context.runtime_registers + rbnative.xmm7  ]
        ;movdqu xmm8,  [%1 + context.runtime_registers + rbnative.xmm8  ]
        ;movdqu xmm9,  [%1 + context.runtime_registers + rbnative.xmm9  ]
        ;movdqu xmm10, [%1 + context.runtime_registers + rbnative.xmm10 ]
        ;movdqu xmm11, [%1 + context.runtime_registers + rbnative.xmm11 ]
        ;movdqu xmm12, [%1 + context.runtime_registers + rbnative.xmm12 ]
        ;movdqu xmm13, [%1 + context.runtime_registers + rbnative.xmm13 ]
        ;movdqu xmm14, [%1 + context.runtime_registers + rbnative.xmm14 ]
        ;movdqu xmm15, [%1 + context.runtime_registers + rbnative.xmm15 ]
    %endmacro

    %macro load_context 1
        wrfsbase %1
        mov rLWCSI, fs:[context.registers + rbtactyk.lwcsi + random_const_FS]
        mov rMCSI, fs:[context.registers + rbtactyk.mcsi + random_const_FS]
        mov rADDR1, fs:[context.registers + rbtactyk.addr1 + random_const_FS]
        mov rADDR2, fs:[context.registers + rbtactyk.addr2 + random_const_FS]
        mov rADDR3, fs:[context.registers + rbtactyk.addr3 + random_const_FS]
        mov rADDR4, fs:[context.registers + rbtactyk.addr4 + random_const_FS]
        mov rA, fs:[context.registers + rbtactyk.a + random_const_FS]
        mov rB, fs:[context.registers + rbtactyk.b + random_const_FS]
        mov rC, fs:[context.registers + rbtactyk.c + random_const_FS]
        mov rD, fs:[context.registers + rbtactyk.d + random_const_FS]
        mov rE, fs:[context.registers + rbtactyk.e + random_const_FS]
        mov rF, fs:[context.registers + rbtactyk.f + random_const_FS]
        mov rTEMPA, fs:[context.microcontext_stack_address + random_const_FS]
        add rTEMPA, fs:[context.microcontext_stack_offset + random_const_FS]
        sub rTEMPA, random_const_GS
        wrgsbase rTEMPA

        movdqu xmm0, fs:[context.registers + rbtactyk.xa  + random_const_FS]
        movdqu xmm1, fs:[context.registers + rbtactyk.xb  + random_const_FS]
        movdqu xmm2, fs:[context.registers + rbtactyk.xc  + random_const_FS]
        movdqu xmm3, fs:[context.registers + rbtactyk.xd  + random_const_FS]
        movdqu xmm4, fs:[context.registers + rbtactyk.xe  + random_const_FS]
        movdqu xmm5, fs:[context.registers + rbtactyk.xf  + random_const_FS]
        movdqu xmm6, fs:[context.registers + rbtactyk.xg  + random_const_FS]
        movdqu xmm7, fs:[context.registers + rbtactyk.xh  + random_const_FS]
        movdqu xmm8, fs:[context.registers + rbtactyk.xi  + random_const_FS]
        movdqu xmm9, fs:[context.registers + rbtactyk.xj  + random_const_FS]
        movdqu xmm10, fs:[context.registers + rbtactyk.xk  + random_const_FS]
        movdqu xmm11, fs:[context.registers + rbtactyk.xl  + random_const_FS]
        movdqu xmm12, fs:[context.registers + rbtactyk.xm  + random_const_FS]
        movdqu xmm13, fs:[context.registers + rbtactyk.xn  + random_const_FS]
        movdqu xmm14, fs:[context.registers + rbtactyk.xo  + random_const_FS]
        movdqu xmm15, fs:[context.registers + rbtactyk.xp  + random_const_FS]
        mov rTEMPA_32, fs:[context.registers + rbtactyk.mxcsr + random_const_FS ]
        cmp rTEMPA, 0
        jne .ldctx_restoremxcsr
        stmxcsr fs:[context.registers + rbtactyk.mxcsr + random_const_FS ]
        mov rTEMPA_32, fs:[context.registers + rbtactyk.mxcsr + random_const_FS ]
        and rTEMPA_32, 0xffff9fff
        or rTEMPA_32,  0x00006000
        mov fs:[context.registers + rbtactyk.mxcsr + random_const_FS ], rTEMPA_32
        ldmxcsr fs:[context.registers + rbtactyk.mxcsr + random_const_FS]
        jmp .ldctx_end
        .ldctx_restoremxcsr:
        ldmxcsr fs:[context.registers + rbtactyk.mxcsr + random_const_FS ]
        .ldctx_end:
    %endmacro

    %macro store_context 0
        mov fs:[context.registers + rbtactyk.lwcsi + random_const_FS], rLWCSI
        mov fs:[context.registers + rbtactyk.mcsi + random_const_FS], rMCSI
        mov fs:[context.registers + rbtactyk.addr1 + random_const_FS], rADDR1
        mov fs:[context.registers + rbtactyk.addr2 + random_const_FS], rADDR2
        mov fs:[context.registers + rbtactyk.addr3 + random_const_FS], rADDR3
        mov fs:[context.registers + rbtactyk.addr4 + random_const_FS], rADDR4
        mov fs:[context.registers + rbtactyk.a + random_const_FS], rA
        mov fs:[context.registers + rbtactyk.b + random_const_FS], rB
        mov fs:[context.registers + rbtactyk.c + random_const_FS], rC
        mov fs:[context.registers + rbtactyk.d + random_const_FS], rD
        mov fs:[context.registers + rbtactyk.e + random_const_FS], rE
        mov fs:[context.registers + rbtactyk.f + random_const_FS], rF
        ; rdgsbase rTEMPA
        ; mov fs:[context.microcontext_stack_address + random_const_FS], rTEMPA
        
        movdqu fs:[context.registers + rbtactyk.xa + random_const_FS ], xmm0
        movdqu fs:[context.registers + rbtactyk.xb + random_const_FS ], xmm1
        movdqu fs:[context.registers + rbtactyk.xc + random_const_FS ], xmm2
        movdqu fs:[context.registers + rbtactyk.xd + random_const_FS ], xmm3
        movdqu fs:[context.registers + rbtactyk.xe + random_const_FS ], xmm4
        movdqu fs:[context.registers + rbtactyk.xf + random_const_FS ], xmm5
        movdqu fs:[context.registers + rbtactyk.xg + random_const_FS ], xmm6
        movdqu fs:[context.registers + rbtactyk.xh + random_const_FS ], xmm7
        movdqu fs:[context.registers + rbtactyk.xi + random_const_FS ], xmm8
        movdqu fs:[context.registers + rbtactyk.xj + random_const_FS ], xmm9
        movdqu fs:[context.registers + rbtactyk.xk + random_const_FS ], xmm10
        movdqu fs:[context.registers + rbtactyk.xl + random_const_FS ], xmm11
        movdqu fs:[context.registers + rbtactyk.xm + random_const_FS ], xmm12
        movdqu fs:[context.registers + rbtactyk.xn + random_const_FS ], xmm13
        movdqu fs:[context.registers + rbtactyk.xo + random_const_FS ], xmm14
        movdqu fs:[context.registers + rbtactyk.xp + random_const_FS ], xmm15
        stmxcsr fs:[context.registers + rbtactyk.mxcsr + random_const_FS ]
    %endmacro

    ; zero data/address registers and memory locations which augment context state.
    ; Accessing any uninitialized locations in memory which are not zeroed out as part of context reset is undefined behavior.
    ;   The undefined behavior mainly just affects data stashes (other buffers are reset by clearing index values)
    %macro reset_context 1

        mov qword [%1+context.registers+rbtactyk.temp], 0
        mov qword [%1+context.registers+rbtactyk.mcsi], 0
        mov qword [%1+context.registers+rbtactyk.addr1], 0
        mov qword [%1+context.registers+rbtactyk.addr2], 0
        mov qword [%1+context.registers+rbtactyk.addr3], 0
        mov qword [%1+context.registers+rbtactyk.addr4], 0
        mov qword [%1+context.registers+rbtactyk.a], 0
        mov qword [%1+context.registers+rbtactyk.b], 0
        mov qword [%1+context.registers+rbtactyk.c], 0
        mov qword [%1+context.registers+rbtactyk.d], 0
        mov qword [%1+context.registers+rbtactyk.e], 0
        mov qword [%1+context.registers+rbtactyk.f], 0

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
        mov fs:[context.status + random_const_FS], dword STATUS_HALT
        store_context
        rdfsbase rax
        add rax, random_const_FS
        load_runtimecontext rax
        mov r14, [rax + context.runtime_registers + rbnative.r14 ]
        mov r15, [rax + context.runtime_registers + rbnative.r15 ]
        mov rax, STATUS_HALT
        ret
    %endmacro

    %macro error 1
        mov fs:[context.status + random_const_FS], dword %1
        store_context
        rdfsbase rax
        add rax, random_const_FS
        load_runtimecontext rax
        mov r14, [rax + context.runtime_registers + rbnative.r14 ]
        mov r15, [rax + context.runtime_registers + rbnative.r15 ]
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
    mov rTEMPD_32, fs:[context.lwcall_position%1 + random_const_FS]
    mov fs:[context.lwcall_stack+rTEMPD + random_const_FS], rTEMPC_32
    add rTEMPD_32, 4
    cmp rTEMPD_32, 512*4
    errorge STATUS_STACK_OVERFLOW
    mov fs:[context.lwcall_position%1 + random_const_FS], rTEMPD_32
    xor rTEMPC, rTEMPC
    xor rTEMPD, rTEMPD
%endmacro

; restore instruction pointer from the lightweight call stack
%macro pop_lwcall 1
    mov rTEMPD_32, fs:[context.lwcall_position%1 + random_const_FS]
    sub rTEMPD_32, 4
    cmp rTEMPD_32, 0
    errorlt STATUS_STACK_UNDERFLOW
    mov fs:[context.lwcall_position%1 + random_const_FS], rTEMPD_32
    mov rLWCSI_32, fs:[context.lwcall_stack+rTEMPD + random_const_FS]
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
  sub rdi, random_const_FS
  load_context rdi
  mov fs:[context.status + random_const_FS], dword STATUS_RUN
  mov rMAPPER, fs:[context.instruction_index + random_const_FS]
  shl rMAPPER, 3
  add rMAPPER, fs:[context.program_map + random_const_FS]
  jmp [rMAPPER]



