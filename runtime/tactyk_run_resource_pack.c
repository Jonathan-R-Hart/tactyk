
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tactyk_run_resource_pack.h"

#include "tactyk.h"
#include "tactyk_report.h"

void tactyk_run__rsc__restrict_charset__default(struct tactyk_run__RSC *rsc);
void tactyk_run__rsc__restrict_charset(struct tactyk_run__RSC *rsc, char *charset_many, char *charset_one, char *charset_sep);
bool tactyk_run__rsc__test_filename(struct tactyk_run__RSC *rsc, char *fname);
void tactyk_run__rsc__load_manifest(struct tactyk_run__RSC *rsc, char *filename);

struct tactyk_dblock__DBlock *tactyk_run__item_handlers;

void tactyk_run__init() {
    tactyk_run__item_handlers = tactyk_dblock__new_table(16);
    tactyk_dblock__set_persistence_code(tactyk_run__item_handlers, 10000);
    
}

void tactyk_run__rsc__add_item_handler(char *name, tactyk_run__rsc_item_handler handler) {
    tactyk_dblock__put(tactyk_run__item_handlers, name, handler);
}

struct tactyk_run__RSC* tactyk_run__load_resource_pack(char *manifest_filename) {
    struct tactyk_run__RSC *rsc = calloc(1, sizeof(struct tactyk_run__RSC));
    tactyk_run__rsc__restrict_charset__default(rsc);
    tactyk_run__rsc__load_manifest(rsc, manifest_filename);
    
    rsc->constant_table = tactyk_dblock__new_table(256);
    rsc->data_table = tactyk_dblock__new_table(256);
    rsc->module_table = tactyk_dblock__new_table(256);
    rsc->program_table = tactyk_dblock__new_table(256);
    
    tactyk_dblock__set_persistence_code(rsc->constant_table, 10000);
    tactyk_dblock__set_persistence_code(rsc->data_table, 10000);
    tactyk_dblock__set_persistence_code(rsc->module_table, 10000);
    tactyk_dblock__set_persistence_code(rsc->program_table, 10000);
    
    return rsc;
}

void tactyk_run__rsc__restrict_charset__default(struct tactyk_run__RSC *rsc) {
    tactyk_run__rsc__restrict_charset(rsc, "01234589abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_", ".", "/");
}
void tactyk_run__rsc__restrict_charset(struct tactyk_run__RSC *rsc, char *charset_many, char *charset_one, char *charset_sep) {
    memset(rsc->charset, 0, 256);
    uint64_t ln = strlen(charset_many);
    
    for (uint64_t i = 0; i < ln; i += 1) {
        rsc->charset[(uint8_t)charset_many[i]] = TACTYK_RUN__RESTRICT_CHAR__ALLOW_MANY;
    }
    ln = strlen(charset_one);
    for (uint64_t i = 0; i < ln; i += 1) {
        rsc->charset[(uint8_t)charset_one[i]] = TACTYK_RUN__RESTRICT_CHAR__ALLOW_ONE;
    }
    ln = strlen(charset_sep);
    for (uint64_t i = 0; i < ln; i += 1) {
        rsc->charset[(uint8_t)charset_sep[i]] = TACTYK_RUN__RESTRICT_CHAR__SEP;
    }
}
bool tactyk_run__rsc__test_filename(struct tactyk_run__RSC *rsc, char *fname) {
    uint64_t ln = strlen(fname);
    uint64_t cv = 0;
    uint64_t cv_prev = 0;
    for (uint64_t i = 0; i < ln; i += 1) {
        cv_prev = cv;
        cv = (uint64_t)fname[i];
        switch(rsc->charset[cv]) {
            case TACTYK_RUN__RESTRICT_CHAR__BAN: {
                return false;
            }
            case TACTYK_RUN__RESTRICT_CHAR__ALLOW_MANY: {
                continue;
            }
            case TACTYK_RUN__RESTRICT_CHAR__SEP:
            case TACTYK_RUN__RESTRICT_CHAR__ALLOW_ONE: {
                if (cv_prev == cv) {
                    return false;
                }
                else {
                    continue;
                }
            }
        }
    }
    return true;
}
void tactyk_run__rsc__load_manifest(struct tactyk_run__RSC *rsc, char *filename) {
    tactyk_report__string("path from file", filename);
    uint64_t pos = 0;
    uint64_t path_len;
    uint64_t len = strlen(filename);
    for (uint64_t pos = 0; pos < len; pos++) {
        char c = filename[pos];
        uint64_t cv = (uint64_t)c;
        switch(rsc->charset[cv]) {
            case TACTYK_RUN__RESTRICT_CHAR__SEP: {
                path_len = pos+1;
                break;
            }
            case TACTYK_RUN__RESTRICT_CHAR__BAN: {
                char buf[2];
                buf[1] = 0;
                buf[0] = c;
                tactyk_report__string("file name contains banned char", buf);
                error(NULL, NULL);
                break;
            }
        }
    }
    tactyk_report__msg("File name test passed");
    char path[256];
    char fname[256];
    memset(path, 0, 256);
    memset(fname, 0, 256);
    memcpy(path, &filename[0], path_len);
    memcpy(fname, &filename[path_len], 256-path_len);
    
    tactyk_report__string("BASE PATH", path);
    tactyk_report__string("FILE", fname);
    
    FILE *manifest_file = fopen(filename, "r");
    fseek(manifest_file, 0, SEEK_END);
    int64_t mlen = ftell(manifest_file);
    char *manifest_data = calloc(mlen+1, sizeof(char));
    fseek(manifest_file,0, SEEK_SET);
    fread(manifest_data, mlen, 1, manifest_file);
    fclose(manifest_file);
    
    tactyk_report__uint("MANIFEST LEN", mlen);
    
    struct tactyk_dblock__DBlock *manifest = tactyk_dblock__from_safe_c_string(manifest_data);

    tactyk_dblock__fix(manifest);
    tactyk_dblock__tokenize(manifest, '\n', false);
    manifest = tactyk_dblock__remove_blanks(manifest, ' ', '#');
    tactyk_dblock__stratify(manifest, ' ');
    tactyk_dblock__trim(manifest);
    tactyk_dblock__tokenize(manifest, ' ', true);
    
    //tactyk_dblock__print_structure(manifest, true, true, false, 0, '\n');
    
    // parse the resource file, validate and load referneced files
}





