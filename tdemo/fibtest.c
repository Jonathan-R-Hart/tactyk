//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include "fibtest.h"

char *fibtest_src = {
    R"""(
        struct fib_args
            8 iterations

        mem args fib_args 1

        var iterations
            get
                load qword ? addr1 fib_args.iterations

        use_vconstants

        MAIN:
            assign a 0
            assign b 1
            bind addr1 args
            get c iterations
            # load qword c addr1 fib_args.iterations
            FIBLOOP:
            swap a b
            add a b
            dec c
            if c > 0 FIBLOOP
            tcall dump-ctx
            exit
        DIAG:
            cpuclocks
            exit

            # load qword c addr1 fib_args.iterations
    )"""
};

void run_fib_native(uint64_t amount) {
    uint64_t a = 0;
    uint64_t b = 1;
    for (uint64_t i = 0; i < amount; i++) {
        uint64_t tmp = a;
        a = b;
        b = tmp;
        a += b;
    }
    printf("fib-native result: %lu\n", a);
}

struct tactyk_asmvm__Program* run_fib_test(struct tactyk_emit__Context *emitctx, uint64_t amount, struct tactyk_asmvm__Context *ctx) {
    uint64_t c1 = 0;
    uint64_t c2 = 0;


    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__load(plctx, fibtest_src);
    struct tactyk_asmvm__Program *prg = tactyk_pl__build(plctx);
    tactyk_asmvm__add_program(ctx, prg);
    struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_dblock__get(prg->memory_layout_hl, "args");
    uint64_t *data = (uint64_t*) mblk->data;

    data[0] = amount;

    tactyk_asmvm__invoke(ctx, prg, "DIAG");
    c1 = ctx->diagnostic_data[0];
    run_fib_native(amount);
    tactyk_asmvm__invoke(ctx, prg, "DIAG");
    c2 = ctx->diagnostic_data[0];
    printf("fib-native cycle count: %lu\n\n", c2-c1);

    tactyk_asmvm__invoke(ctx, prg, "DIAG");
    c1 = ctx->diagnostic_data[0];
    tactyk_asmvm__invoke(ctx, prg, "MAIN");
    //tactyk_asmvm__print_context(ctx);
    tactyk_asmvm__invoke(ctx, prg, "DIAG");
    //
    c2 = ctx->diagnostic_data[0];
    printf("fib-tactyk result: %lu\n", ctx->reg.rA);
    printf("fib-tactyk cycle count: %lu\n\n", c2-c1);
    return prg;
}
