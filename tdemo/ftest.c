#include <stdint.h>

#include "ftest.h"
#include "tactyk_pl.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

/*
    floating point math in-dev tests.
*/

char *floattest_src = {
    R"""(
        struct fmt
            4 v0a
            4 v0b
            4 v1a
            4 v1b
            8 v2
            8 v3
            8 v4
            8 v5
            8 v6
            8 v7
            8 v8
            8 v9
            64 padding
        mem fdat fmt 1024

        const fc 5432

        MAIN:
            bind addr1 fdat
            vec-in xmm1 addr1 fmt.v0a
            vec-in xmm2 addr1 fmt.v1a
            vec-in xmm3 addr1 fmt.v2
            vec-in xmm4 addr1 fmt.v3
            vec-add PS xmm1 xmm2
            vec-div SD xmm3 xmm4
            vec-out ftoi e xmm3
            vec-out f xmm1

            assign a 123
            vec-in itof xmm5 a
            vec-mul SD xmm5 xmm4
            vec-out ftoi b xmm5

            vec-out addr1 fmt.v5 xmm5
            vec-out ftoi addr1 fmt.v6 xmm3

            vec-in itof xmm6 4
            load qword c addr1 fmt.v4
            # vec-in xmm7 addr1 fmt.v4
            vec-in xmm7 c
            # vec-in float xmm7 1.75
            vec-mul xmm6 xmm7
            vec-out ftoi d xmm6

            vec-in itof xmm8 addr1 fmt.v7
            # vec-out ftoi e xmm8
            vec-out addr1 fmt.v8 xmm8
            assign a 5432
            # vec-in xmm9 5432.001

            if xmm8 > fc PRINTSOMETHING

            exit

            PRINTSOMETHING:
            assign a 10
            ccall printchar
            assign a 65
            ccall printchar
            ccall printchar
            add a 1
            ccall printchar
            ccall printchar
            add a 1
            ccall printchar
            ccall printchar
            assign a 10
            ccall printchar
            ccall printchar
            exit

            # load qword c addr1 fib_args.iterations
    )"""
};

void run_float_test(struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *ctx) {

    struct tactyk_asmvm__Program *prg = tactyk_pl__load(emitctx, floattest_src);


    struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_dblock__get(prg->memory_layout_hl, "fdat");

    //struct tactyk_asmvm__memblock_lowlevel *lb = mblk->memblock;

    //struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_table__get_strkey(prg->symbols.memtbl, "args");
    float *fdata = (float*) mblk->data;
    double *ddata = (double*) mblk->data;
    int64_t *idata = (int64_t*) mblk->data;

    //  1.7 + 11.0 -> 32-bit packed floats -> 12.0 in the lower 32-bits of register rF
    //  5.2 + 101.0 -> 32-bit packed floats -> 106.1997 in the upper 32-bits of register rF
    //  400.0 / 3.0 -> just a pair of 64-bit floats -> 133.333 in register rE
    fdata[0] = 1.7;
    fdata[1] = 5.2;

    fdata[2] = 11.0;
    fdata[3] = 101.0;

    ddata[2] = 400.0;
    ddata[3] = 3.0;
    ddata[4] = 1.75;

    idata[7] = 5432;

    tactyk_asmvm__invoke(ctx, prg, "MAIN");
    tactyk_asmvm__print_context(ctx);

    // cast to (double*) and print
    printf("rE: %f\n", *((double*)&ctx->regbank_A.rE));

    // cast to (float*), offset, and print
    printf("rF[low]: %f\n", *((float*)&ctx->regbank_A.rF));
    printf("rF[high]: %f\n",  ((float*)&ctx->regbank_A.rF)[1] );

    printf("d[5] = %f\n", ddata[5]);
    printf("(int) d[6] = %jd\n", idata[6]);
    printf("(float) d[8] = %f\n", ddata[8]);

}
