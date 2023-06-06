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
    
    struct tactyk_dblock__DBlock *data;
    
};

struct tactyk_run__RSC* tactyk_run__load_resource_pack(char *manifest_filename);

#endif //TACTYK_RUN__RESOURCE_PACK_H
