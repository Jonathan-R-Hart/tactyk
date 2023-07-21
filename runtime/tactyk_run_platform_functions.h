
#ifndef TACTYK_RUN__PLATFORM_FUNCTIONS_H
#define TACTYK_RUN__PLATFORM_FUNCTIONS_H

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_run_resource_pack.h"

void tactyk_run__platform_init(struct tactyk_emit__Context *emitctx);
void tactyk_run__platform__set_resource_pack(struct tactyk_run__RSC *rsc);

void tactyk_run__platform__get_data(struct tactyk_asmvm__Context *ctx);
void tactyk_run__platform__export(struct tactyk_asmvm__Context *ctx);

#endif //TACTYK_RUN__PLATFORM_FUNCTIONS_H
