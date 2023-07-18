#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tactyk.h"
#include "tactyk_report.h"
#include "tactyk_visa.h"
#include "tactyk_emit.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit_svc.h"
#include "tactyk_pl.h"
#include "tactyk_debug.h"
#include "aux_printit.h"
#include "aux_util.h"
#include "aux_sdl.h"

#include "tactyk_run_resource_pack.h"
#include "tactyk_run_platform_functions.h"

int main(int argc, char *argv[], char *envp[]) {
    tactyk_init();

    printf("%s\n", TACTYK_SE__DESCRIPTION);

    char *visa_directory_name = "rsc";
    char *visa_fname = NULL;

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
        tactyk_visa__load_config_module("tactyk_core_simd.visa");
        tactyk_visa__load_config_module("tactyk_core_simd-util.visa");
        tactyk_visa__load_config_module("tactyk_core_bits.visa");
        tactyk_visa__load_config_module("tactyk_core_stash_ext.visa");
    }
    else {
        tactyk_visa__load_config_module(visa_fname);
    }
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();
    
    tactyk_visa__init_emit(emitctx);
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *asmvmctx = tactyk_asmvm__new_context(vm);

    tactyk_debug__configure_api(emitctx);
    tactyk_emit_svc__configure(emitctx);
    aux_printit__configure(emitctx);
    aux_util__configure(emitctx);
    aux_sdl__configure(emitctx, 4096,4096);
   
    if (argc <= 1) {
        printf("Nothing to load.  bye!\n");
        exit(0);
    }
    
    char *fname = NULL;
     
    for (int64_t i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            if (strstr(argv[i], "perf") != NULL) {
                printf("Running with experimental security features disabled.  Enjoy!\n");
                emitctx->use_executable_layout_randomization = false;
                emitctx->use_exopointers = false;
                emitctx->use_extra_permutations = false;
                emitctx->use_immediate_scrambling = false;
            }
        }
        else {
            fname = arg;
        }
    }
    
    tactyk_run__init();
    tactyk_run__platform_init(emitctx);
    
    struct tactyk_run__RSC *rsc = tactyk_run__load_resource_pack(fname, emitctx, asmvmctx);
    tactyk_run__platform__set_resource_pack(rsc);
    
    tactyk_asmvm__invoke(asmvmctx, rsc->main_program, rsc->main_entrypoint);
    
    exit(0);
}













