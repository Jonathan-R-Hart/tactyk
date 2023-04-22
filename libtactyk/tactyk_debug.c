
#include <stdio.h>
#include <stdint.h>

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_debug.h"

void tactyk_debug__configure_api(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-ctx", tactyk_debug__print_context);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-mbind", tactyk_debug__print_mbind);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-stack", tactyk_debug__print_vmstack);
    tactyk_emit__add_tactyk_apifunc(emitctx, "break", tactyk_debug__break);
}

void tactyk_debug__write_context(struct tactyk_asmvm__Context *ctx, FILE *stream) {
    fprintf(stream, "===== TACTYK CONTEXT ===================================================================================\n");
    fprintf(stream, "| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20.14g | %-20.14g |\n", ctx, ctx->reg.xA.f64[1], ctx->reg.xA.f64[0]);
    fprintf(stream, "| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20.14g | %-20.14g |\n", ctx->microcontext_stack, ctx->reg.xB.f64[1], ctx->reg.xB.f64[0]);
    fprintf(stream, "| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20.14g | %-20.14g |\n", ctx->reg.xC.f64[1], ctx->reg.xC.f64[0]);
    fprintf(stream, "| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20.14g | %-20.14g |\n", ctx->reg.xD.f64[1], ctx->reg.xD.f64[0]);
    fprintf(stream, "| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20.14g | %-20.14g |\n", ctx->reg.xE.f64[1], ctx->reg.xE.f64[0]);
    fprintf(stream, "| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20.14g | %-20.14g |\n", ctx->reg.rLWCSI, ctx->reg.xF.f64[1], ctx->reg.xF.f64[0]);
    fprintf(stream, "| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20.14g | %-20.14g |\n", ctx->reg.rMCSI, ctx->reg.xG.f64[1], ctx->reg.xG.f64[0]);
    fprintf(stream, "| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20.14g | %-20.14g |\n", ctx->reg.rTEMPS, ctx->reg.xH.f64[1], ctx->reg.xH.f64[0]);
    fprintf(stream, "| rADDR1   (r14) | %20p | xI (xmm8)      | %-20.14g | %-20.14g |\n", ctx->reg.rADDR1, ctx->reg.xI.f64[1], ctx->reg.xI.f64[0]);
    fprintf(stream, "| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20.14g | %-20.14g |\n", ctx->reg.rADDR2, ctx->reg.xJ.f64[1], ctx->reg.xJ.f64[0]);
    fprintf(stream, "| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20.14g | %-20.14g |\n", ctx->reg.rADDR3, ctx->reg.xK.f64[1], ctx->reg.xK.f64[0]);
    fprintf(stream, "| rADDR4   (r10) | %20p | xL (xmm11)     | %-20.14g | %-20.14g |\n", ctx->reg.rADDR4, ctx->reg.xL.f64[1], ctx->reg.xL.f64[0]);
    fprintf(stream, "| rA       (rdi) | %20jd | xM (xmm12)     | %-20.14g | %-20.14g |\n", ctx->reg.rA, ctx->reg.xM.f64[1], ctx->reg.xM.f64[0]);
    fprintf(stream, "| rB       (rsi) | %20jd | xN (xmm13)     | %-20.14g | %-20.14g |\n", ctx->reg.rB, ctx->reg.xN.f64[1], ctx->reg.xN.f64[0]);
    fprintf(stream, "| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20.14g | %-20.14g |\n", ctx->reg.rC, ctx->reg.xTEMPA.f64[1], ctx->reg.xTEMPA.f64[0]);
    fprintf(stream, "| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20.14g | %-20.14g |\n", ctx->reg.rD, ctx->reg.xTEMPB.f64[1], ctx->reg.xTEMPB.f64[0]);
    fprintf(stream, "| rE       (r8)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rE);
    fprintf(stream, "| rF       (r9)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rF);
    fprintf(stream, "========================================================================================================\n");
}
void tactyk_debug__write_mbind(struct tactyk_asmvm__Context *ctx, FILE *stream) {
    fprintf(stream, "===== BOUND MEMORY ======================\n");
    struct tactyk_asmvm__memblock_lowlevel *mbll = &ctx->active_memblocks[0];
    fprintf(stream, "| ADDR1          | %20p |\n", mbll->base_address);
    fprintf(stream, "| Element Bound  | %20d |\n", mbll->element_bound);
    fprintf(stream, "| Array Bound    | %20d |\n", mbll->array_bound);
    fprintf(stream, "| Memblock Index | %20d |\n", mbll->memblock_index);
    fprintf(stream, "| Offset         | %20d |\n", mbll->offset);
    fprintf(stream, "| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[1];
    fprintf(stream, "| ADDR2          | %20p |\n", mbll->base_address);
    fprintf(stream, "| Element Bound  | %20d |\n", mbll->element_bound);
    fprintf(stream, "| Array Bound    | %20d |\n", mbll->array_bound);
    fprintf(stream, "| Memblock Index | %20d |\n", mbll->memblock_index);
    fprintf(stream, "| Offset         | %20d |\n", mbll->offset);
    fprintf(stream, "| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[2];
    fprintf(stream, "| ADDR2          | %20p |\n", mbll->base_address);
    fprintf(stream, "| Element Bound  | %20d |\n", mbll->element_bound);
    fprintf(stream, "| Array Bound    | %20d |\n", mbll->array_bound);
    fprintf(stream, "| Memblock Index | %20d |\n", mbll->memblock_index);
    fprintf(stream, "| Offset         | %20d |\n", mbll->offset);
    fprintf(stream, "| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[3];
    fprintf(stream, "| ADDR2          | %20p |\n", mbll->base_address);
    fprintf(stream, "| Element Bound  | %20d |\n", mbll->element_bound);
    fprintf(stream, "| Array Bound    | %20d |\n", mbll->array_bound);
    fprintf(stream, "| Memblock Index | %20d |\n", mbll->memblock_index);
    fprintf(stream, "| Offset         | %20d |\n", mbll->offset);
    fprintf(stream, "=========================================\n");
}
void tactyk_debug__write_vmstack(struct tactyk_asmvm__Stack *stack, FILE *stream) {
    fprintf(stream, "===== TACTYK VM STACK =======================================================================================================\n");
    char *lockstate;
    if (stack->stack_lock) {
        lockstate = "LOCKED";
    }
    else {
        lockstate = "UNLOCKED";
    }
    fprintf(stream, "| state: %-8s |                                                                                                         |\n", lockstate);
    fprintf(stream, "|---------------------------------------------------------------------------------------------------------------------------|\n");
    fprintf(stream, "|   pos |    src-commands |   return-target |  lwc-floor | mctx-floor |   dest-commands |  dest-functions |     jump-target |\n");
    fprintf(stream, "|---------------------------------------------------------------------------------------------------------------------------|\n");
    for (int64_t pos = 0; pos <= stack->stack_position; pos += 1) {
        struct tactyk_asmvm__vm_stack_entry *entry = &stack->entries[pos];
        fprintf(stream, "| %5ju | %15p | %15ju | %10u | %10u | %15p | %15p | %15ju |\n", pos, entry->source_command_map, entry->source_return_index, entry->source_lwcallstack_floor, entry->source_mctxstack_floor, entry->dest_command_map, entry->dest_function_map, entry->dest_jump_index);
    }
    fprintf(stream, "=============================================================================================================================\n");
}
void tactyk_debug__write_vmprograms(struct tactyk_asmvm__Context *ctx, FILE *stream) {

}

// should probably add a mechanism for determing what format(s) to use for showing data/xmmx register contents.
void tactyk_debug__print_context(struct tactyk_asmvm__Context *ctx) {
    tactyk_debug__write_context(ctx, stdout);
}

void tactyk_debug__print_mbind(struct tactyk_asmvm__Context *ctx) {
    tactyk_debug__write_mbind(ctx, stdout);
}


void tactyk_debug__print_vmstack(struct tactyk_asmvm__Context *ctx) {
    tactyk_debug__write_vmstack(ctx->stack, stdout);
}
void tactyk_debug__print_vmprograms(struct tactyk_asmvm__Context *ctx) {
    tactyk_debug__write_vmprograms(ctx, stdout);
}

void tactyk_debug__break(struct tactyk_asmvm__Context *ctx) {
    printf("EXECUTION PAUSED.  Press 'Enter' to proceed.  Enter 'q' to quit.\n");

    char ch = getchar( );
    if (ch == 'q') {
        printf("Farewell!\n");
        exit(0);
    }
}
