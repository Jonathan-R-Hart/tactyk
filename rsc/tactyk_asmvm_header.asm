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

    ; register allocation.  For now, these have been selected to align with UNIX calls.
    ; But it might be better to deliberately misalign it with UNIX calls (placing only constants and values not under script control
    ; on registers used to pass parameters), so as to further impair Return-oriented-programming (it already sidelines the call stack
    ; and places small values on the rsp and rbp registers so as to try to cause any unintended stack manipulation to trigger a segfault)

    %define rPROG   r12
    %define rIPTR   rbp
    %define rTEMPA  rax
    %define rTEMPC  rcx
    %define rTEMPD  rdx
    %define rADDR1  r14
    %define rADDR2  r15
    %define rADDR3  rbx
    %define rADDR4  r10
    %define rFBR    rsp
    %define rA      rdi
    %define rB      rsi
    %define rC      r11
    %define rD      r13
    %define rE      r8
    %define rF      r9


    %define rPROG_8 %[rPROG]_8
    %define rPROG_16 %[rPROG]_16
    %define rPROG_32 %[rPROG]_32
    %define rPROG_64 %[rPROG]_64


    %define rIPTR_8 %[rIPTR]_8
    %define rIPTR_16 %[rIPTR]_16
    %define rIPTR_32 %[rIPTR]_32
    %define rIPTR_64 %[rIPTR]_64

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

    %define rFBR_8 %[rFBR]_8
    %define rFBR_16 %[rFBR]_16
    %define rFBR_32 %[rFBR]_32
    %define rFBR_64 %[rFBR]_64

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


    ;%define INDEX_rCTX      0
    ;%define INDEX_rSTASH    1
    ;%define INDEX_rPROG     2
    ;%define INDEX_rIPTR     3
    ;%define INDEX_rTEMP     4
    ;%define INDEX_rADDR1    5
    ;%define INDEX_rADDR2    6
    ;%define INDEX_rADDR3    7
    ;%define INDEX_rADDR4    8
    ;%define INDEX_rFBR      9
    ;%define INDEX_rA        10
    ;%define INDEX_rB        11
    ;%define INDEX_rC        12
    ;%define INDEX_rD        13
    ;%define INDEX_rE        14
    ;%define INDEX_rF        15

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

    struc regbank
        qwords self,stash,prog,iptr,temp,addr1,addr2,addr3,addr4,fbr,a,b,c,d,e,f
    endstruc

    struc mctx
        qwords a,b,c,d,e,f, iptr, fbr
        qwords addr3, addr3_base, addr3_par_a, addr3_par_b
        qwords addr4, addr4_base, addr4_par_a, addr4_par_b
        qwords f0_low, f0_high, f1_low, f1_high, f2_low, f2_high, f3_low, f3_high, f4_low, f4_high, f5_low, f5_high, f6_low, f6_high, f7_low, f7_high
    endstruc

    ; microcontext size
    ; The microcontext buffer contains has space 2048 stack frames
    ; Microcontext position is a buffer offset which is stored.  Safety is handled by masking any input values.
    %define MCTX_BUFFER_QWORDS 65536
    %define MCTX_BUFFER_MASK 0xffe0


    struc controlstate
        .runtime_registers:     resq 16
        .stack_position:        resq 1
        .stack:                 resq 1024
        .static_contexts:       resq 1024
        .dcontext_position:     resq 1
        .dynamic_contexts:      resq 1024
    endstruc

    %define STASH_SCALE 32
    %define MCTXSTACK_SCALE 1024
    %define MCTX_BITSHIFT 8
    struc microcontext
        qwords a1,b1,c1,d1,e1,f1
        qwords a2,b2,c2,d2,e2,f2
        qwords a3,b3,c3,d3,e3,f3
        qwords iptr, fbr
        qwords addr1
        dwords addr1_element_bound, addr1_array_bound, addr1_left, addr1_right
        qwords addr2
        dwords addr2_element_bound, addr2_array_bound, addr2_left, addr2_right
        qwords addr3
        dwords addr3_element_bound, addr3_array_bound, addr3_left, addr3_right
        qwords addr4
        dwords addr4_element_bound, addr4_array_bound, addr4_left, addr4_right
    endstruc

    struc context

        ; 0

        qwords maxip, subcontext
        qwords memblocks, memblocks_count

        ; 32

        qwords addr1
        dwords addr1_element_bound, addr1_array_bound, addr1_left, addr1_right
        qwords addr2
        dwords addr2_element_bound, addr2_array_bound, addr2_left, addr2_right
        qwords addr3
        dwords addr3_element_bound, addr3_array_bound, addr3_left, addr3_right
        qwords addr4
        dwords addr4_element_bound, addr4_array_bound, addr4_left, addr4_right

        ; 128

        ; storage for return targets for lightweight function falls (minimal context changes)
        ; qwords lwcall_position1, lwcall_position2, lwcall_position3, lwcall_position4

        
        qwords lwcall_stack_address, microcontext_stack_address, microcontext_stack_offset, microcontext_stack_size

        ; 160
        
        ; .lwcall_stack:  resd 512
        ; addrN:  base address
        ; element_bound:  Offset of 8th from last byte in an element (stride-8)
        ; (for simpler overflow prevention, small words at high offsets are prohibited)
        ; array_bound: Offset of last element (array.length-1*stride)
        ; left  [circular buffer interpretation]: left position
        ; right  [circular buffer interpretation]: right position

        ; 2208

        ; .stash resb microcontext_size*STASH_SCALE

        ; qwords mctxstack_position1, mctxstack_position2, mctxstack_position3, mctxstack_position4
        ; qwords mctxstack1, mctxstack2, mctxstack3, mctxstack4, active_mctxstack

        ;.mctxstack resb microcontext_size*MCTXSTACK_SCALE

        ;qwords stack, stack_position, stack_bound
        ;qwords queue, queue_left, queue_right, queue_bound

        ;.mctxstash:     resq 32*8
        ;.mctx_bufpositions:  resw 8
        ;.mctxbuffer:    resq MCTX_BUFFER_QWORDS

        .diagnostic_out:resq 1024

        qwords controlstate, program, hl_program_ref, instruction_index, status, stepper
        .registers:             resq 16
    endstruc


    %unmacro qwords 1-*


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

    %define iSIZE 8

    ; General setup for fetching and executing the next operation
    ; This should be hard-coded into each function
    ;
    ;  jmp [rPTOG+rIPTR*iSIZE]
    ;  ... SomeOperation ...
    ;  add rIPTR, 1

    ; General setup for accesing memory
    ;
    ;  mov rGSTR, [<rADDR1|rADDR2|rSTAT>+OFFSET*<1|2|4|8>]


    %macro store_runtimecontext 0
        ;mov rTEMPA, [rCTX + context.controlstate]
        mov [rTEMPA + controlstate.runtime_registers + 0], rbx
        mov [rTEMPA + controlstate.runtime_registers + 8], rbp
        mov [rTEMPA + controlstate.runtime_registers + 16], rsp
        mov [rTEMPA + controlstate.runtime_registers + 24], r12
        mov [rTEMPA + controlstate.runtime_registers + 32], r13
        mov [rTEMPA + controlstate.runtime_registers + 40], r14
        mov [rTEMPA + controlstate.runtime_registers + 48], r15
        rdfsbase r12
        rdgsbase r13
        mov [rTEMPA + controlstate.runtime_registers + 56], r12
        mov [rTEMPA + controlstate.runtime_registers + 64], r13
    %endmacro

    %macro load_runtimecontext 0
        mov rTEMPA, fs:[context.controlstate]
        mov r12, [rTEMPA + controlstate.runtime_registers + 56]
        mov r13, [rTEMPA + controlstate.runtime_registers + 64]
        wrfsbase r12
        wrgsbase r13
        mov rbx, [rTEMPA + controlstate.runtime_registers + 0]
        mov rbp, [rTEMPA + controlstate.runtime_registers + 8]
        mov rsp, [rTEMPA + controlstate.runtime_registers + 16]
        mov r12, [rTEMPA + controlstate.runtime_registers + 24]
        mov r13, [rTEMPA + controlstate.runtime_registers + 32]
        mov r14, [rTEMPA + controlstate.runtime_registers + 40]
        mov r15, [rTEMPA + controlstate.runtime_registers + 48]
    %endmacro

    %macro load_context 0
        ;mov rSTASH, fs:[context.registers + regbank.stash]
        mov rPROG, fs:[context.registers + regbank.prog]
        mov rIPTR, fs:[context.registers + regbank.iptr]
        ;mov rTEMP, [context.registers + regbank.temp]
        mov rADDR1, fs:[context.registers + regbank.addr1]
        mov rADDR2, fs:[context.registers + regbank.addr2]
        mov rADDR3, fs:[context.registers + regbank.addr3]
        mov rADDR4, fs:[context.registers + regbank.addr4]
        mov rFBR, fs:[context.registers + regbank.fbr]
        mov rA, fs:[context.registers + regbank.a]
        mov rB, fs:[context.registers + regbank.b]
        mov rC, fs:[context.registers + regbank.c]
        mov rD, fs:[context.registers + regbank.d]
        mov rE, fs:[context.registers + regbank.e]
        mov rF, fs:[context.registers + regbank.f]
        mov rTEMPA, fs:[context.microcontext_stack_address]
        add rTEMPA, fs:[context.microcontext_stack_offset]
        wrgsbase rTEMPA
    %endmacro

    %macro store_context 0
        ;mov fs:[context.registers + regbank.stash], rSTASH
        ;mov fs:[context.registers + regbank.prog], rPROG
        mov fs:[context.registers + regbank.iptr], rIPTR
        ;mov fs:[context.registers + regbank.temp], rTEMP
        mov fs:[context.registers + regbank.addr1], rADDR1
        mov fs:[context.registers + regbank.addr2], rADDR2
        mov fs:[context.registers + regbank.addr3], rADDR3
        mov fs:[context.registers + regbank.addr4], rADDR4
        mov fs:[context.registers + regbank.fbr], rFBR
        mov fs:[context.registers + regbank.a], rA
        mov fs:[context.registers + regbank.b], rB
        mov fs:[context.registers + regbank.c], rC
        mov fs:[context.registers + regbank.d], rD
        mov fs:[context.registers + regbank.e], rE
        mov fs:[context.registers + regbank.f], rF
        ; rdgsbase rTEMPA
        ; mov fs:[context.microcontext_stack_address], rTEMPA
    %endmacro

    ; zero data/address registers and memory locations which augment context state.
    ; Accessing any uninitialized locations in memory which are not zeroed out as part of context reset is undefined behavior.
    ;   The undefined behavior mainly just affects data stashes (other buffers are reset by clearing index values)
    %macro reset_context 1

        mov qword [%1+context.registers+regbank.temp], 0
        mov qword [%1+context.registers+regbank.fbr], 0
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
        ;swap_context
        store_context
        load_runtimecontext
        rdfsbase rax
        ;mov rax, rCTX
        ret
    %endmacro

    %macro error 1
        mov fs:[context.status], dword %1
        ;swap_context
        store_context
        load_runtimecontext
        rdfsbase rax
        ;mov rax, rCTX
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
    ;mov [rCTX+context.internalH], rTEMPD
    errorlt STATUS_STACK_UNDERFLOW
    mov fs:[context.lwcall_position%1], rTEMPD_32
    mov rIPTR_32, fs:[context.lwcall_stack+rTEMPD]
    xor rTEMPD, rTEMPD
%endmacro

run:
  mov rTEMPA, [rdi + context.controlstate]
  store_runtimecontext
  wrfsbase rdi
  load_context
  mov fs:[context.status], dword STATUS_RUN
  mov rTEMPA, fs:[context.instruction_index]
  ; exception - In this one specific case, the temp register ca not be cleared before exiting an instruction.
  jmp [rPROG+rTEMPA*8]