#include "tactyk_run_platform_functions.h"

#include <string.h>

#include "tactyk_report.h"
#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

struct tactyk_run__RSC *tactyk_run__platform__resources;

void tactyk_run__platform_init(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__add_tactyk_apifunc(emitctx, "get-data", tactyk_run__platform__get_data);
    tactyk_emit__add_tactyk_apifunc(emitctx, "export", tactyk_run__platform__export);
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
    if (ctx->reg.rA == 0) {
        tactyk_report__reset();
        tactyk_report__msg("data-resource specifier is NULL (rA == 0)");
        error(NULL, NULL);
    }
    uint64_t data[2];
    data[0] = ctx->reg.rA;
    data[1] = 0;
    char *name = (char*)data;
    struct tactyk_dblock__DBlock *ctn = tactyk_dblock__get(tactyk_run__platform__resources->data_table, name);
    if (ctn == NULL) {
        tactyk_report__reset();
        tactyk_report__string("data-resource specifier is invalid (rA)", name);
        error(NULL, NULL);
    }
    ctx->reg.rADDR1 = (uint64_t*) ctn->data;
    struct tactyk_asmvm__memblock_lowlevel *mref = &ctx->active_memblocks[0];
    mref->base_address = (uint8_t*)ctn->data;
    mref->array_bound = 1;
    mref->element_bound = ctn->length;
    mref->memblock_index = 0xffffffff;
    mref->offset = 0;
}

void tactyk_run__platform__export(struct tactyk_asmvm__Context *ctx) {
    // if nothing bound to the input address, give up immediately.
    char *ptr = (char*) ctx->active_memblocks[0].base_address;
    if (ptr == NULL) {
        return;
    }
    // search the export table for a reference to the indicated buffer
    //      (maybe should return pass/fail or an error code)
    for (uint64_t i = 0; i < tactyk_run__platform__resources->export_table->element_count; i += 1) {
        struct tactyk_run__exportable *exp =
            (struct tactyk_run__exportable*) tactyk_dblock__index(tactyk_run__platform__resources->export_table, i);
        if (exp->data == ptr) {
            int64_t pos = exp->maxlen -1;
            // measure the file size by searching for the last non-null byte.
            //  last-position + 1 is the length.
            while (pos >= 0) {
                if (ptr[pos] != 0) {
                    break;
                }
                pos -= 1;
            }
            if (pos == -1) {
                // delete the file
                //FILE *f = tactyk_run__rsc__delete_file(tactyk_run__platform__resources->base_path, exp->fname);
                //tactyk_report__string("Deleted export target", exp->fname);
            }
            else {
                // persistent datas!
                FILE *f = tactyk_run__rsc__get_fileref(tactyk_run__platform__resources->base_path, exp->fname, "w");
                if (f == NULL) {
                    tactyk_report__msg("Failed to open file for writing.");
                    error(NULL, NULL);
                }
                fwrite(ptr, pos+1, 1, f);
                tactyk_report__string("Wrote to export target", exp->fname);
            }
        }
    }
}








