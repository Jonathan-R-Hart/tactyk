
#include <stdint.h>

#include "esvctest.h"

#include "tactyk_debug.h"
#include "tactyk_pl.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

/*
    floating point math in-dev tests.
*/

char *squareit_src = {
    R"""(
        SQ:
            mul a a
            exit
    )"""
};

char *esvctest_src = {
    R"""(
        use_vconstants

        text txt
            l SQUAREIT.

        MAIN:
            bind addr1 txt

            tcall dump-ctx
            tcall emit-new
            stash a1

            # assign a 123
            assign a 2
            assign b 8
            tcall emit-label
            assign b a
            stash b1

            assign a .mul
            tcall emit-cmd
            tcall dump-ctx

            assign a .a
            assign b .a
            tcall emit-token-2
            tcall emit-cmd-end

            assign a .exit
            tcall emit-cmd
            tcall emit-cmd-end

            tcall emit-build
            unstash b1
            tcall dump-ctx

            # planned cross-script call
            # tactyk programs which are not combined are only soft-linked through a virtual machine, so extra parameters are needed to set up function calls between scripts.
            # These extra parameters take up valuable register space, so instead of using a single-line invocation, the call is set up by pushing the required references
            # onto a global (VM) stack, then setting up parameters into the function to call, then jumping to the function at the top of said stack.
            # unstash a1b1
            # tvm-prepcall a b
            # assign a 5
            # tvm-call



            exit
    )"""
};

struct tactyk_asmvm__Program* run_esvc_test(struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *ctx) {

    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__load(plctx, esvctest_src);
    struct tactyk_asmvm__Program *prg = tactyk_pl__build(plctx);
    tactyk_asmvm__add_program(ctx, prg);
    tactyk_asmvm__invoke(ctx, prg, "MAIN");
    return prg;
}
