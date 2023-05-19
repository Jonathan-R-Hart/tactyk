
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_debug.h"

uint64_t tactyk_debug__xmm_display_mode = TACTYK_DEBUG__XMM_DISPLAYMODE__FLOAT64;

void tactyk_debug__configure_api(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-dispmode", tactyk_debug__set_display_mode);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-ctx", tactyk_debug__print_context);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-mbind", tactyk_debug__print_mbind);
    tactyk_emit__add_tactyk_apifunc(emitctx, "dump-stack", tactyk_debug__print_vmstack);
    tactyk_emit__add_tactyk_apifunc(emitctx, "break", tactyk_debug__break);
}

void tactyk_debug__write_context(struct tactyk_asmvm__Context *ctx, FILE *stream) {
    switch(tactyk_debug__xmm_display_mode) {
        case TACTYK_DEBUG__XMM_DISPLAYMODE__FLOAT32: {
            fprintf(stream, "===== TACTYK CONTEXT =================================================================================================================================\n");
            fprintf(stream, "| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx, ctx->reg.xA.f32[3], ctx->reg.xA.f32[2], ctx->reg.xA.f32[1], ctx->reg.xA.f32[0]);
            fprintf(stream, "| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->microcontext_stack, ctx->reg.xB.f32[3], ctx->reg.xB.f32[2], ctx->reg.xB.f32[1], ctx->reg.xB.f32[0]);
            fprintf(stream, "| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.xC.f32[3], ctx->reg.xC.f32[2], ctx->reg.xC.f32[1], ctx->reg.xC.f32[0]);
            fprintf(stream, "| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.xD.f32[3], ctx->reg.xD.f32[2], ctx->reg.xD.f32[1], ctx->reg.xD.f32[0]);
            fprintf(stream, "| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.xE.f32[3], ctx->reg.xE.f32[2], ctx->reg.xE.f32[1], ctx->reg.xE.f32[0]);
            fprintf(stream, "| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rLWCSI, ctx->reg.xF.f32[3], ctx->reg.xF.f32[2], ctx->reg.xF.f32[1], ctx->reg.xF.f32[0]);
            fprintf(stream, "| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rMCSI, ctx->reg.xG.f32[3], ctx->reg.xG.f32[2], ctx->reg.xG.f32[1], ctx->reg.xG.f32[0]);
            fprintf(stream, "| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rTEMPS, ctx->reg.xH.f32[3], ctx->reg.xH.f32[2], ctx->reg.xH.f32[1], ctx->reg.xH.f32[0]);
            fprintf(stream, "| rADDR1   (r14) | %20p | xI (xmm8)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rADDR1, ctx->reg.xI.f32[3], ctx->reg.xI.f32[2], ctx->reg.xI.f32[1], ctx->reg.xI.f32[0]);
            fprintf(stream, "| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rADDR2, ctx->reg.xJ.f32[3], ctx->reg.xJ.f32[2], ctx->reg.xJ.f32[1], ctx->reg.xJ.f32[0]);
            fprintf(stream, "| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rADDR3, ctx->reg.xK.f32[3], ctx->reg.xK.f32[2], ctx->reg.xK.f32[1], ctx->reg.xK.f32[0]);
            fprintf(stream, "| rADDR4   (r10) | %20p | xL (xmm11)     | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rADDR4, ctx->reg.xL.f32[3], ctx->reg.xM.f32[2], ctx->reg.xL.f32[1], ctx->reg.xL.f32[0]);
            fprintf(stream, "| rA       (rdi) | %20jd | xM (xmm12)     | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rA, ctx->reg.xM.f32[3], ctx->reg.xM.f32[2], ctx->reg.xM.f32[1], ctx->reg.xM.f32[0]);
            fprintf(stream, "| rB       (rsi) | %20jd | xN (xmm13)     | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rB, ctx->reg.xN.f32[3], ctx->reg.xN.f32[2], ctx->reg.xN.f32[1], ctx->reg.xN.f32[0]);
            fprintf(stream, "| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rC, ctx->reg.xTEMPA.f32[3], ctx->reg.xTEMPA.f32[2], ctx->reg.xTEMPA.f32[1], ctx->reg.xTEMPA.f32[0]);
            fprintf(stream, "| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20.14g | %-20.14g | %-20.14g | %-20.14g |\n", ctx->reg.rD, ctx->reg.xTEMPB.f32[3], ctx->reg.xTEMPB.f32[2], ctx->reg.xTEMPB.f32[1], ctx->reg.xTEMPB.f32[0]);
            fprintf(stream, "| rE       (r8)  | %20jd | ---            |                  --- |                  --- |                  --- |                  --- |\n", ctx->reg.rE);
            fprintf(stream, "| rF       (r9)  | %20jd | ---            |                  --- |                  --- |                  --- |                  --- |\n", ctx->reg.rF);
            fprintf(stream, "======================================================================================================================================================\n");
            break;
        }
        case TACTYK_DEBUG__XMM_DISPLAYMODE__FLOAT64: {
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
            break;
        }
        case TACTYK_DEBUG__XMM_DISPLAYMODE__INT32: {
            fprintf(stream, "===== TACTYK CONTEXT =================================================================================================================================\n");
            fprintf(stream, "| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20d | %-20d | %-20d | %-20d |\n", ctx, ctx->reg.xA.i32[3], ctx->reg.xA.i32[2], ctx->reg.xA.i32[1], ctx->reg.xA.i32[0]);
            fprintf(stream, "| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->microcontext_stack, ctx->reg.xB.i32[3], ctx->reg.xB.i32[2], ctx->reg.xB.i32[1], ctx->reg.xB.i32[0]);
            fprintf(stream, "| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.xC.i32[3], ctx->reg.xC.i32[2], ctx->reg.xC.i32[1], ctx->reg.xC.i32[0]);
            fprintf(stream, "| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.xD.i32[3], ctx->reg.xD.i32[2], ctx->reg.xD.i32[1], ctx->reg.xD.i32[0]);
            fprintf(stream, "| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.xE.i32[3], ctx->reg.xE.i32[2], ctx->reg.xE.i32[1], ctx->reg.xE.i32[0]);
            fprintf(stream, "| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rLWCSI, ctx->reg.xF.i32[3], ctx->reg.xF.i32[2], ctx->reg.xF.i32[1], ctx->reg.xF.i32[0]);
            fprintf(stream, "| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rMCSI, ctx->reg.xG.i32[3], ctx->reg.xG.i32[2], ctx->reg.xG.i32[1], ctx->reg.xG.i32[0]);
            fprintf(stream, "| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rTEMPS, ctx->reg.xH.i32[3], ctx->reg.xH.i32[2], ctx->reg.xH.i32[1], ctx->reg.xH.i32[0]);
            fprintf(stream, "| rADDR1   (r14) | %20p | xI (xmm8)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rADDR1, ctx->reg.xI.i32[3], ctx->reg.xI.i32[2], ctx->reg.xI.i32[1], ctx->reg.xI.i32[0]);
            fprintf(stream, "| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rADDR2, ctx->reg.xJ.i32[3], ctx->reg.xJ.i32[2], ctx->reg.xJ.i32[1], ctx->reg.xJ.i32[0]);
            fprintf(stream, "| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rADDR3, ctx->reg.xK.i32[3], ctx->reg.xK.i32[2], ctx->reg.xK.i32[1], ctx->reg.xK.i32[0]);
            fprintf(stream, "| rADDR4   (r10) | %20p | xL (xmm11)     | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rADDR4, ctx->reg.xL.i32[3], ctx->reg.xM.i32[2], ctx->reg.xL.i32[1], ctx->reg.xL.i32[0]);
            fprintf(stream, "| rA       (rdi) | %20jd | xM (xmm12)     | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rA, ctx->reg.xM.i32[3], ctx->reg.xM.i32[2], ctx->reg.xM.i32[1], ctx->reg.xM.i32[0]);
            fprintf(stream, "| rB       (rsi) | %20jd | xN (xmm13)     | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rB, ctx->reg.xN.i32[3], ctx->reg.xN.i32[2], ctx->reg.xN.i32[1], ctx->reg.xN.i32[0]);
            fprintf(stream, "| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rC, ctx->reg.xTEMPA.i32[3], ctx->reg.xTEMPA.i32[2], ctx->reg.xTEMPA.i32[1], ctx->reg.xTEMPA.i32[0]);
            fprintf(stream, "| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20d | %-20d | %-20d | %-20d |\n", ctx->reg.rD, ctx->reg.xTEMPB.i32[3], ctx->reg.xTEMPB.i32[2], ctx->reg.xTEMPB.i32[1], ctx->reg.xTEMPB.i32[0]);
            fprintf(stream, "| rE       (r8)  | %20jd | ---            |                  --- |                  --- |                  --- |                  --- |\n", ctx->reg.rE);
            fprintf(stream, "| rF       (r9)  | %20jd | ---            |                  --- |                  --- |                  --- |                  --- |\n", ctx->reg.rF);
            fprintf(stream, "======================================================================================================================================================\n");
            break;
        }
        default:
        case TACTYK_DEBUG__XMM_DISPLAYMODE__INT64: {
            fprintf(stream, "===== TACTYK CONTEXT ===================================================================================\n");
            fprintf(stream, "| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20jd | %-20jd |\n", ctx, ctx->reg.xA.i64[1], ctx->reg.xA.i64[0]);
            fprintf(stream, "| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20jd | %-20jd |\n", ctx->microcontext_stack, ctx->reg.xB.i64[1], ctx->reg.xB.i64[0]);
            fprintf(stream, "| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20jd | %-20jd |\n", ctx->reg.xC.i64[1], ctx->reg.xC.i64[0]);
            fprintf(stream, "| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20jd | %-20jd |\n", ctx->reg.xD.i64[1], ctx->reg.xD.i64[0]);
            fprintf(stream, "| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20jd | %-20jd |\n", ctx->reg.xE.i64[1], ctx->reg.xE.i64[0]);
            fprintf(stream, "| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20jd | %-20jd |\n", ctx->reg.rLWCSI, ctx->reg.xF.i64[1], ctx->reg.xF.i64[0]);
            fprintf(stream, "| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20jd | %-20jd |\n", ctx->reg.rMCSI, ctx->reg.xG.i64[1], ctx->reg.xG.i64[0]);
            fprintf(stream, "| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20jd | %-20jd |\n", ctx->reg.rTEMPS, ctx->reg.xH.i64[1], ctx->reg.xH.i64[0]);
            fprintf(stream, "| rADDR1   (r14) | %20p | xI (xmm8)      | %-20jd | %-20jd |\n", ctx->reg.rADDR1, ctx->reg.xI.i64[1], ctx->reg.xI.i64[0]);
            fprintf(stream, "| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20jd | %-20jd |\n", ctx->reg.rADDR2, ctx->reg.xJ.i64[1], ctx->reg.xJ.i64[0]);
            fprintf(stream, "| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20jd | %-20jd |\n", ctx->reg.rADDR3, ctx->reg.xK.i64[1], ctx->reg.xK.i64[0]);
            fprintf(stream, "| rADDR4   (r10) | %20p | xL (xmm11)     | %-20jd | %-20jd |\n", ctx->reg.rADDR4, ctx->reg.xL.i64[1], ctx->reg.xL.i64[0]);
            fprintf(stream, "| rA       (rdi) | %20jd | xM (xmm12)     | %-20jd | %-20jd |\n", ctx->reg.rA, ctx->reg.xM.i64[1], ctx->reg.xM.i64[0]);
            fprintf(stream, "| rB       (rsi) | %20jd | xN (xmm13)     | %-20jd | %-20jd |\n", ctx->reg.rB, ctx->reg.xN.i64[1], ctx->reg.xN.i64[0]);
            fprintf(stream, "| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20jd | %-20jd |\n", ctx->reg.rC, ctx->reg.xTEMPA.i64[1], ctx->reg.xTEMPA.i64[0]);
            fprintf(stream, "| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20jd | %-20jd |\n", ctx->reg.rD, ctx->reg.xTEMPB.i64[1], ctx->reg.xTEMPB.i64[0]);
            fprintf(stream, "| rE       (r8)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rE);
            fprintf(stream, "| rF       (r9)  | %20jd | ---            |                  --- |                  --- |\n", ctx->reg.rF);
            fprintf(stream, "========================================================================================================\n");
            break;
        }
        case TACTYK_DEBUG__XMM_DISPLAYMODE__STRING: {
            char xmmbuf[320];
            memset(xmmbuf, 0, sizeof(xmmbuf));
            memcpy(&xmmbuf[0 * 20], (void*)&ctx->reg.xA, 16);
            memcpy(&xmmbuf[1 * 20], (void*)&ctx->reg.xB, 16);
            memcpy(&xmmbuf[2 * 20], (void*)&ctx->reg.xC, 16);
            memcpy(&xmmbuf[3 * 20], (void*)&ctx->reg.xD, 16);
            memcpy(&xmmbuf[4 * 20], (void*)&ctx->reg.xE, 16);
            memcpy(&xmmbuf[5 * 20], (void*)&ctx->reg.xF, 16);
            memcpy(&xmmbuf[6 * 20], (void*)&ctx->reg.xG, 16);
            memcpy(&xmmbuf[7 * 20], (void*)&ctx->reg.xH, 16);
            memcpy(&xmmbuf[8 * 20], (void*)&ctx->reg.xI, 16);
            memcpy(&xmmbuf[9 * 20], (void*)&ctx->reg.xJ, 16);
            memcpy(&xmmbuf[10 * 20], (void*)&ctx->reg.xK, 16);
            memcpy(&xmmbuf[11 * 20], (void*)&ctx->reg.xL, 16);
            memcpy(&xmmbuf[12 * 20], (void*)&ctx->reg.xM, 16);
            memcpy(&xmmbuf[13 * 20], (void*)&ctx->reg.xN, 16);
            memcpy(&xmmbuf[14 * 20], (void*)&ctx->reg.xTEMPA, 16);
            memcpy(&xmmbuf[15 * 20], (void*)&ctx->reg.xTEMPB, 16);
            fprintf(stream, "===== TACTYK CONTEXT ================================================== - - -\n");
            fprintf(stream, "| CTX-seg  (fs:) | %20p | xA (xmm0)      | %-20s\n", ctx, &xmmbuf[0 * 20]);
            fprintf(stream, "| MCTX-seg (gs:) | %20p | xB (xmm1)      | %-20s\n", ctx->microcontext_stack, &xmmbuf[1 * 20]);
            fprintf(stream, "| rTEMPA   (rax) |                  --- | xC (xmm2)      | %-20s\n", &xmmbuf[2 * 20]);
            fprintf(stream, "| rTEMPC   (rcx) |                  --- | xD (xmm3)      | %-20s\n", &xmmbuf[3 * 20]);
            fprintf(stream, "| rTEMPD   (rdx) |                  --- | xE (xmm4)      | %-20s\n", &xmmbuf[4 * 20]);
            fprintf(stream, "| rLWCSI   (rbp) | %20jd | xF (xmm5)      | %-20s\n", ctx->reg.rLWCSI, &xmmbuf[5 * 20]);
            fprintf(stream, "| rMSCI    (rsp) | %20jd | xG (xmm6)      | %-20s\n", ctx->reg.rMCSI, &xmmbuf[6 * 20]);
            fprintf(stream, "| rTEMPS   (r12) | %20jd | xH (xmm7)      | %-20s\n", ctx->reg.rTEMPS, &xmmbuf[7 * 20]);
            fprintf(stream, "| rADDR1   (r14) | %20p | xI (xmm8)      | %-20s\n", ctx->reg.rADDR1, &xmmbuf[8 * 20]);
            fprintf(stream, "| rADDR2   (r15) | %20p | xJ (xmm9)      | %-20s\n", ctx->reg.rADDR2, &xmmbuf[9 * 20]);
            fprintf(stream, "| rADDR3   (rbx) | %20p | xK (xmm10)     | %-20s\n", ctx->reg.rADDR3, &xmmbuf[10 * 20]);
            fprintf(stream, "| rADDR4   (r10) | %20p | xL (xmm11)     | %-20s\n", ctx->reg.rADDR4, &xmmbuf[11 * 20]);
            fprintf(stream, "| rA       (rdi) | %20jd | xM (xmm12)     | %-20s\n", ctx->reg.rA, &xmmbuf[12 * 20]);
            fprintf(stream, "| rB       (rsi) | %20jd | xN (xmm13)     | %-20s\n", ctx->reg.rB, &xmmbuf[13 * 20]);
            fprintf(stream, "| rC       (r11) | %20jd | xTEMPA (xmm14) | %-20s\n", ctx->reg.rC, &xmmbuf[14 * 20]);
            fprintf(stream, "| rD       (r13) | %20jd | xTEMPB (xmm15) | %-20s\n", ctx->reg.rD, &xmmbuf[15 * 20]);
            fprintf(stream, "| rE       (r8)  | %20jd | ---            |\n", ctx->reg.rE);
            fprintf(stream, "| rF       (r9)  | %20jd | ---            |\n", ctx->reg.rF);
            fprintf(stream, "======================================================================= - - -\n");
            break;
        }
    }
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
    fprintf(stream, "===== TACTYK VM STACK ===========================================================================================================================================\n");
    char *lockstate;
    if (stack->stack_lock) {
        lockstate = "LOCKED";
    }
    else {
        lockstate = "UNLOCKED";
    }
    fprintf(stream, "| state: %-8s |                                                                                                                                             |\n", lockstate);
    fprintf(stream, "|---------------------------------------------------------------------------------------------------------------------------------------------------------------|\n");
    fprintf(stream, "|   pos |    src-commands |   return-target |     src-maxiptr |  lwc-floor | mctx-floor |   dest-commands |  dest-functions |     jump-target |    dest-maxiptr |\n");
    fprintf(stream, "|---------------------------------------------------------------------------------------------------------------------------------------------------------------|\n");
    for (int64_t pos = 0; pos <= stack->stack_position; pos += 1) {
        struct tactyk_asmvm__vm_stack_entry *entry = &stack->entries[pos];
        fprintf(
            stream, "| %5ju | %15p | %15u | %15u | %10u | %10u | %15p | %15p | %15u | %15u |\n",
            pos, entry->source_command_map, entry->source_return_index, entry->source_max_iptr, entry->source_lwcallstack_floor, entry->source_mctxstack_floor,
            entry->dest_command_map, entry->dest_function_map, entry->dest_jump_index, entry->dest_max_iptr);
    }
    fprintf(stream, "=================================================================================================================================================================\n");
}
void tactyk_debug__write_vmprograms(struct tactyk_asmvm__Context *ctx, FILE *stream) {

}

void tactyk_debug__set_display_mode(struct tactyk_asmvm__Context *ctx) {
    tactyk_debug__xmm_display_mode = ctx->reg.rA;
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
