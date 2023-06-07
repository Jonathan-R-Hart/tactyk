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

#include "tactyk_run_resource_pack.h"

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
    }
    else {
        tactyk_visa__load_config_module(visa_fname);
    }
    struct tactyk_emit__Context *emitctx = tactyk_emit__init();
    
    tactyk_visa__init_emit(emitctx);
    struct tactyk_asmvm__VM *vm = tactyk_asmvm__new_vm();
    struct tactyk_asmvm__Context *ctx = tactyk_asmvm__new_context(vm);

    tactyk_debug__configure_api(emitctx);
    tactyk_emit_svc__configure(emitctx);
    aux_printit__configure(emitctx);
    aux_util__configure(emitctx);

    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__define_constants(plctx, ".VISA", emitctx->visa_token_constants);
    
    
    if (argc <= 1) {
        printf("Nothing to load.  bye!\n");
        exit(0);
    }
    
    void tactyk_run__init();
    
    struct tactyk_run__RSC *rsc = tactyk_run__load_resource_pack(argv[1]);
    exit(0);
}













