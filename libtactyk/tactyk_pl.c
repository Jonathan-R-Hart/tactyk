//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include <assert.h>

#include "tactyk_asmvm.h"
#include "tactyk.h"
#include "tactyk_pl.h"
#include "tactyk_alloc.h"

#include "tactyk_dblock.h"
#include "tactyk_report.h"

typedef bool (*tactyk_pl__func)(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
struct tactyk_dblock__DBlock *tkpl_funcs;
struct tactyk_asmvm__struct default_mem_layout;

void tactyk_pl__init() {
    tkpl_funcs = tactyk_dblock__new_table(16);

    tactyk_dblock__put(tkpl_funcs, "mem", tactyk_pl__mem);
    tactyk_dblock__put(tkpl_funcs, "struct", tactyk_pl__struct);
    tactyk_dblock__put(tkpl_funcs, "extmem", tactyk_pl__extmem);
    tactyk_dblock__put(tkpl_funcs, "data", tactyk_pl__data);
    tactyk_dblock__put(tkpl_funcs, "flat", tactyk_pl__flatdata);
    tactyk_dblock__put(tkpl_funcs, "text", tactyk_pl__text);
    tactyk_dblock__put(tkpl_funcs, "const", tactyk_pl__const);
    tactyk_dblock__put(tkpl_funcs, "var", tactyk_pl__var);
    tactyk_dblock__put(tkpl_funcs, "get", tactyk_pl__get);
    tactyk_dblock__put(tkpl_funcs, "set", tactyk_pl__set);
    tactyk_dblock__put(tkpl_funcs, "use_vconstants", tactyk_pl__ld_visa_constants);
    tactyk_dblock__put(tkpl_funcs, "use", tactyk_pl__ld_constants);
    tactyk_dblock__put(tkpl_funcs, "bus", tactyk_pl__bus);
    
    tactyk_dblock__set_persistence_code(tkpl_funcs, 100);

    default_mem_layout.byte_stride = 8;
    strcpy(default_mem_layout.name, "default-layout");
    default_mem_layout.num_properties = 1;
    default_mem_layout.properties = calloc(1, sizeof(struct tactyk_asmvm__property));
    default_mem_layout.properties->byte_offset = 0;
    default_mem_layout.properties->byte_width = 8;
    strncpy(default_mem_layout.properties->name, "item", TACTYK__MAX_IDENTIFIER_LENGTH);
}

struct tactyk_pl__Context *tactyk_pl__new(struct tactyk_emit__Context *emitctx) {
    struct tactyk_pl__Context *ctx = tactyk_alloc__allocate(1, sizeof(struct tactyk_pl__Context));
    ctx->emitctx = emitctx;

    ctx->struct_table = tactyk_dblock__new_managedobject_table(64, sizeof(struct tactyk_asmvm__struct));
    ctx->getters = tactyk_dblock__new_table(64);
    ctx->setters = tactyk_dblock__new_table(64);

    // the struct table gets to survive garbage collection!  barely ... (the only real purpose of it is reflection)
    tactyk_dblock__set_persistence_code(ctx->struct_table, 10);
    //tactyk_dblock__set_persistence_code(ctx->getters, 10);
    //tactyk_dblock__set_persistence_code(ctx->setters, 10);

    tactyk_emit__init_program(emitctx);

    ctx->program = emitctx->program;
    ctx->memspec_highlevel_table = ctx->program->memory_layout_hl;
    ctx->memspec_lowlevel_buffer = ctx->program->memory_layout_ll;
    
    ctx->constant_sets = tactyk_dblock__new_table(64);
    tactyk_dblock__set_persistence_code(ctx->constant_sets, 10);
    
    for (int32_t i = 0; i < 256; i++) {
        ctx->alias_chars[i] = false;
    }
    char *achars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int32_t nc_count = strlen(achars);
    for (int32_t i = 0; i < nc_count; i++) {
        char ch = achars[i];
        ctx->alias_chars[(int32_t)ch] = true;
    }
    
    return ctx;
}
void tactyk_pl__define_constants(struct tactyk_pl__Context *ctx, char *name, struct tactyk_dblock__DBlock *constants) {
    tactyk_dblock__put(ctx->constant_sets, name, constants);
}

void tactyk_pl__load(struct tactyk_pl__Context *plctx, char *code) {
    struct tactyk_dblock__DBlock *dbcode = tactyk_dblock__from_safe_c_string(code);

    tactyk_dblock__fix(dbcode);
    tactyk_dblock__tokenize(dbcode, '\n', false);
    dbcode = tactyk_dblock__remove_blanks(dbcode, ' ', '#');
    tactyk_dblock__stratify(dbcode, ' ');
    tactyk_dblock__trim(dbcode);
    tactyk_dblock__tokenize(dbcode, ' ', true);

    tactyk_pl__load_dblock(plctx, dbcode);
}

void tactyk_pl__load_dblock(struct tactyk_pl__Context *plctx, struct tactyk_dblock__DBlock *dbcode) {
    struct tactyk_emit__Context *emitctx = plctx->emitctx;
    if (plctx->alias_table != NULL) {
        tactyk_pl__resolve_aliased_tokens(plctx, dbcode);
    }
    struct tactyk_dblock__DBlock *dbstack[256];
    int64_t dbstack_index = 0;
    dbstack[0] = dbcode;
    bool escape_block = false;
    while (dbcode != NULL) {
        handle_dbcode: {
            tactyk_pl__func func = tactyk_dblock__get(tkpl_funcs, dbcode->token);
            if (func == NULL) {
                escape_block = false;
                struct tactyk_dblock__DBlock *tok = dbcode->token;

                assert (tok != NULL);
                if (tactyk_dblock__lastchar(tok) == ':') {
                    tok->length -= 1;       // ignore the ':'
                    tactyk_dblock__update_hash(tok);
                    tactyk_emit__add_script_label(emitctx, tok);
                    tok = tok->next;
                }
                if (tok != NULL) {
                    tactyk_pl__add_script_command(plctx, tok, dbcode);
                    //tactyk_emit__add_script_command(emitctx, tok, dbcode);
                }
            }
            else {
                escape_block = func(plctx, dbcode);
            }
        }
        if (dbcode != NULL) {
            if ((!escape_block) && (dbcode->child != NULL)) {
                dbstack[dbstack_index] = dbcode;
                dbstack_index += 1;
                dbcode = dbcode->child;
                tactyk_emit__push_codeblock(emitctx, false);
                continue;
            }
            else if (dbcode->next != NULL) {
                dbcode = dbcode->next;
                continue;
            }
            else {
                do {
                    dbstack_index -= 1;
                    if (dbstack_index == -1) {
                        return;
                    }
                    tactyk_emit__pop_codeblock(emitctx);
                    dbcode = dbstack[dbstack_index]->next;
                } while (dbcode == NULL);
                goto handle_dbcode;
            }
        }
    }
}

void tactyk_pl__resolve_aliased_tokens(struct tactyk_pl__Context *plctx, struct tactyk_dblock__DBlock *dbcode) {
    tactyk_report__reset();
    tactyk_report__msg("REWRITE");
    struct tactyk_dblock__DBlock *dbstack[256];
    int64_t dbstack_index = 0;
    dbstack[0] = dbcode;
    bool escape_block = false;
    while (dbcode != NULL) {
        struct tactyk_dblock__DBlock *token = dbcode->token;
        while (token != NULL) {
            if ( tactyk_dblock__contains_char(token, '$') ) {
                tactyk_report__dblock("Original line", dbcode);
                tactyk_report__indent(2);
                tactyk_report__dblock("Original token", token);
                struct tactyk_dblock__DBlock *ntoken = tactyk_dblock__interpolate(token, '$', plctx->alias_chars, plctx->alias_table, NULL);
                tactyk_dblock__set_content(token, ntoken);
                tactyk_report__dblock("Rewritten token", ntoken);
                tactyk_report__indent(-2);
            }
            token = token->next;
        }
        if (dbcode->child != NULL) {
            dbstack[dbstack_index] = dbcode;
            dbstack_index += 1;
            dbcode = dbcode->child;
            continue;
        }
        else if (dbcode->next != NULL) {
            dbcode = dbcode->next;
            continue;
        }
        else {
            do {
                dbstack_index -= 1;
                if (dbstack_index == -1) {
                    return;
                }
                dbcode = dbstack[dbstack_index]->next;
            } while (dbcode == NULL);
        }
    }
}


void tactyk_pl__add_script_command(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *token, struct tactyk_dblock__DBlock *line) {
    struct tactyk_dblock__DBlock *_token = token;
    struct tactyk_dblock__DBlock *_prev = NULL;
    while (_token != NULL) {
        if (tactyk_dblock__getchar(_token, 0) == '^') {
            goto read_bus_tokens;
        }
        _prev = _token;
        _token = _token->next;
    }
    tactyk_emit__add_script_command(ctx->emitctx, token, line);
    return;
    
    struct tactyk_dblock__DBlock *bus_tokens[256];
    struct tactyk_dblock__DBlock *bus_cmd = NULL;
    struct tactyk_dblock__DBlock *bus_token = NULL;
    
    read_bus_tokens: {
        bus_token = _token;
        memset(bus_tokens, 0, sizeof(bus_tokens));
        // iterate the chars of all subsequent tokens
        char txt[1024];
        uint64_t pos = 1;
        uint64_t busidx = 0;
        uint64_t cmdidx = 0;
        bool put_command;
        while (bus_token != NULL) {
            tactyk_dblock__export_cstring(txt, 1024, bus_token);
            while (pos < 1024) {
                char c = txt[pos];
                switch(txt[pos]) {
                    
                    // end of token
                    case 0: {
                        if (put_command) {
                            put_command = false;
                            bus_tokens[cmdidx] = ctx->bus_tokens[busidx];
                            busidx += 1;
                            cmdidx = 0;
                        }
                        goto read_bus_tokens__next;
                    }
                    
                    // back up [to allow a token to be referenced multiple times]
                    case ',': {
                        if (put_command) {
                            put_command = false;
                            bus_tokens[cmdidx] = ctx->bus_tokens[busidx];
                        }
                        if (busidx > 0) {
                            busidx -= 1;
                        }
                        cmdidx = 0;
                        break;
                    }
                    
                    // attach any trailing tokens to the token preceding the bus-tokens.
                    case '^': {
                        if (put_command) {
                            put_command = false;
                            bus_tokens[cmdidx] = ctx->bus_tokens[busidx];
                        }
                        goto insert_bus_tokens;
                    }
                    
                    // ignored chars
                    case '<': case '>':
                    case '{': case '}':
                    case '[': case ']':
                    case '(': case ')':
                    case ' ': {
                        if (put_command) {
                            bus_tokens[cmdidx] = ctx->bus_tokens[busidx];
                            busidx += 1;
                            cmdidx = 0;
                        }
                        break;
                    }
                    default: {
                        int64_t iv = (uint64_t) (c - '0');
                        if ( (iv >= 0) && (iv <= 9) ) {
                            put_command = true;
                            cmdidx *= 10;
                            cmdidx += iv;
                        }
                        else {
                            if (put_command) {
                                put_command = false;
                                bus_tokens[cmdidx] = ctx->bus_tokens[busidx];
                            }
                            // treat anything else as a NULL bus entry
                            busidx += 1;
                            cmdidx = 0;
                        }
                    }
                }
                if (busidx >= 256) {
                    busidx = 255;
                }
                pos += 1;
            }
            read_bus_tokens__next:
            pos = 0;
            bus_token = bus_token->next;
        }
        // if the bus token list does not terminate, then terminate the input command at the preceding token.
        _prev->next = NULL;
    }
    
    insert_bus_tokens:{
        _prev->next = bus_token->next;
        uint64_t rc_pos = 1;
        struct tactyk_dblock__DBlock *__token = token->next;
        bus_cmd = bus_tokens[0];
        if (bus_cmd == NULL) {
            bus_cmd = token;
        }
        
        printf("BUS RESULT: ");
        tactyk_dblock__print(bus_cmd);
        printf(" ");
        
        struct tactyk_dblock__DBlock *bc_token = tactyk_dblock__new(16);
        tactyk_dblock__set_content(bc_token,bus_cmd);
        bus_cmd = bc_token;
        struct tactyk_dblock__DBlock *buscmd_end = bus_cmd;
        while (true) {
            if (bus_tokens[rc_pos] != NULL) {
                bc_token = tactyk_dblock__new(16);
                tactyk_dblock__set_content(bc_token, bus_tokens[rc_pos]);
                buscmd_end->next = bc_token;
                buscmd_end = bc_token;
                tactyk_dblock__print(bc_token);
                printf(" ");
            }
            else if (__token != NULL) {
                bc_token = tactyk_dblock__new(16);
                tactyk_dblock__set_content(bc_token, __token);
                buscmd_end->next = bc_token;
                buscmd_end = bc_token;
                __token = __token->next;
                tactyk_dblock__print(bc_token);
                printf(" ");
            }
            else {
                break;
            }
            rc_pos += 1;
        }
    }
    printf("\n");
    tactyk_emit__add_script_command(ctx->emitctx, bus_cmd, line);
}

bool tactyk_pl__bus(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    uint64_t idx = 0;
    struct tactyk_dblock__DBlock *token = dblock->token->next;
    memset(ctx->bus_tokens, 0, sizeof(ctx->bus_tokens));
    while (token != NULL) {
        ctx->bus_tokens[idx] = token;
        idx += 1;
        token = token->next;
        if (idx == 256) {
            break;
        }
    }
}

struct tactyk_asmvm__Program* tactyk_pl__build(struct tactyk_pl__Context *plctx) {
    tactyk_emit__compile(plctx->emitctx);
    struct tactyk_asmvm__Program *program = plctx->program;
    tactyk_dblock__cull(0);
    tactyk_alloc__free(plctx);
    return program;
}


bool tactyk_pl__get(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_dblock__DBlock *arg = dblock->token->next;
    struct tactyk_dblock__DBlock *name = tactyk_dblock__last_peer(arg);
    struct tactyk_dblock__DBlock *templ = tactyk_dblock__get(ctx->getters, name);
    templ = tactyk_dblock__deep_copy(templ);
    struct tactyk_dblock__DBlock *tok = templ->token;
    while (tok != NULL) {
        if ( (tok->length == 1) && ( ((char*)tok->data)[0] == '?') ) {
            if (arg == NULL) {
                error("EMIT -- Not enough arguments to fill getter template", dblock);
            }
            tactyk_dblock__set_content(tok, arg);
            arg = arg->next;
        }
        tok = tok->next;
    }
    if (templ == NULL) {
        error("EMIT -- no getter template", name);
    }

    tactyk_emit__add_script_command(ctx->emitctx, templ->token, dblock);

    return true;
}
bool tactyk_pl__set(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_dblock__DBlock *name = dblock->token->next;
    struct tactyk_dblock__DBlock *arg = name->next;
    struct tactyk_dblock__DBlock *templ = tactyk_dblock__get(ctx->setters, name);
    templ = tactyk_dblock__deep_copy(templ);
    struct tactyk_dblock__DBlock *tok = templ->token;
    while (tok != NULL) {
        if ( (tok->length == 1) && ( ((char*)tok->data)[0] == '?') ) {
            if (arg == NULL) {
                error("EMIT -- Not enough arguments to fill setter template", dblock);
            }
            tactyk_dblock__set_content(tok, arg);
            arg = arg->next;
        }
        tok = tok->next;
    }
    if (templ == NULL) {
        error("EMIT -- no setter template", name);
    }

    tactyk_emit__add_script_command(ctx->emitctx, templ->token, dblock);
    return true;
}
bool tactyk_pl__var(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_dblock__DBlock *name = dblock->token->next;
    struct tactyk_dblock__DBlock *def = dblock->child;

    while (def != NULL) {
        struct tactyk_dblock__DBlock *def_tokens = def->child;
        if (strncmp("get", (char*)def->data, 3) == 0) {
            tactyk_dblock__put(ctx->getters, name, def_tokens);
        }
        else if (strncmp("set", (char*)def->data, 3) == 0) {
            tactyk_dblock__put(ctx->setters, name, def_tokens);
        }
        def = def->next;
    }
    return true;
}

//
void tactyk_pl__define_mem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock, struct tactyk_asmvm__memblock_lowlevel **m_ll,  struct tactyk_asmvm__memblock_highlevel **m_hl) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    if ( (dblock->token == NULL) || (dblock->token->next == NULL) ) {
        tactyk_report__msg("Struct specifier is not present");
        error(NULL, NULL);
    }
    
    tactyk_report__msg("  Definition");

    struct tactyk_asmvm__struct *layout = NULL;
    struct tactyk_dblock__DBlock *mem_name = NULL;
    struct tactyk_dblock__DBlock *st_name = NULL;
    uint64_t scale = 0;

    switch(tactyk_dblock__count_tokens(dblock)) {
        case 3: {
            mem_name = dblock->token->next;
            st_name = dblock->token->next;
            struct tactyk_dblock__DBlock *tok = dblock->token->next->next;
            if (!tactyk_dblock__try_parseuint(&scale, tok)) {
                tactyk_report__dblock("    Invalid scale", tok);
                error(NULL, NULL);
            }

            break;
        }
        case 4: {
            mem_name = dblock->token->next;
            st_name = dblock->token->next->next;
            layout = tactyk_dblock__get(ctx->struct_table, st_name);
            struct tactyk_dblock__DBlock *tok = dblock->token->next->next->next;
            if (!tactyk_dblock__try_parseuint(&scale, tok)) {
                tactyk_report__dblock("    Invalid scale", tok);
                error(NULL, NULL);
            }
            break;
        }
        case 2: {
            mem_name = dblock->token->next;
            st_name = dblock->token->next;
            scale = 0;
            break;
        }
        case 0:
        case 1: {
            tactyk_report__msg("    Invalid definition");
            error(NULL, NULL);
            break;
        }
    }
    
    tactyk_report__dblock("    Memblock name", mem_name);
    tactyk_report__dblock("    Struct name", st_name);
    
    
    if (layout == NULL) {
        struct tactyk_dblock__DBlock *layout_ctn = tactyk_dblock__get(ctx->struct_table, mem_name);
        if (layout_ctn != NULL) {
            layout = (struct tactyk_asmvm__struct*) layout_ctn->data;
        }
        else {
            layout = &default_mem_layout;
        }
    }
    // almost retained the old scheme for maintaining a contiguous list of structs
    //      (then rememebered that dblock-container covers that case as well)

    int64_t id = ctx->memspec_highlevel_table->element_count;
    
    tactyk_report__uint("    memblock identifier", id);
    
    assert(ctx->memspec_lowlevel_buffer->element_count == ctx->memspec_highlevel_table->element_count);

    *m_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(ctx->memspec_lowlevel_buffer);
    *m_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(ctx->memspec_highlevel_table, mem_name);
    
    tactyk_report__ptr("    ref [LL]", m_ll);
    tactyk_report__ptr("    ref [HL]", m_hl);
    
    tactyk_report__uint("    size", scale * layout->byte_stride);
    
    tactyk_report__uint("    object count", scale);
    tactyk_report__uint("    element bound", layout->byte_stride);
    tactyk_report__int("    array bound", (scale-1) * layout->byte_stride + 1);

    struct tactyk_asmvm__memblock_lowlevel *mem_ll = *m_ll;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = *m_hl;

    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = scale;
    mem_hl->memblock_id = id;
    mem_hl->data = NULL;
    mem_hl->definition = layout;

    mem_ll->array_bound = (scale-1) * layout->byte_stride + 1;
    mem_ll->element_bound = layout->byte_stride;
    mem_ll->memblock_index = id;
    mem_ll->offset = 0;
    mem_ll->base_address = NULL;
    

    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(ectx->memblock_table, mem_name, memid);
}


bool tactyk_pl__mem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {

    struct tactyk_asmvm__memblock_lowlevel *m_ll;
    struct tactyk_asmvm__memblock_highlevel *m_hl;
    
    tactyk_report__reset();
    tactyk_report__msg("ALLOCATE MEMBLOCK");
    
    tactyk_pl__define_mem(ctx, dblock, &m_ll, &m_hl);

    m_ll->offset = 0;
    m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__ALLOC;


    m_hl->data = tactyk_alloc__allocate(m_hl->num_entries, m_hl->definition->byte_stride);
    tactyk_report__ptr("data-ref", m_hl->data);

    m_ll->base_address = m_hl->data;
    
    if (m_hl->definition == &default_mem_layout) {
        tactyk_report__msg("No defined layout (using default)");
        warn(NULL, NULL);
    }
    
    return true;
}

// define external memory
// This differs from the "internal" memory specification in that it does not allocate a block of memory.
// It is expected that the host application will allocate the memory
//      the host application is also free to set the memory specification as needed
//      (all bounds checking is performed at runtime)
bool tactyk_pl__extmem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {

    tactyk_report__reset();
    tactyk_report__msg("EXTERNAL MEMBLOCK");
    
    struct tactyk_asmvm__memblock_lowlevel *m_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *m_hl = NULL;

    tactyk_pl__define_mem(ctx, dblock, &m_ll, &m_hl);

    m_ll->offset = TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL;
    m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL;
    
    return true;
}

bool tactyk_pl__text(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;

    tactyk_report__reset();
    tactyk_report__msg("TEXT MEMBLOCK");
    tactyk_report__dblock_full("code", dblock);
    
    if ( (dblock->token == NULL) || (dblock->token->next == NULL) ) {
        tactyk_report__msg("Name not specified");
        error(NULL, NULL);
    }

    struct tactyk_dblock__DBlock *name = dblock->token->next;

    
    tactyk_report__msg("Memblock Definition");
    
    int64_t id = ctx->memspec_highlevel_table->element_count;

    tactyk_report__uint("  identifier", id);
    
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(ctx->memspec_lowlevel_buffer);
    struct tactyk_asmvm__memblock_highlevel *mem_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(ctx->memspec_highlevel_table, name);

    tactyk_report__ptr("  ref [LL]", mem_ll);
    tactyk_report__ptr("  ref [HL]", mem_hl);
    
    mem_ll->offset = 0;
    mem_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;

    struct tactyk_dblock__DBlock *tbuf = tactyk_dblock__new(1024);
    struct tactyk_dblock__DBlock *line = dblock->child;
    if (line != NULL) {
        while (true) {
            tactyk_dblock__append(tbuf, line);
            line = line->next;
            if (line == NULL) {
                break;
            }
            tactyk_dblock__append_char(tbuf, '\n');
        }
    }
    uint64_t len = tbuf->length;
    for (uint64_t i = 0; i < 8; i++) {
        tactyk_dblock__append_char(tbuf, 0);
    }
    uint8_t *data = tactyk_dblock__release(tbuf);
    tactyk_dblock__dispose(tbuf);

    
    tactyk_report__uint("  size", len);
    
    tactyk_report__uint("  object count", 1);
    tactyk_report__uint("  element bound", len);
    tactyk_report__int("  array bound", 1);
    
    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = 1;
    mem_hl->memblock_id = id;
    mem_hl->data = data;

    mem_ll->array_bound = 1;
    mem_ll->element_bound = len;
    mem_ll->memblock_index = id;
    mem_ll->offset = 0;
    mem_ll->base_address = data;


    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(ectx->memblock_table, name, memid);

    {
        char struct_sz_name[1040];
        char struct_raw_name[1024];
        tactyk_dblock__export_cstring(struct_raw_name, 1024, name);
        snprintf(struct_sz_name, 1040, "%s_size", struct_raw_name);
        tactyk_report__string("  size identifier", struct_sz_name);

        struct tactyk_dblock__DBlock *struct_sz_const = tactyk_dblock__from_int(len);
        tactyk_dblock__put(ectx->const_table, struct_sz_name, struct_sz_const);
    }
    
    return true;
}

bool tactyk_pl__flatdata(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;

    tactyk_report__reset();
    tactyk_report__msg("FLAT-DATA MEMBLOCK");
    tactyk_report__dblock_full("code", dblock);
    
    struct tactyk_dblock__DBlock *name = dblock->token->next;
    struct tactyk_dblock__DBlock *type = dblock->token->next->next;
    
    uint64_t nbytes = 0;
    bool isfloat = false;
    if (type == NULL) {
        nbytes = 1;
    }
    else if (!tactyk_dblock__try_parseuint(&nbytes, type)) {
        if (tactyk_dblock__equals_c_string(type, "byte")) {
            nbytes = 1;
        }
        else if (tactyk_dblock__equals_c_string(type, "word")) {
            nbytes = 2;
        }
        else if (tactyk_dblock__equals_c_string(type, "dword")) {
            nbytes = 4;
        }
        else if (tactyk_dblock__equals_c_string(type, "qword")) {
            nbytes = 8;
        }
        else if (tactyk_dblock__equals_c_string(type, "float32")) {
            isfloat = true;
            nbytes = 4;
        }
        else if (tactyk_dblock__equals_c_string(type, "float64")) {
            isfloat = true;
            nbytes = 8;
        }
        else {
            nbytes = 1;
        }
    }

    uint64_t item_count = 0;
    struct tactyk_dblock__DBlock *line = dblock->child;
    while (line != NULL) {
        item_count += tactyk_dblock__count_tokens(line);
        line = line->next;
    }

    uint8_t *data = tactyk_alloc__allocate(item_count, nbytes);
    tactyk_report__uint("items", item_count);
    tactyk_report__uint("size(bytes)", item_count*nbytes);
    tactyk_report__ptr("ref", data);
    
    uint64_t wbytes = nbytes;
    if (wbytes > 8) {
        wbytes = 8;
    }

    if (isfloat) {
        if (nbytes <= 3) {
            tactyk_report__uint("Invalid [floating point format] size", nbytes);
        }
        else if (nbytes <= 7) {
            uint64_t pos = 0;
            line = dblock->child;
            while (line != NULL) {
                struct tactyk_dblock__DBlock *token = line->token;
                while (token != NULL) {
                    double f64val = 0;
                    if (tactyk_dblock__try_parsedouble(&f64val, token)) {
                        double f32val = (float)f64val;
                        memcpy(&data[pos], &f32val, 4);
                    }
                    else {
                        tactyk_report__dblock("Not a floating point number", token);
                        warn(NULL, NULL);
                    }

                    pos += nbytes;
                    token = token->next;
                }
                line = line->next;
            }
        }
        else {
            uint64_t pos = 0;
            line = dblock->child;
            while (line != NULL) {
                struct tactyk_dblock__DBlock *token = line->token;
                while (token != NULL) {
                    double f64val = 0;
                    if (tactyk_dblock__try_parsedouble(&f64val, token)) {
                        memcpy(&data[pos], &f64val, 8);
                    }
                    else {
                        tactyk_report__dblock("Not a floating point number", token);
                        warn(NULL, NULL);
                    }
                    pos += nbytes;
                    token = token->next;
                }
                line = line->next;
            }
        }
    }
    else {
        uint64_t pos = 0;
        line = dblock->child;
        while (line != NULL) {
            struct tactyk_dblock__DBlock *token = line->token;
            while (token != NULL) {
                int64_t ival = 0;
                if (tactyk_dblock__try_parseint(&ival, token)) {
                    memcpy(&data[pos], &ival, wbytes);
                }
                else {
                    tactyk_report__dblock("Not a n integerr", token);
                    warn(NULL, NULL);
                }

                pos += nbytes;
                token = token->next;
            }
            line = line->next;
        }
    }

    struct tactyk_asmvm__struct *layout = calloc(1, sizeof(struct tactyk_asmvm__struct));
    layout->byte_stride = item_count * nbytes;
    layout->num_properties = 1;
    layout->properties = calloc(1, sizeof(struct tactyk_asmvm__property));
    layout->properties->byte_offset = 0;
    layout->properties->byte_width = item_count * nbytes;

    int64_t id = ctx->memspec_highlevel_table->element_count;
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(ctx->memspec_lowlevel_buffer);
    struct tactyk_asmvm__memblock_highlevel *mem_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(ctx->memspec_highlevel_table, name);

    tactyk_report__msg("Memblock Definition");
    tactyk_report__uint("  identifier", id);
    tactyk_report__ptr("  ref [LL]", mem_ll);
    tactyk_report__ptr("  ref [HL]", mem_hl);
    tactyk_report__uint("  size", layout->byte_stride);
    tactyk_report__uint("  object count", 1);
    tactyk_report__uint("  element bound", layout->byte_stride);
    tactyk_report__int("  array bound", 1);
    
    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = 1;
    mem_hl->memblock_id = id;
    mem_hl->data = data;
    mem_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;
    mem_hl->definition = layout;

    mem_ll->array_bound = 1;
    mem_ll->element_bound = layout->byte_stride;
    mem_ll->memblock_index = id;
    mem_ll->offset = 0;
    mem_ll->base_address = data;

    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(ectx->memblock_table, name, memid);
    
    return true;
}

bool tactyk_pl__data(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    tactyk_report__reset();
    tactyk_report__msg("TACTYK-PL -- DATA");
    tactyk_report__dblock_full("code", dblock);
    
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    
    struct tactyk_asmvm__memblock_lowlevel *m_ll;
    struct tactyk_asmvm__memblock_highlevel *m_hl;
    
    tactyk_pl__define_mem(ctx, dblock, &m_ll, &m_hl);
    
    m_ll->offset = 0;
    m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;
    
    struct tactyk_asmvm__struct *layout = m_hl->definition;
    
    uint64_t lpos = 0;
    uint64_t dpos = 0;
    uint64_t elem = 0;
    struct tactyk_dblock__DBlock *line = dblock->child;
    uint64_t max_elements = layout->num_properties * m_hl->num_entries;
    
    uint8_t *data = tactyk_alloc__allocate(m_hl->num_entries, layout->byte_stride);
    
    m_hl->data = data;
    m_ll->base_address = data;
    
    while (line != NULL) {
        struct tactyk_dblock__DBlock *token = line->token;
        while (token != NULL) {
            struct tactyk_asmvm__property *prop = &layout->properties[lpos];
            
            int64_t i64val = 0;
            double f64val = 0;
            bool is_int_valid = tactyk_dblock__try_parseint(&i64val, token);
            bool is_float_valid = tactyk_dblock__try_parsedouble(&f64val, token);
            
            if (is_int_valid) {
                uint64_t nbytes = prop->byte_width;
                if (nbytes > 8) {
                    nbytes = 8;
                }
                tactyk_report__int("  integer", i64val & (0xffffffffffffffff >> (64-nbytes*8)));
                memcpy(&data[dpos], &i64val, nbytes);
                //for (int64_t i = 0; i < nbytes; i++) {
                //    uint64_t d = dpos;
                //    data[d] = (uint8_t)ui64val;
                //    uival >>= 8;
                //    d += 1;
                //}
            }
            else if (is_float_valid) {
                if (prop->byte_width <= 3) {
                    tactyk_report__uint("Invalid floating point size", prop->byte_width);
                    error(NULL, NULL);
                }
                else if (prop->byte_width <= 7) {
                    float f32val = (float)f64val;
                    tactyk_report__float32("  float32", f32val);
                    memcpy(&data[dpos], &f32val, 4);
                }
                else {
                    tactyk_report__float64("  float64", f64val);
                    memcpy(&data[dpos], &f64val, 8);
                }
            }
            
            dpos += prop->byte_width;
            lpos = (lpos + 1) % layout->num_properties;
            elem += 1;
            token = token->next;
            
            if (elem >= max_elements) {
                if ( (token != NULL) || (line->next != NULL) ) {
                    tactyk_report__msg("Truncated due to size constraint");
                    warn(NULL, NULL);
                }
                return true;
            }
        }
        line = line->next;
    }
    return true;
}

//void resolve_instruction_struct(struct unprocessed_command *ucmd) {
//void tactyk_pl__struct(struct tactyk_emit__Context *emitctx, struct tactyk_pl__thing *__tokens) {
bool tactyk_pl__struct(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    tactyk_report__reset();
    tactyk_report__msg("TACTYK-PL -- STRUCT");
    tactyk_report__dblock_full("Code", dblock);
    
    if ( (dblock->token == NULL) || (dblock->token->next == NULL) ) {
        tactyk_report__msg("No name specified");
        error(NULL, NULL);
    }

    struct tactyk_dblock__DBlock *st_name = dblock->token->next;
    
    struct tactyk_asmvm__struct *st = (struct tactyk_asmvm__struct*) tactyk_dblock__new_managedobject(ctx->struct_table, st_name);
    
    tactyk_dblock__export_cstring(st->name, TACTYK__MAX_IDENTIFIER_LENGTH, st_name);
    st->num_properties = tactyk_dblock__count_children(dblock);
    st->properties = calloc(st->num_properties, sizeof(struct tactyk_asmvm__property));
    
    tactyk_report__uint("Field Count", st->num_properties);
    
    uint64_t offset = 0;
    uint64_t prev_offset = 0;
    uint64_t stride = 0;

    dblock = dblock->child;
    uint64_t property_index = 0;

    char directive = ' ';
    struct tactyk_dblock__DBlock *p_name = NULL;
    uint64_t p_width;

    while (dblock != NULL) {
        tactyk_report__dblock("FIELD", dblock);
        int64_t num_tokens = tactyk_dblock__count_tokens(dblock);
        switch(num_tokens) {
            case 2: {
                directive = ' ';
                p_name = dblock->token->next;
                if (!tactyk_dblock__try_parseuint(&p_width, dblock->token)) {
                    tactyk_report__msg("  Invalid field width");
                    error(NULL, NULL);
                }
                break;
            }
            case 3: {
                uint8_t *data = (uint8_t*)dblock->token->data;
                directive = data[0];
                p_name = dblock->token->next->next;
                if (!tactyk_dblock__try_parseuint(&p_width, dblock->token->next)) {
                    tactyk_report__msg("  Invalid field width");
                    error(NULL, NULL);
                }
                break;
            }
            case 0:
            case 1:
            default: {
                tactyk_report__int("  Invalid token count", num_tokens);
                error(NULL, NULL);
                break;
            }
        }

        assert(property_index < st->num_properties);
        struct tactyk_asmvm__property *prop = &st->properties[property_index];
        property_index += 1;

        tactyk_dblock__export_cstring(prop->name, TACTYK__MAX_IDENTIFIER_LENGTH, p_name);
        //char *prop_name = tactyk_dblock__export_cstring(p_name);

        //prop->name = prop_name;
        prop->byte_width = p_width;
        prop->byte_offset = offset;
        
        char prop_qname[1024];
        sprintf(prop_qname, "%s.%s", st->name, prop->name);
        
        tactyk_report__string("  Identifier", prop_qname);
        tactyk_report__int("  Width", p_width);
        
        switch(directive) {
            case '.': {
                offset = prev_offset;
                prop->byte_offset = prev_offset;
                if ( (offset + p_width) > stride) {
                    stride = (offset + p_width);
                }
                offset += p_width;
                break;
            }
            case '>': {
                prev_offset = 0;
                prop->byte_offset = 0;
                if ( (offset + p_width) > stride) {
                    stride = (offset + p_width);
                }
                offset = p_width;
                break;
            }
            default: {
                prev_offset = offset;
                offset += p_width;
                if ( offset > stride) {
                    stride = offset;
                }
                break;
            }
        }
        tactyk_report__int("  Offset", prop->byte_offset);
        
        struct tactyk_dblock__DBlock *propid = tactyk_dblock__from_int(prop->byte_offset);
        tactyk_dblock__put(ectx->const_table, prop_qname, propid);

        dblock = dblock->next;
    }
    {
        char struct_sz_name[1024];
        sprintf(struct_sz_name, "%s_size", st->name);
        st->byte_stride = stride;

        struct tactyk_dblock__DBlock *struct_sz_const = tactyk_dblock__from_int(stride);
        tactyk_dblock__put(ectx->const_table, struct_sz_name, struct_sz_const);
        tactyk_report__msg("SIZE");
        tactyk_report__string("  Identifier", struct_sz_name);
        tactyk_report__uint("  size", stride);
    }
    return true;
}

bool tactyk_pl__const(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    tactyk_report__reset();
    tactyk_report__msg("TACTYK-PL -- CONST");
    tactyk_report__dblock("Code", dblock);
    if ( (dblock->token == NULL) || (dblock->token->next == NULL) || (dblock->token->next->next == NULL) ) {
        tactyk_report__msg("Constant definition is invalid.");
        error(NULL, NULL);
    }

    struct tactyk_dblock__DBlock *name = dblock->token->next;
    struct tactyk_dblock__DBlock *value = dblock->token->next->next;
    
    tactyk_report__dblock("Name", name);
    tactyk_report__dblock("Value", value);
    
    union tactyk_util__float_int v;
    if (tactyk_dblock__try_parseint(&v.ival, value)) {
        tactyk_report__msg("Type: Integer");
        tactyk_dblock__put(ectx->const_table, name, value);
        return true;
    }
    if (tactyk_dblock__try_parsedouble(&v.fval, value)) {
        tactyk_report__msg("Type: float-64");
        value = tactyk_dblock__from_uint(v.ival);
        tactyk_dblock__put(ectx->fconst_table, name, value);
        return true;
    }
    return true;
}
void tactyk_pl__ld_constants__impl(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *name) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    struct tactyk_dblock__DBlock *ctable = tactyk_dblock__get(ctx->constant_sets, name);
    if (ctable == NULL) {
        error("PL -- Invalid constant table name", name);
    }
    struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**) ctable->data;
    for (uint64_t i = 0; i < ctable->element_capacity; i += 1) {
        uint64_t ofs = i*2;
        struct tactyk_dblock__DBlock *key = fields[ofs];
        struct tactyk_dblock__DBlock *value = fields[ofs+1];
        if ( (key != NULL) && (value != NULL) && (value != TACTYK_PSEUDONULL) ) {
            tactyk_dblock__put(ectx->const_table, key, value);
        }
    }
}

bool tactyk_pl__ld_constants(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_dblock__DBlock *name = dblock->token->next;
    tactyk_pl__ld_constants__impl(ctx, name);
    return true;
}
bool tactyk_pl__ld_visa_constants(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_dblock__DBlock *name = tactyk_dblock__from_safe_c_string(".VISA");
    tactyk_pl__ld_constants__impl(ctx, name);
    return true;
}
