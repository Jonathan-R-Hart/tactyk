#ifndef TACTYK_RUN__RESOURCE_PACK_H
#define TACTYK_RUN__RESOURCE_PACK_H

#include <stdbool.h>
#include <stdint.h>

#include "tactyk_dblock.h"
#include "tactyk_emit.h"
#include "tactyk_asmvm.h"

#define TACTYK_RUN__RESTRICT_CHAR__BAN 0
#define TACTYK_RUN__RESTRICT_CHAR__ALLOW_ONE 1
#define TACTYK_RUN__RESTRICT_CHAR__ALLOW_MANY 2
#define TACTYK_RUN__RESTRICT_CHAR__SEP 3

struct tactyk_run__RSC {
    uint8_t charset[256];    
    char base_path[256];
    char manifest_name[256];
    
    struct tactyk_dblock__DBlock *data_table;
    struct tactyk_dblock__DBlock *export_table;
    struct tactyk_dblock__DBlock *module_table;
    struct tactyk_dblock__DBlock *program_table;
    
    struct tactyk_emit__Context *emitctx;
    struct tactyk_asmvm__Context *asmvmctx;
    struct tactyk_asmvm__Program *main_program;
    char main_entrypoint[256];
};

struct tactyk_run__exportable {
    struct tactyk_dblock__DBlock *dblock;
    char fname[1024];
    uint64_t maxlen;
    void *data;
};

typedef bool (*tactyk_run__rsc_item_handler)(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);

void tactyk_run__init();
//  void tactyk_run__rsc__delete_file(char *path, char *fname);
FILE* tactyk_run__rsc__get_fileref(char *path, char *fname, char *mode);
struct tactyk_run__RSC* tactyk_run__load_resource_pack(char *manifest_filename, struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *asmvmctx);
void tactyk_run__rsc__add_item_handler(char *name, tactyk_run__rsc_item_handler handler);

#endif //TACTYK_RUN__RESOURCE_PACK_H
