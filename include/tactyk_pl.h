//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_PL__INCLUDE_GUARD
#define TACTYK_PL__INCLUDE_GUARD

#include "tactyk_util.h"

#include "tactyk_asmvm.h"
#include "tactyk_visa.h"
#include "tactyk_emit.h"

#include "tactyk_dblock.h"

#define MAX_BLOCK_TOKENS 2048

#define STASH_ARRAY_SIZE 65536
#define STASH_ELEMENT_SIZE 32

#define TACTYK_PL__RAW_TEXT_MAX_LENGTH 1024
#define TACTYK_PL__DEFINITION_TEXT_MAX_LENGTH 65536
#define TACTYK_PL__MAX_BLOCK_TOKENS 4096

void tactyk_pl__init();
struct tactyk_pl__Context *tactyk_pl__new(struct tactyk_emit__Context *emitctx);
void tactyk_pl__load(struct tactyk_pl__Context *plctx, char *code);
void tactyk_pl__load_dblock(struct tactyk_pl__Context *plctx, struct tactyk_dblock__DBlock *dbcode);
struct tactyk_asmvm__Program* tactyk_pl__build(struct tactyk_pl__Context *plctx);

struct text_definition {
    char input[TACTYK_PL__RAW_TEXT_MAX_LENGTH];
    uint64_t input_length;
    char output[TACTYK_PL__DEFINITION_TEXT_MAX_LENGTH];
    uint64_t output_length;
    int64_t min_position;
    int64_t max_position;
};

struct tactyk_pl__Context {
    struct tactyk_emit__Context *emitctx;
    struct tactyk_asmvm__Program *program;
    struct tactyk_dblock__DBlock *struct_table;
    struct tactyk_dblock__DBlock *memspec_highlevel_table;
    struct tactyk_dblock__DBlock *memspec_lowlevel_buffer;
    
    struct tactyk_dblock__DBlock *getters;
    struct tactyk_dblock__DBlock *setters;
    
    bool alias_chars[256];
    
    struct tactyk_dblock__DBlock *constant_sets;
    struct tactyk_dblock__DBlock *bus_tokens[256];
};

void tactyk_pl__add_script_command(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *token, struct tactyk_dblock__DBlock *line);
void tactyk_pl__rewrite_aliased_tokens(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *alias_table, struct tactyk_dblock__DBlock *dbcode);

bool tactyk_pl__bus(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__params(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__var(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__get(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__set(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__struct(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__mem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__extmem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__flatdata(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__data(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__text(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__const(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);

bool tactyk_pl__ld_constants(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
bool tactyk_pl__ld_visa_constants(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
void tactyk_pl__define_constants(struct tactyk_pl__Context *ctx, char *name, struct tactyk_dblock__DBlock *constants);

#endif /* TACTYK_PL__INCLUDE_GUARD */
