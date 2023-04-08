#include <stdint.h>

#include "ftest.h"
#include "tactyk_pl.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

char *floattest_src = {
    R"""(
        struct fmt
            4 va
            4 vb
            4 vc
            4 vd
            8 ve
            8 vf
            64 padding
        mem fdat fmt 1024

        MAIN:
            bind addr1 fdat
            vec-in xmm1 addr1 fmt.va
            vec-in xmm2 addr1 fmt.vc
            vec-in xmm3 addr1 fmt.ve
            vec-in xmm4 addr1 fmt.vf
            vec-add PS xmm1 xmm2
            vec-div SD xmm3 xmm4
            vec-out e xmm3
            vec-out f xmm1

            assign a 123
            vec-in itof xmm5 a
            vec-mul SD xmm5 xmm4
            vec-out ftoi b xmm5

            vec-out addr1 40 xmm5
            vec-out ftoi addr1 48 xmm3
            exit

            # load qword c addr1 fib_args.iterations
    )"""
};

void run_float_test(struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *ctx) {

    struct tactyk_asmvm__Program *prg = tactyk_pl__load(emitctx, floattest_src);


    struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_dblock__get(prg->memory_layout_hl, "fdat");

    struct tactyk_asmvm__memblock_lowlevel *lb = mblk->memblock;

    //struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_table__get_strkey(prg->symbols.memtbl, "args");
    float *fdata = (float*) mblk->data;
    double *ddata = (double*) mblk->data;

    //  1.7 + 11.0 -> 32-bit packed floats -> 12.0 in the lower 32-bits of register rF
    //  5.2 + 101.0 -> 32-bit packed floats -> 106.1997 in the upper 32-bits of register rF
    //  400.0 / 3.0 -> just a pair of 64-bit floats -> 133.333 in register rE
    fdata[0] = 1.7;
    fdata[1] = 5.2;

    fdata[2] = 11.0;
    fdata[3] = 101.0;

    ddata[2] = 400.0;
    ddata[3] = 3.0;

    tactyk_asmvm__invoke(ctx, prg, "MAIN");
    tactyk_asmvm__print_context(ctx);

    // cast to (double*) and print
    printf("rE: %f\n", *((double*)&ctx->regbank_A.rE));

    // cast to (float*), offset, and print
    printf("rF[low]: %f\n", *((float*)&ctx->regbank_A.rF));
    printf("rF[high]: %f\n",  ((float*)&ctx->regbank_A.rF)[1] );

    printf("d[5] = %f\n", ddata[5]);
    printf("(int) d[6] = %jd\n", *((uint64_t*)&ddata[6]));

}
