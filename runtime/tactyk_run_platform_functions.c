#include "tactyk_run_platform_functions.h"

#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

struct tactyk_run__RSC *tactyk_run__platform__resources;

void tactyk_run__platform_init(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__add_tactyk_apifunc(emitctx, "get-data", tactyk_run__platform__get_data);
}

void tactyk_run__platform__set_resource_pack(struct tactyk_run__RSC *rsc) {
    tactyk_run__platform__resources = rsc;
}

//  tcall get-data
//  input:
//      rA  - name of data block to retrieve (8 char names)
//  output:
//      memblock1 - memory binding granting access to the data block.
void tactyk_run__platform__get_data(struct tactyk_asmvm__Context *ctx) {
    uint64_t data[2];
    data[0] = ctx->reg.rA;
    data[1] = 0;
    char *name = (char*)data; 
    struct tactyk_dblock__DBlock *ctn = tactyk_dblock__get(tactyk_run__platform__resources->data_table, name);
    ctx->reg.rADDR1 = (uint64_t*) ctn->data;
    struct tactyk_asmvm__memblock_lowlevel *mref = &ctx->active_memblocks[0];
    mref->base_address = (uint8_t*)ctn->data;
    mref->array_bound = 1;
    mref->element_bound = ctn->length;
    mref->memblock_index = 0xffffffff;
    mref->offset = 0;
}
