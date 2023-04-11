
#include <stdio.h>
#include <stdint.h>

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_debug.h"

void tactyk_debug__configure_api(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-ctx", tactyk_debug__print_context);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-mbind", tactyk_debug__print_mbind);
    tactyk_emit__add_tactyk_apifunc(emitctx, "break", tactyk_debug__break);
}

// should probably add a mechanism for determing what format(s) to use for showing data/xmmx register contents.
void tactyk_debug__print_context(struct tactyk_asmvm__Context *ctx) {
    printf("===== TACTYK CONTEXT ===================================================================================\n");
    printf("| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20f | %-20f |\n", ctx, ctx->regbank_A.xa.f64[1], ctx->regbank_A.xa.f64[0]);
    printf("| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20f | %-20f |\n", ctx->microcontext_stack, ctx->regbank_A.xb.f64[1], ctx->regbank_A.xb.f64[0]);
    printf("| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20f | %-20f |\n", ctx->regbank_A.xc.f64[1], ctx->regbank_A.xc.f64[0]);
    printf("| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20f | %-20f |\n", ctx->regbank_A.xd.f64[1], ctx->regbank_A.xd.f64[0]);
    printf("| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20f | %-20f |\n", ctx->regbank_A.xe.f64[1], ctx->regbank_A.xe.f64[0]);
    printf("| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20f | %-20f |\n", ctx->regbank_A.rLWCSI, ctx->regbank_A.xf.f64[1], ctx->regbank_A.xf.f64[0]);
    printf("| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20f | %-20f |\n", ctx->regbank_A.rMCSI, ctx->regbank_A.xg.f64[1], ctx->regbank_A.xg.f64[0]);
    printf("| rPROG    (r12) | %20p | xH (xmm7)      | %-20f | %-20f |\n", ctx->regbank_A.rPROG, ctx->regbank_A.xh.f64[1], ctx->regbank_A.xh.f64[0]);
    printf("| rADDR1   (r14) | %20p | xI (xmm8)      | %-20f | %-20f |\n", ctx->regbank_A.rADDR1, ctx->regbank_A.xi.f64[1], ctx->regbank_A.xi.f64[0]);
    printf("| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20f | %-20f |\n", ctx->regbank_A.rADDR2, ctx->regbank_A.xj.f64[1], ctx->regbank_A.xj.f64[0]);
    printf("| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20f | %-20f |\n", ctx->regbank_A.rADDR3, ctx->regbank_A.xk.f64[1], ctx->regbank_A.xk.f64[0]);
    printf("| rADDR4   (r10) | %20p | xL (xmm11)     | %-20f | %-20f |\n", ctx->regbank_A.rADDR4, ctx->regbank_A.xl.f64[1], ctx->regbank_A.xl.f64[0]);
    printf("| rA       (rdi) | %20jd | xM (xmm12)     | %-20f | %-20f |\n", ctx->regbank_A.rA, ctx->regbank_A.xm.f64[1], ctx->regbank_A.xm.f64[0]);
    printf("| rB       (rsi) | %20jd | xN (xmm13)     | %-20f | %-20f |\n", ctx->regbank_A.rB, ctx->regbank_A.xn.f64[1], ctx->regbank_A.xn.f64[0]);
    printf("| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20f | %-20f |\n", ctx->regbank_A.rC, ctx->regbank_A.xo.f64[1], ctx->regbank_A.xo.f64[0]);
    printf("| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20f | %-20f |\n", ctx->regbank_A.rD, ctx->regbank_A.xp.f64[1], ctx->regbank_A.xp.f64[0]);
    printf("| rE       (r8)  | %20jd | ---            |                  --- |                  --- |\n", ctx->regbank_A.rE);
    printf("| rF       (r8)  | %20jd | ---            |                  --- |                  --- |\n", ctx->regbank_A.rF);
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
    printf("=== BOUND MEMORY ========================\n");
}

void tactyk_debug__break(struct tactyk_asmvm__Context *ctx) {
    printf("EXECUTION PAUSED.  Press 'Enter' to proceed.  Enter 'q' to quit.\n");

    char ch = getchar( );
    if (ch == 'q') {
        printf("Farewell!\n");
        exit(0);
    }
}
