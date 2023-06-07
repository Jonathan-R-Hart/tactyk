
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tactyk_run_resource_pack.h"

#include "tactyk.h"
#include "tactyk_report.h"
#include "tactyk_emit.h"
#include "tactyk_pl.h"
#include "tactyk_asmvm.h"

void tactyk_run__rsc__restrict_charset__default(struct tactyk_run__RSC *rsc);
void tactyk_run__rsc__restrict_charset(struct tactyk_run__RSC *rsc, char *charset_many, char *charset_one, char *charset_sep);
bool tactyk_run__rsc__test_filename(struct tactyk_run__RSC *rsc, char *fname);
void tactyk_run__rsc__load_manifest(struct tactyk_run__RSC *rsc, char *filename);

struct tactyk_dblock__DBlock *tactyk_run__item_handlers;

bool tactyk_run__rsc__load_module(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);
bool tactyk_run__rsc__load_data(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);
bool tactyk_run__rsc__build_program(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);
bool tactyk_run__rsc__define_entrypoint(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);
bool tactyk_run__rsc__message(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data);

void tactyk_run__init() {
    tactyk_run__item_handlers = tactyk_dblock__new_table(16);
    tactyk_dblock__set_persistence_code(tactyk_run__item_handlers, 10000);
    
    tactyk_dblock__put(tactyk_run__item_handlers, "module", tactyk_run__rsc__load_module);
    tactyk_dblock__put(tactyk_run__item_handlers, "mod", tactyk_run__rsc__load_module);
    tactyk_dblock__put(tactyk_run__item_handlers, "data", tactyk_run__rsc__load_data);
    tactyk_dblock__put(tactyk_run__item_handlers, "dat", tactyk_run__rsc__load_data);
    tactyk_dblock__put(tactyk_run__item_handlers, "program", tactyk_run__rsc__build_program);
    tactyk_dblock__put(tactyk_run__item_handlers, "prg", tactyk_run__rsc__build_program);
    tactyk_dblock__put(tactyk_run__item_handlers, "main", tactyk_run__rsc__define_entrypoint);
    tactyk_dblock__put(tactyk_run__item_handlers, "msg", tactyk_run__rsc__message);
    tactyk_dblock__put(tactyk_run__item_handlers, "message", tactyk_run__rsc__message);
}

void tactyk_run__rsc__add_item_handler(char *name, tactyk_run__rsc_item_handler handler) {
    tactyk_dblock__put(tactyk_run__item_handlers, name, handler);
}

struct tactyk_run__RSC* tactyk_run__load_resource_pack(char *manifest_filename, struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *asmvmctx) {
    
    struct tactyk_run__RSC *rsc = calloc(1, sizeof(struct tactyk_run__RSC));
    
    rsc->data_table = tactyk_dblock__new_table(256);
    rsc->module_table = tactyk_dblock__new_table(256);
    rsc->program_table = tactyk_dblock__new_table(256);
    
    tactyk_dblock__set_persistence_code(rsc->data_table, 10000);
    tactyk_dblock__set_persistence_code(rsc->module_table, 10000);
    tactyk_dblock__set_persistence_code(rsc->program_table, 10000);
    
    rsc->emitctx = emitctx;
    rsc->asmvmctx = asmvmctx;
    
    tactyk_run__rsc__restrict_charset__default(rsc);
    tactyk_run__rsc__load_manifest(rsc, manifest_filename);
    
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

void tactyk_run__rsc__load_file(char *path, char *fname, int64_t *len, uint8_t **data) {
    char fullname[1024];
    snprintf(fullname, 1024, "%s/%s", path, fname);
    
    FILE *f = fopen(fullname, "r");
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    *data = calloc(*len+1, sizeof(uint8_t));
    fseek(f,0, SEEK_SET);
    fread(*data, *len, 1, f);
    fclose(f);
}
struct tactyk_dblock__DBlock* tactyk_run__rsc__to_structured_text(char *data) {
    struct tactyk_dblock__DBlock *stext = tactyk_dblock__from_safe_c_string(data);

    tactyk_dblock__fix(stext);
    tactyk_dblock__tokenize(stext, '\n', false);
    stext = tactyk_dblock__remove_blanks(stext, ' ', '#');
    tactyk_dblock__stratify(stext, ' ');
    tactyk_dblock__trim(stext);
    tactyk_dblock__tokenize(stext, ' ', true);
    
    tactyk_dblock__set_persistence_code(stext, 10000);
    
    return stext;
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
    memcpy(path, &filename[0], path_len-1);
    memcpy(fname, &filename[path_len], 256-path_len);
    
    tactyk_report__string("BASE PATH", path);
    tactyk_report__string("FILE", fname);
    rsc->base_path = path;
    rsc->manifest_name = fname;
    
    int64_t mlen = 0;
    uint8_t *mdata = NULL;
    
    tactyk_run__rsc__load_file(path, fname, &mlen, &mdata);
    
    tactyk_report__uint("MANIFEST LEN", mlen);
    
    struct tactyk_dblock__DBlock *manifest = tactyk_run__rsc__to_structured_text(mdata);
    
    bool dowarn = false;
    struct tactyk_dblock__DBlock *item = manifest;
    
    tactyk_report__break();
    
    while (item != NULL) {
        struct tactyk_dblock__DBlock *kw = item->token;
        if (tactyk_dblock__getchar(kw, 0) == '#') {
            continue;
        }
        tactyk_run__rsc_item_handler handler = tactyk_dblock__get(tactyk_run__item_handlers, kw);
        if (handler == NULL) {
            tactyk_report__dblock("Unrecognized resource-pack directive", item);
            dowarn = true;
        }
        else {
            handler(rsc, item);
        }
        item = item->next;
    }
    
    //tactyk_dblock__print_structure(manifest, true, true, false, 0, '\n');
    
    // parse the resource file, validate and load referneced files
    if (dowarn) {
        warn(NULL, NULL);
    }
}

bool tactyk_run__rsc__load_module(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data) {
    tactyk_report__dblock("load-module", data);
    
    struct tactyk_dblock__DBlock *token = data->token->next;
    struct tactyk_dblock__DBlock *name = token;
    struct tactyk_dblock__DBlock *filename = NULL;
    
    if (name == NULL) {
        tactyk_report__msg("Module directive parameters are missing");
        error(NULL, NULL);
    }
    filename = token->next;
    if (filename == NULL) {
        tactyk_report__msg("Module directive filename parameter is missing");
        error(NULL, NULL);
    }
    
    
    char filename_text[256];
    tactyk_dblock__export_cstring(filename_text, 256, filename);
    
    if (!tactyk_run__rsc__test_filename(rsc, filename_text)) {
        tactyk_report__string("Resource file name rejected (BANNED chars of undesirable patterns)", filename_text);
        error(NULL, NULL);
    }
    
    int64_t len = 0;
    uint8_t *bytes;
    
    tactyk_report__string("Load module-file", filename_text);
    tactyk_run__rsc__load_file(rsc->base_path, filename_text, &len, &bytes);    
    tactyk_report__int("Loaded bytes", len);
    
    struct tactyk_dblock__DBlock *tactyk_pl_module_src = tactyk_run__rsc__to_structured_text(bytes);
    
    tactyk_dblock__put(rsc->module_table, name, tactyk_pl_module_src);
}
bool tactyk_run__rsc__load_data(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data) {
    
    struct tactyk_dblock__DBlock *token = data->token->next;
    struct tactyk_dblock__DBlock *name = token;
    struct tactyk_dblock__DBlock *filename = NULL;
    
    if (name == NULL) {
        tactyk_report__msg("Data directive parameters are missing");
        error(NULL, NULL);
    }
    filename = token->next;
    if (filename == NULL) {
        tactyk_report__msg("Data directive filename parameter is missing");
        error(NULL, NULL);
    }
    
    
    char filename_text[256];
    tactyk_dblock__export_cstring(filename_text, 256, filename);
    
    if (!tactyk_run__rsc__test_filename(rsc, filename_text)) {
        tactyk_report__string("Data file name rejected (BANNED chars of undesirable patterns)", filename_text);
        error(NULL, NULL);
    }
    
    int64_t len = 0;
    uint8_t *bytes;
    
    tactyk_report__string("Load data-file", filename_text);
    tactyk_run__rsc__load_file(rsc->base_path, filename_text, &len, &bytes);    
    tactyk_report__int("Loaded bytes", len);
    
    struct tactyk_dblock__DBlock *dbctn = tactyk_dblock__from_bytes(NULL, bytes, 0, len, true); 
    
    tactyk_dblock__put(rsc->data_table, name, dbctn);
    
}
bool tactyk_run__rsc__build_program(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data) {
    
    struct tactyk_pl__Context *plctx = tactyk_pl__new(rsc->emitctx);
    tactyk_pl__define_constants(plctx, ".VISA", rsc->emitctx->visa_token_constants);
    
    struct tactyk_dblock__DBlock *prog_name = data->token->next;
    if (prog_name == NULL) {
        tactyk_report__dblock("Program directive parameters are missing", data);
        error(NULL, NULL);
    }
    struct tactyk_dblock__DBlock *module_name = prog_name->next;
    if (module_name == NULL) {
        tactyk_report__dblock("Program directive, no modules specified", data);
        error(NULL, NULL);
    }
    while (module_name != NULL) {
        struct tactyk_dblock__DBlock *module_src = (struct tactyk_dblock__DBlock*) tactyk_dblock__get(rsc->module_table, module_name);
        
        if (module_src == NULL) {
            tactyk_report__dblock("tactyk module not defined", module_name);
            error(NULL, NULL);
        }
        
        tactyk_pl__load_dblock(plctx, module_src);
        
        module_name = module_name->next;
    }
    struct tactyk_asmvm__Program *program = tactyk_pl__build(plctx);
    tactyk_asmvm__add_program(rsc->asmvmctx, program);
    
    tactyk_dblock__put(rsc->program_table, prog_name, program);
}
bool tactyk_run__rsc__define_entrypoint(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data) {
    
    struct tactyk_dblock__DBlock *prog_name = data->token->next;
    if (prog_name == NULL) {
        tactyk_report__dblock("Entry-point directive parameters are missing", data);
        error(NULL, NULL);
    }
    struct tactyk_dblock__DBlock *func_name = prog_name->next;
    if (func_name == NULL) {
        tactyk_report__dblock("Entry-point directive, function name is missing", data);
        error(NULL, NULL);
    }
    
    rsc->main_program = (struct tactyk_asmvm__Program*) tactyk_dblock__get(rsc->program_table, prog_name);
    if (rsc->main_program == NULL) {
        tactyk_report__dblock("Entry-point directive, program not defined", data);
        error(NULL, NULL);
        
    }
    tactyk_dblock__export_cstring(rsc->main_entrypoint, 256, func_name);
}

bool tactyk_run__rsc__message(struct tactyk_run__RSC *rsc, struct tactyk_dblock__DBlock *data) {
    if (data->child != NULL) {
        tactyk_dblock__print_structure(data->child, true, true, false, 0, '\n');
    }
    else {
        struct tactyk_dblock__DBlock *token = data->token->next;
        while (token != NULL) {
            tactyk_dblock__print(token);
            putc(' ', stdout);
            token = token->next;
        }
        putc('\n', stdout);
    }
}


