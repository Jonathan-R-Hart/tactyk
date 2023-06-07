#ifndef TACTYK_RUN__RESOURCE_PACK_H
#define TACTYK_RUN__RESOURCE_PACK_H

#include "stdbool.h"

#include "tactyk_dblock.h"

#define TACTYK_RUN__RESTRICT_CHAR__BAN 0
#define TACTYK_RUN__RESTRICT_CHAR__ALLOW_ONE 1
#define TACTYK_RUN__RESTRICT_CHAR__ALLOW_MANY 2
#define TACTYK_RUN__RESTRICT_CHAR__SEP 3

struct tactyk_run__RSC {
    uint8_t charset[256];
    char *base_path;
    
    struct tactyk_dblock__DBlock *data_table;
    struct tactyk_dblock__DBlock *constant_table;
    struct tactyk_dblock__DBlock *module_table;
    struct tactyk_dblock__DBlock *program_table;
    
    struct tactyk_asmvm__Program *main_program;
    char *main_entrypoint;
};

typedef bool (*tactyk_run__rsc_item_handler)(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);

void tactyk_run__init();

struct tactyk_run__RSC* tactyk_run__load_resource_pack(char *manifest_filename);
void tactyk_run__rsc__add_item_handler(char *name, tactyk_run__rsc_item_handler handler);

#endif //TACTYK_RUN__RESOURCE_PACK_H
