//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tactyk_visa.h"
#include "tactyk_emit.h"
#include "tactyk_dblock.h"
#include "tactyk.h"

// tactyk config is intended to have shallow nesting, but a high value is used here because
//  it was decided that "depth" as a direct indentation measurement would be simpler to implement.
//  (small values would cause this to break if high indent levels are used in the config file)
#define TACTYK_VISA_CONFIG_MAXDEPTH 64

bool tactyk_visa__mk_instruction(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__mk_typespec(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__mk_subroutine(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__ld_header(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__cfg_settings(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__cfg_fterminals(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);
bool tactyk_visa__extend(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec);

struct tactyk_dblock__DBlock *visa_hl_subroutines;


bool initialized = false;

//struct tactyk_textbuf__Buffer *tvn_sttext;          // static-ish collection of strings to avoid use-after free while processing programs.
//struct tactyk_structured_text *st_buf;
struct tactyk_dblock__DBlock *tactyk_visa_spec;

// a non-null "flag" indicator to insert into
struct tactyk_dblock__DBlock *TACTYK_VISA_FLAG;

char *rsc_directory_name;

void tactyk_visa__init(char *rsc_dname) {

    if (initialized) return;
    initialized = true;
    rsc_directory_name = rsc_dname;

    visa_hl_subroutines = tactyk_dblock__new_table(16);
    tactyk_dblock__put(visa_hl_subroutines, "instruction", tactyk_visa__mk_instruction);
    tactyk_dblock__put(visa_hl_subroutines, "typespec", tactyk_visa__mk_typespec);
    tactyk_dblock__put(visa_hl_subroutines, "subroutine", tactyk_visa__mk_subroutine);
    tactyk_dblock__put(visa_hl_subroutines, "settings", tactyk_visa__cfg_settings);
    tactyk_dblock__put(visa_hl_subroutines, "header", tactyk_visa__ld_header);
    tactyk_dblock__put(visa_hl_subroutines, "extend", tactyk_visa__extend);

    if (TACTYK_VISA_FLAG == NULL) {
        TACTYK_VISA_FLAG = tactyk_dblock__from_c_string("flag");
    }
    //sub_instruction
}

void tactyk_visa__load_config_module(char *visa_fname) {
    // load the configuration file
    // read it line by line
    // use the off-side rule to generate a basic data strcture (pythonish syntactic whitespace)
    //      in principle, I probably should generalize the bracketed block handling from other portions of tactyk, but it isn't properly recursive,
    //      and the the configuration file is intended have a simple grammar and very shallow nesting
    // measure leading whitespace
    // tokenize each line
    //      use whitespace as token delimiter
    //      ignore zero-length tokens
    // store tokens
    // add a convenience reference to the first token of each line called "command" (could stand to remove this or switch it to a macro)
    // use a stack to structure the data
    // the data structure is heirarchical with each node having a pointer to an adjacent node and a child node
    //      NULL pointers are interpreted as terminals,
    //      it is assumed that if the file can be parsed, then there is no serious need to restrict the pointer chasing when handling the data structure
    //          (NOTE:  the virtual ISA defines the low-level behavior of the virtual machuine, and thus is a highly priviliged construct, so checking
    //                  is only about consistancy and correctness.)
    //
    // Afterward, apply the configuration by invoking the handler for component top-level item in the data structure
    char fname[1024];
    snprintf(fname, 1024, "%s/%s", rsc_directory_name, visa_fname);
    FILE *f = fopen(fname, "r");
    if (f == NULL) {
        error("VIRTUAL-ISA -- specification file not found", NULL);
    }
    fseek(f, 0, SEEK_END);
    int64_t len = ftell(f);
    uint8_t *fbytes = calloc(len+1, sizeof(uint8_t));
    fseek(f,0, SEEK_SET);
    fread(fbytes, len, 1, f);
    fclose(f);

    struct tactyk_dblock__DBlock *visa_src = tactyk_dblock__from_bytes(NULL, fbytes, 0, (uint64_t)len, true);
    
    
    tactyk_dblock__fix(visa_src);
    tactyk_dblock__tokenize(visa_src, '\n', false);
    struct tactyk_dblock__DBlock *vspec = tactyk_dblock__remove_blanks(visa_src, ' ', '#');
    tactyk_dblock__stratify(vspec, ' ');
    tactyk_dblock__trim(vspec);
    tactyk_dblock__tokenize(vspec, ' ', true);
    tactyk_dblock__set_persistence_code(vspec, 100);
    
    if (tactyk_visa_spec == NULL) {
        tactyk_visa_spec = vspec;
    }
    else {
        struct tactyk_dblock__DBlock *vspec_tail = tactyk_visa_spec;
        while (vspec_tail->next != NULL) {
            vspec_tail = vspec_tail->next;
        }
        vspec_tail->next = vspec;
    }
}

void tactyk_visa__init_emit(struct tactyk_emit__Context *ctx) {
    for (int32_t i = 0; i < 256; i++) {
        ctx->namechars[i] = false;
    }
    char *nchars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.";
    int32_t nc_count = strlen(nchars);
    for (int32_t i = 0; i < nc_count; i++) {
        char ch = nchars[i];
        ctx->namechars[(int32_t)ch] = true;
    }
    //generate top-level visa subroutines routines (they're context-specific)
    struct tactyk_dblock__DBlock *st_code = tactyk_visa_spec;
    while (st_code != NULL) {
        struct tactyk_dblock__DBlock *tok = st_code->token;
        tactyk_emit__sub_func tvisa_sub = (tactyk_emit__sub_func) tactyk_dblock__get(visa_hl_subroutines, tok);
        if (tvisa_sub == NULL) {
            error("TACTYK-VISA -- Unrecognized top-level tactyk-visa subroutine", st_code);
        }
        tvisa_sub(ctx, st_code);
        st_code = st_code->next;
    }
    // generate an inverse mapping of handles to visa-tokens by reading the token table
    // Where there are collisions, the last entry in the token table "wins".  THis is arbitrary and should make no difference,
    //      as colliding entries are all supposed to resolve to the same thing.
    struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**) ctx->visa_token_constants->data;
    uint64_t t_count = ctx->visa_token_constants->element_count;
    ctx->visa_token_invmap = calloc(t_count, sizeof(void*));
    for (uint64_t i = 0; i < ctx->visa_token_constants->element_capacity; i += 1) {
        uint64_t ofs = i*2;
        struct tactyk_dblock__DBlock *key = fields[ofs];
        struct tactyk_dblock__DBlock *value = fields[ofs+1];
        uint64_t ival = 0;
        if (key != NULL) {
            if (!tactyk_dblock__try_parseuint(&ival, value)) {
                error("VISA -- invalid or corrupt visa-token handle (impossible condition)", NULL);
            }
            assert(ival < t_count);
            ctx->visa_token_invmap[ival] = key;
        }
    }
}

bool tactyk_visa__mk_instruction(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *name = vopcfg->token->next;
    
    bool chainin = false;
    bool chainout = false;
    
    if (tactyk_dblock__equals_c_string(name, "chain-in")) {
        name = name->next;
        chainin = true;
    }
    else if (tactyk_dblock__equals_c_string(name, "chain-out")) {
        name = name->next;
        chainout = true;
    }
    else if (tactyk_dblock__equals_c_string(name, "chain")) {
        name = name->next;
        chainin = true;
        chainout = true;
    }
    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__new_managedobject(ctx->instruction_table, name);
    sub->func = tactyk_emit__ExecInstruction;
    sub->vopcfg = vopcfg;
    sub->chain_in = chainin;
    sub->chain_out = chainout;
    
    uint64_t index = ctx->token_handle_count;
    ctx->token_handle_count += 1;
    struct tactyk_dblock__DBlock *th_value = tactyk_dblock__from_uint(index);
    struct tactyk_dblock__DBlock *th_name = tactyk_dblock__new(16);
    tactyk_dblock__append_char(th_name, '.');
    tactyk_dblock__append(th_name, name);
    tactyk_dblock__put(ctx->visa_token_constants, th_name, th_value);
    return true;
}

bool tactyk_visa__mk_typespec(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *name = vopcfg->token->next;
    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__new_managedobject(ctx->typespec_table, name);
    sub->func = tactyk_emit__ExecSubroutine;
    sub->vopcfg = vopcfg;
    struct tactyk_dblock__DBlock *tlist = vopcfg->child;
    while (tlist != NULL) {
        if (strncmp(tlist->data, "select-operand", 14) == 0) {
            goto extract_tokens;
        }
        tlist = tlist->next;
    }
    return true;

    extract_tokens:
    struct tactyk_dblock__DBlock *specifier = tlist->child;

    while (specifier != NULL) {
        uint64_t index = ctx->token_handle_count;
        ctx->token_handle_count += 1;
        struct tactyk_dblock__DBlock *th_value = tactyk_dblock__from_uint(index);
        struct tactyk_dblock__DBlock *spec_token = specifier->token->next;
        struct tactyk_dblock__DBlock *th_name;
        while (spec_token != NULL) {
            th_name = tactyk_dblock__new(16);
            tactyk_dblock__append_char(th_name, '.');
            tactyk_dblock__append(th_name, spec_token);
            tactyk_dblock__put(ctx->visa_token_constants, th_name, th_value);
            spec_token = spec_token->next;
        }
        specifier = specifier->next;
    }

    return true;
}
bool tactyk_visa__mk_subroutine(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *name = vopcfg->token->next;
    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__new_managedobject(ctx->subroutine_table, name);
    sub->func = tactyk_emit__ExecSubroutine;
    sub->vopcfg = vopcfg;
    return true;
}
bool tactyk_visa__ld_header(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *name = vopcfg->token->next;
    char fname[512];
    tactyk_dblock__export_cstring(fname, 512, name);
    char buf[1024];
    snprintf(buf, 1024, "%s/%s", rsc_directory_name, fname);
    FILE *f = fopen(buf, "r");
    fseek(f, 0, SEEK_END);
    int64_t len = ftell(f);
    ctx->asm_header = calloc(len, sizeof(char)+1);
    fseek(f,0, SEEK_SET);
    fread(ctx->asm_header, len, 1, f);
    fclose(f);
    return true;
}
bool tactyk_visa__cfg_settings(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *entry = vopcfg->child;
    while (entry != NULL) {
        struct tactyk_dblock__DBlock *name = entry->token;
        struct tactyk_dblock__DBlock *value = entry->token->next;
        if (value == NULL) {
            value = TACTYK_VISA_FLAG;
        }
        tactyk_dblock__put(ctx->global_vars, name, value);
        entry = entry->next;
    }
    return true;
}

bool tactyk_visa__extend(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *cls = spec->token->next;
    if (cls == NULL) {
        error("VISA -- Subroutine extender does not specify a class", spec);
    }
    
    struct tactyk_dblock__DBlock *name = cls->next;
    if (name == NULL) {
        error("VISA -- Subroutine extender does not specify a class", spec);
    }
    struct tactyk_dblock__DBlock *target_token = name->next;
    
    struct tactyk_emit__subroutine_spec *sub = NULL;
    if ( tactyk_dblock__equals_c_string(cls, "sub") || tactyk_dblock__equals_c_string(cls, "subroutine") ) {
        sub = tactyk_dblock__get(ctx->subroutine_table, name);
    }
    if ( tactyk_dblock__equals_c_string(cls, "instr") || tactyk_dblock__equals_c_string(cls, "instruction") ) {
        sub = tactyk_dblock__get(ctx->instruction_table, name);
    }
    if ( tactyk_dblock__equals_c_string(cls, "typespec") ) {
        sub = tactyk_dblock__get(ctx->typespec_table, name);
    }
    
    if (sub == NULL) {
        error("VISA [subroutine extender] -- subroutine not found", spec);
    }
    
    int64_t stack_index = 0;
    struct tactyk_dblock__DBlock **sub_stack = calloc(256, sizeof(void*));    
    struct tactyk_dblock__DBlock *target = sub->vopcfg;
    sub_stack[stack_index] = target;
    int64_t ct = 0;
    
    if (target_token != NULL) {
        struct tactyk_dblock__DBlock *index = target_token->next;
        if (target_token->next != NULL) {
            if (!tactyk_dblock__try_parseint(&ct, index)) {
                error("VISA [subroutine extender target index] -- invalid integer", spec);
            }
        }
    }
    
    struct tactyk_dblock__DBlock *t = target;
    while (target_token != NULL) {
        if (tactyk_dblock__equals(t->token, target_token)) {
            if (ct <= 0) {
                target = t;
                goto appendit;
            }
            else {
                ct -= 1;
            }
        }
        if (t->child != NULL) {
            t = t->child;
            stack_index += 1;
            sub_stack[stack_index] = t;
        }
        else if (t->next != NULL) {
            t = t->next;
            sub_stack[stack_index] = t;
        }
        else if (stack_index > 0) {
            stack_index -= 1;
            t = sub_stack[stack_index]->next;
            sub_stack[stack_index] = t;
            while (t == NULL) {
                if (stack_index <= 0) {
                    error("VISA [subroutine extender] -- Invalid target", spec);
                }
                stack_index -= 1;
                t = sub_stack[stack_index]->next;
            }
        }
        else {
            error("VISA [subroutine extender] -- Invalid target", spec);
        }
    }
    
    appendit: {
        if (target->child == NULL) {
            target->child = spec->child;
        }
        else {
            target = target->child;
            while (target->next != NULL) {
                target = target->next;
            }
            target->next = spec->child;
        }
    }
}



