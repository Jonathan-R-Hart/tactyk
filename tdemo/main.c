//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/random.h>
#include <setjmp.h>

#include <stdint.h>

// #include <limits.h>
// #include <unistd.h>

#include <stdlib.h>
#include <string.h>

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_pl.h"
#include "tactyk_visa.h"
#include "tactyk_debug.h"
#include "tactyk_dblock.h"
#include "tactyk_emit_svc.h"

#include "aux_printit.h"
#include "aux_util.h"

#include "tactyk.h"
#include "qstest.h"
#include "fibtest.h"
#include "ftest.h"
#include "esvctest.h"

#include "aux_testlib.h"
#include "aux_sdl.h"

//#include "qstest.h"

//  Project Name:  TACTYKVM (should) Affix Captions To Your Kitten Virtual Machine (TACTYKVM)
//
//  TACTYKVM is an embeddable scripting engine.  Program execution is accomplished with a threaded code interpreter.
//  This is intended to be secure (minimal design and no direct interaction with the system) and performant
//      (scripting scheme selected to make the most of the compromises which come with code interpretation).
//
//  TACTYKVM also possibly introduces a new technique:  Sequenced pointer re-targetting.  Data, pointers, and indices are not encoded into instructions or passed as
//  conventional arguments (or on a stack).  Instead, parameters are passed by assigning pointers prior to running functions (setup can be anywhere between program
//  initalization and actual unction execution).  As the program runs, the pointers used to pass data between functions are re-targetted as part of control flow.
//
//  (In theory, one should be able to produce an optimized build simply by making the data structure a compile-time structure, concatenating/inlining the code for all
//  functions a program uses, and letting an optimizing compiler have at it -- at least if the optimizer is able to interpret explicit defined block of memory as only
//  an array of variables and not as an absolute that must be reproduced).

//  Potential optimizations (or more likely "premature" optimizations)
//
//    After VM buffer sizes are specified, Duplicate portions of the assembly and convert various indirections into hard-coded absolute addresses in machine code.
//
//    Prepare separate builds using hard-coded VM sizes (through macros and compiler options)
//      If compiler optimization does what I think it does, then this should give exactly the same as the above, and there are not actually a lot of sane combinations
//      of VM options that would need to be prepared (due to restricting buffer sizes to powers of two).
//


void testfunc(int64_t asdf) {
    printf("----------------------------------------------- test func %jd!\n", asdf);
}


#if !(defined TACTYK_SHELL_INTERFACE) && !(defined ASDF_FDSA)
int main() {
    tactyk_init();

    printf("%s\n", TACTYK_SE__DESCRIPTION);

    tactyk_visa__init("rsc");
    tactyk_visa__load_config_module("tactyk_core.visa");
    tactyk_visa__load_config_module("tactyk_core_typespec.visa");
    tactyk_visa__load_config_module("tactyk_core_ccall.visa");
    tactyk_visa__load_config_module("tactyk_core_memory.visa");
    tactyk_visa__load_config_module("tactyk_core_bulk_transfer.visa");
    tactyk_visa__load_config_module("tactyk_core_stash.visa");
    tactyk_visa__load_config_module("tactyk_core_tvmcall.visa");
    tactyk_visa__load_config_module("tactyk_core_xmm_fpmath.visa");
    tactyk_visa__load_config_module("tactyk_core_math.visa");
    
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();

    tactyk_visa__init_emit(emitctx);
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *ctx = tactyk_asmvm__new_context(vm);

    tactyk_debug__configure_api(emitctx);
    aux_configure(emitctx);
    aux_sdl__configure(emitctx);
    tactyk_emit_svc__configure(emitctx);
    //tactyk_dblock__persist_all(50);
    //struct tactyk_asmvm__Program *floatprg = run_float_test(emitctx, ctx);

    //run_fib_test(emitctx, 10000000000, ctx);
    //struct tactyk_asmvm__Program *fibprg = run_fib_test(emitctx, 2000000, ctx);
    //struct tactyk_asmvm__Program *fibprg = run_fib_test(emitctx, 25, ctx);
    //run_qsort_tests(emitctx, 10000000, 1, ctx);
    struct tactyk_asmvm__Program *qsprg = run_qsort_tests(emitctx, 10, 1, ctx);

    //tactyk_asmvm__invoke(ctx, fibprg, "MAIN");
    //tactyk_asmvm__invoke(ctx, qsprg, "MAIN");

    //struct tactyk_asmvm__Program *esvcprg = run_esvc_test(emitctx, ctx);
    return 0;
}
#endif // default [testing] interface

#ifdef TACTYK_SHELL_INTERFACE
int main(int argc, char *argv[], char *envp[]) {
    tactyk_init();

    printf("%s\n", TACTYK_SE__DESCRIPTION);

    char *visa_directory_name = "rsc";
    char *visa_fname = NULL;

    bool printctx = false;

    //scan the args once for a custom virtual-ISA specification.
    for (int64_t i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strcmp(arg, "-visa") == 0) {
            i += 1;
            visa_fname = argv[i];
        }
        else if (strcmp(arg, "-print-context") == 0) {
            printctx = true;
        }
    }

    tactyk_visa__init(visa_directory_name);
    if (visa_fname == NULL) {
        tactyk_visa__load_config_module("tactyk_core.visa");
        tactyk_visa__load_config_module("tactyk_core_typespec.visa");
        tactyk_visa__load_config_module("tactyk_core_ccall.visa");
        tactyk_visa__load_config_module("tactyk_core_memory.visa");
        tactyk_visa__load_config_module("tactyk_core_bulk_transfer.visa");
        tactyk_visa__load_config_module("tactyk_core_stash.visa");
        tactyk_visa__load_config_module("tactyk_core_tvmcall.visa");
        tactyk_visa__load_config_module("tactyk_core_xmm_fpmath.visa");
        tactyk_visa__load_config_module("tactyk_core_math.visa");
    }
    else {
        tactyk_visa__load_config_module(visa_fname);
    }
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();
                                        //tactyk_visa__init(fname);
    tactyk_visa__init_emit(emitctx);
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *ctx = tactyk_asmvm__new_context(vm);

    tactyk_debug__configure_api(emitctx);
    aux_configure(emitctx);
    aux_sdl__configure(emitctx);
    tactyk_emit_svc__configure(emitctx);
    aux_printit__configure(emitctx);
    aux_util__configure(emitctx);

    // intermediate storage for loaded data
    //  (tactyk uses the allocated data passed in as a backing data source during compilation, so it can't be freed
    //   immediately after passing it to tactyk_pl, but it is safe to free it immediately after compilation [before running]
    //   as all generated symbol tables use copied / managed memory).
    int64_t module_count = 0;
    char **module_src = calloc(argc, sizeof(void*));

    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__define_constants(plctx, ".VISA", emitctx->visa_token_constants);

    // re-scan the args and ingest source code files.
    for (int64_t i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] != '-') {
            FILE *f = fopen(arg, "r");
            fseek(f, 0, SEEK_END);
            int64_t len = ftell(f);
            char *pl_src = calloc(len+1, sizeof(char));
            fseek(f,0, SEEK_SET);
            fread(pl_src, len, 1, f);
            fclose(f);
            module_src[module_count] = pl_src;
            module_count += 1;
            tactyk_pl__load(plctx, pl_src);
        }
    }
    if (module_count > 0) {
        struct tactyk_asmvm__Program *prg = tactyk_pl__build(plctx);
        tactyk_asmvm__add_program(ctx, prg);

        tactyk_asmvm__invoke(ctx, prg, "MAIN");
        for (int64_t i = 0; i < module_count; i += 1) {
            free(module_src[i]);
        }
    }
    free(module_src);
    exit(0);
}
#endif // TACTYK_SHELL_INTERFACE















