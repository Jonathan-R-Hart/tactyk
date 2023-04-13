
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

// should probably add a mechanism for determing what format(s) to use for showing data/xmmx register contents.
void tactyk_debug__print_context(struct tactyk_asmvm__Context *ctx) {
    printf("===== TACTYK CONTEXT ===================================================================================\n");
    printf("| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20.14g | %-20.14g |\n", ctx, ctx->reg.xa.f64[1], ctx->reg.xa.f64[0]);
    printf("| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20.14g | %-20.14g |\n", ctx->microcontext_stack, ctx->reg.xb.f64[1], ctx->reg.xb.f64[0]);
    printf("| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20.14g | %-20.14g |\n", ctx->reg.xc.f64[1], ctx->reg.xc.f64[0]);
    printf("| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20.14g | %-20.14g |\n", ctx->reg.xd.f64[1], ctx->reg.xd.f64[0]);
    printf("| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20.14g | %-20.14g |\n", ctx->reg.xe.f64[1], ctx->reg.xe.f64[0]);
    printf("| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20.14g | %-20.14g |\n", ctx->reg.rLWCSI, ctx->reg.xf.f64[1], ctx->reg.xf.f64[0]);
    printf("| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20.14g | %-20.14g |\n", ctx->reg.rMCSI, ctx->reg.xg.f64[1], ctx->reg.xg.f64[0]);
    printf("| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20.14g | %-20.14g |\n", ctx->reg.rTEMPS, ctx->reg.xh.f64[1], ctx->reg.xh.f64[0]);
    printf("| rADDR1   (r14) | %20p | xI (xmm8)      | %-20.14g | %-20.14g |\n", ctx->reg.rADDR1, ctx->reg.xi.f64[1], ctx->reg.xi.f64[0]);
    printf("| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20.14g | %-20.14g |\n", ctx->reg.rADDR2, ctx->reg.xj.f64[1], ctx->reg.xj.f64[0]);
    printf("| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20.14g | %-20.14g |\n", ctx->reg.rADDR3, ctx->reg.xk.f64[1], ctx->reg.xk.f64[0]);
    printf("| rADDR4   (r10) | %20p | xL (xmm11)     | %-20.14g | %-20.14g |\n", ctx->reg.rADDR4, ctx->reg.xl.f64[1], ctx->reg.xl.f64[0]);
    printf("| rA       (rdi) | %20jd | xM (xmm12)     | %-20.14g | %-20.14g |\n", ctx->reg.rA, ctx->reg.xm.f64[1], ctx->reg.xm.f64[0]);
    printf("| rB       (rsi) | %20jd | xN (xmm13)     | %-20.14g | %-20.14g |\n", ctx->reg.rB, ctx->reg.xn.f64[1], ctx->reg.xn.f64[0]);
    printf("| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20.14g | %-20.14g |\n", ctx->reg.rC, ctx->reg.xTEMPA.f64[1], ctx->reg.xTEMPA.f64[0]);
    printf("| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20.14g | %-20.14g |\n", ctx->reg.rD, ctx->reg.xTEMPB.f64[1], ctx->reg.xTEMPB.f64[0]);
    printf("| rE       (r8)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rE);
    printf("| rF       (r9)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rF);
    printf("========================================================================================================\n");
}

void tactyk_debug__print_mbind(struct tactyk_asmvm__Context *ctx) {

    printf("===== BOUND MEMORY ======================\n");
    struct tactyk_asmvm__memblock_lowlevel *mbll = &ctx->active_memblocks[0];
    printf("| ADDR1          | %20p |\n", mbll->base_address);
    printf("| Element Bound  | %20d |\n", mbll->element_bound);
    printf("| Array Bound    | %20d |\n", mbll->array_bound);
    printf("| Memblock Index | %20d |\n", mbll->memblock_index);
    printf("| Type Code      | %20d |\n", mbll->type);
    printf("| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[1];
    printf("| ADDR2          | %20p |\n", mbll->base_address);
    printf("| Element Bound  | %20d |\n", mbll->element_bound);
    printf("| Array Bound    | %20d |\n", mbll->array_bound);
    printf("| Memblock Index | %20d |\n", mbll->memblock_index);
    printf("| Type Code      | %20d |\n", mbll->type);
    printf("| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[2];
    printf("| ADDR2          | %20p |\n", mbll->base_address);
    printf("| Element Bound  | %20d |\n", mbll->element_bound);
    printf("| Array Bound    | %20d |\n", mbll->array_bound);
    printf("| Memblock Index | %20d |\n", mbll->memblock_index);
    printf("| Type Code      | %20d |\n", mbll->type);
    printf("| -------------- | -------------------- |\n");
    mbll = &ctx->active_memblocks[3];
    printf("| ADDR2          | %20p |\n", mbll->base_address);
    printf("| Element Bound  | %20d |\n", mbll->element_bound);
    printf("| Array Bound    | %20d |\n", mbll->array_bound);
    printf("| Memblock Index | %20d |\n", mbll->memblock_index);
    printf("| Type Code      | %20d |\n", mbll->type);
    printf("=========================================\n");
}


void tactyk_debug__print_vmstack(struct tactyk_asmvm__Context *ctx) {
    printf("===== TACTYK VM STACK =======================================================================================================\n");
    char *lockstate;
    if (ctx->stack->stack_lock) {
        lockstate = "LOCKED";
    }
    else {
        lockstate = "UNLOCKED";
    }
    printf("| state: %-8s |                                                                                                         |\n", lockstate);
    printf("|---------------------------------------------------------------------------------------------------------------------------|\n");
    printf("|   pos |    src-commands |   return-target |  lwc-floor | mctx-floor |   dest-commands |  dest-functions |     jump-target |\n");
    printf("|---------------------------------------------------------------------------------------------------------------------------|\n");
    for (int64_t pos = 0; pos <= ctx->stack->stack_position; pos += 1) {
        struct tactyk_asmvm__vm_stack_entry *entry = &ctx->stack->entries[pos];
        printf("| %5ju | %15p | %15ju | %10u | %10u | %15p | %15p | %15ju |\n", pos, entry->source_command_map, entry->source_return_index, entry->source_lwcallstack_floor, entry->source_mctxstack_floor, entry->dest_command_map, entry->dest_function_map, entry->dest_jump_index);
    }
    printf("=============================================================================================================================\n");
}

void tactyk_debug__print_vmprograms(struct tactyk_asmvm__Context *ctx) {


}
void tactyk_debug__break(struct tactyk_asmvm__Context *ctx) {
    printf("EXECUTION PAUSED.  Press 'Enter' to proceed.  Enter 'q' to quit.\n");

    char ch = getchar( );
    if (ch == 'q') {
        printf("Farewell!\n");
        exit(0);
    }
}
