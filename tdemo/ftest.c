#include <stdint.h>

#include "ftest.h"

#include "tactyk_debug.h"
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
            assign xb 77
            assign d 7
            div xb d
            store float addr1 0 xb
            # add xb 10.3
            # assign c xb

            if xb == 11 PRINTSOMETHING

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
    /*
    fdata[0] = 1.7;
    fdata[1] = 5.2;

    fdata[2] = 11.0;
    fdata[3] = 101.0;

    ddata[2] = 400.0;
    ddata[3] = 3.0;
    ddata[4] = 1.75;

    idata[7] = 5432;
    */
    tactyk_asmvm__invoke(ctx, prg, "MAIN");
    tactyk_debug__print_context(ctx);

    printf("AS-INT:\n");
    for (int64_t i = 0; i < 8; i++) {
        printf("  %jd: %jd\n", i, idata[i]);
    }
    printf("AS-FLOAT32:\n");
    for (int64_t i = 0; i < 16; i += 2) {
        printf("  %jd: %f\t%f\n", i, fdata[i], fdata[i+1]);
    }
    printf("AS-FLOAT-64:\n");
    for (int64_t i = 0; i < 8; i++) {
        printf("  %jd: %f\n", i, ddata[i]);
    }


}
