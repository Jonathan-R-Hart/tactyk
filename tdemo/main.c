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

#include "tactyk_dblock.h"

#include "tactyk.h"
#include "qstest.h"
#include "fibtest.h"

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

// simple and safe random number generator
//          -- safe if safe means favoring a secure PRNG over something one has personally invented.
//          -- not so safe if safe is also supposed to mean 'unable to overwrite arbtrary amounts of arbitrarilly positioned memory.'
//          -- Some would say this is not a very portable approach, but the entire product line at issue got decertified for personal use years ago -- "free isn't freedom".
FILE *dev_urand;
void sys_rand(void *ptr, uint64_t nbytes) {
    uint64_t x = fread(ptr, nbytes, 1, dev_urand);
    x *= 2;
}

// overridable error handler
tactyk__error_handler error;
tactyk__error_handler warn;
static jmp_buf tactyk_err_jbuf;

void tactyk__default_warning_handler(char *msg, void *data) {
    if (data == NULL) {
        printf("WARNING -- %s\n", msg);
    }
    else {
        printf("WARNING -- %s: ", msg);
        tactyk_dblock__println(data);
    }
}


void tactyk__default_error_handler(char *msg, void *data) {
    if (data == NULL) {
        printf("ERROR -- %s\n", msg);
    }
    else {
        printf("ERROR -- %s: ", msg);
        tactyk_dblock__println(data);
    }
    longjmp(tactyk_err_jbuf, 1);
}

void testfunc(int64_t asdf) {
    printf("----------------------------------------------- test func %jd!\n", asdf);
}


#if !(defined TACTYK_SHELL_INTERFACE) && !(defined ASDF_FDSA)
int main() {
    error = tactyk__default_error_handler;
    warn = tactyk__default_warning_handler;

    printf("%s\n", TACTYK_SE__DESCRIPTION);

    // standalone erorr handling is to exit()
    // when invoked as library,t error handling should be to return NULL
    //      (the host application will have to override the error handler to get messages out)
    if (setjmp(tactyk_err_jbuf)) {
        printf("Error out.\n");
        exit(1);
    }

    tactyk_dblock__init();

    dev_urand = fopen("/dev/urandom", "r");

    tactyk_visa__init("rsc/tactyk_core.visa");
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();
    emitctx->visa_file_prefix = "rsc/";
    emitctx->rand = sys_rand;       // haven't yet decided to add a default PRNG, but when I do, it does need to be secure.

    tactyk_visa__init_emit(emitctx);
    tactyk_pl__init();
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *ctx = tactyk_asmvm__new_context(vm);

    aux_configure(emitctx);
    aux_sdl__configure(emitctx);

    //run_fib_test(emitctx, 10000000000, ctx);
    //run_fib_test(emitctx, 2000000, ctx);
    //run_qsort_tests(emitctx, 10000000, 1, ctx);
    run_qsort_tests(emitctx, 10, 1, ctx);

    //tactyk_visa_new__init("tactyk_core.visa");

    // is closing /dev/urandom needed?  (or, for that matter, anything read-only [if the process is going to terminate immediately afterward])
    // If so, then all calls to exit() should be replaced with something that exits properly.
    // If not so, then I'd prefer to get out of the cargo cult and delete whatever is superfluous.
    fclose(dev_urand);
    return 0;
}
#endif // default [testing] interface

#ifdef TACTYK_SHELL_INTERFACE
int main(int argc, char *argv[], char *envp[]) {
    error = tactyk__default_error_handler;
    warn = tactyk__default_warning_handler;

    printf("%s\n", TACTYK_SE__DESCRIPTION);

    // standalone erorr handling is to exit()
    // when invoked as library,t error handling should be to return NULL
    //      (the host application will have to override the error handler to get messages out)
    if (setjmp(tactyk_err_jbuf)) {
        printf("Error out.\n");
        exit(1);
    }
    dev_urand = fopen("/dev/urandom", "r");

    tactyk_dblock__init();

    char *visa_fname = "rsc/tactyk_core.visa";


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

    tactyk_visa__init(visa_fname);
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();
                                        //tactyk_visa__init(fname);
    emitctx->visa_file_prefix = "rsc/";
    emitctx->rand = sys_rand;       // haven't yet decided to add a default PRNG, but when I do, it does need to be secure.
    tactyk_visa__init_emit(emitctx);
    tactyk_pl__init();
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *ctx = tactyk_asmvm__new_context(vm);

    aux_configure(emitctx);
    aux_sdl__configure(emitctx);

    // re-scan the args and ingest source code files.
    for (int64_t i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] != '-') {
            FILE *f = fopen(arg, "r");
            fseek(f, 0, SEEK_END);
            int64_t len = ftell(f);
            char *pl_src = calloc(len, sizeof(char));
            fseek(f,0, SEEK_SET);
            fread(pl_src, len, 1, f);
            fclose(f);

            struct tactyk_asmvm__Program *prg = tactyk_pl__load(emitctx, pl_src);
            tactyk_asmvm__invoke(ctx, prg, "MAIN");
            //tactyk_asmvm__print_context(ctx);
            free(pl_src);
            exit(0);
        }
    }
    exit(0);
}
#endif // TACTYK_SHELL_INTERFACE















