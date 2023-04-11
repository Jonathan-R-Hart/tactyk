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

#include "tactyk_dblock.h"

typedef bool (*tactyk_pl__func)(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock);
struct tactyk_dblock__DBlock *tkpl_funcs;
struct tactyk_asmvm__struct default_mem_layout;

void tactyk_pl__init() {
    tkpl_funcs = tactyk_dblock__new_table(16);

    tactyk_dblock__put(tkpl_funcs, "mem", tactyk_pl__mem);
    tactyk_dblock__put(tkpl_funcs, "struct", tactyk_pl__struct);
    tactyk_dblock__put(tkpl_funcs, "extmem", tactyk_pl__extmem);
    tactyk_dblock__put(tkpl_funcs, "data", tactyk_pl__data);
    tactyk_dblock__put(tkpl_funcs, "text", tactyk_pl__text);
    tactyk_dblock__put(tkpl_funcs, "const", tactyk_pl__const);
    tactyk_dblock__put(tkpl_funcs, "var", tactyk_pl__var);
    tactyk_dblock__put(tkpl_funcs, "get", tactyk_pl__get);
    tactyk_dblock__put(tkpl_funcs, "set", tactyk_pl__set);
    tactyk_dblock__put(tkpl_funcs, "add_vconstants", tactyk_pl__ld_visa_constants);

    default_mem_layout.byte_stride = 8;
    strcpy(default_mem_layout.name, "default-layout");
    default_mem_layout.num_properties = 1;
    default_mem_layout.properties = calloc(1, sizeof(struct tactyk_asmvm__property));
    default_mem_layout.properties->byte_offset = 0;
    default_mem_layout.properties->byte_width = 8;
    strncpy(default_mem_layout.properties->name, "item", MAX_IDENTIFIER_LENGTH);
}

struct tactyk_pl__Context *tactyk_pl__new(struct tactyk_emit__Context *emitctx) {
    tactyk_emit__reset(emitctx);
    struct tactyk_pl__Context *ctx = calloc(1, sizeof(struct tactyk_pl__Context));
    ctx->program = calloc(1, sizeof(struct tactyk_asmvm__Program));
    emitctx->program = ctx->program;
    ctx->emitctx = emitctx;

    ctx->struct_table = tactyk_dblock__new_managedobject_table(64, sizeof(struct tactyk_asmvm__struct));
    ctx->getters = tactyk_dblock__new_table(64);
    ctx->setters = tactyk_dblock__new_table(64);

    {
        uint64_t ln = TACTYK_ASMVM__MEMBLOCK_CAPACITY;
        ctx->memspec_lowlevel_buffer = tactyk_dblock__new_container(ln, sizeof(struct tactyk_asmvm__memblock_lowlevel));
        tactyk_dblock__fix(ctx->memspec_lowlevel_buffer);
        tactyk_dblock__make_pseudosafe(ctx->memspec_lowlevel_buffer);
        ctx->memspec_highlevel_table = tactyk_dblock__new_managedobject_table(ln, sizeof(struct tactyk_asmvm__memblock_highlevel));
    }

    // the struct table gets to survive garbage collection!  barely ... (the only real purpose of it is reflection)
    tactyk_dblock__set_persistence_code(ctx->struct_table, 2);
    //tactyk_dblock__set_persistence_code(ctx.getters, 2);
    //tactyk_dblock__set_persistence_code(ctx.setters, 2);
    tactyk_dblock__set_persistence_code(ctx->memspec_lowlevel_buffer, 2);
    tactyk_dblock__set_persistence_code(ctx->memspec_highlevel_table, 2);

    ctx->program->memory_layout_hl = ctx->memspec_highlevel_table;
    ctx->program->memory_layout_ll = ctx->memspec_lowlevel_buffer;

    tactyk_emit__init_program(emitctx);
    return ctx;
}

void tactyk_pl__load(struct tactyk_pl__Context *plctx, char *code) {
    struct tactyk_emit__Context *emitctx = plctx->emitctx;
    struct tactyk_dblock__DBlock *dbcode = tactyk_dblock__from_safe_c_string(code);

    tactyk_dblock__fix(dbcode);
    tactyk_dblock__tokenize(dbcode, '\n', false);
    dbcode = tactyk_dblock__remove_blanks(dbcode, ' ', '#');
    tactyk_dblock__stratify(dbcode, ' ');
    tactyk_dblock__trim(dbcode);
    tactyk_dblock__tokenize(dbcode, ' ', true);
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
                    tactyk_emit__add_script_command(emitctx, tok, dbcode);
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
                goto handle_dbcode;
            }
        }
    }
}

struct tactyk_asmvm__Program* tactyk_pl__build(struct tactyk_pl__Context *plctx) {
    tactyk_emit__compile(plctx->emitctx);
    struct tactyk_asmvm__Program *program = plctx->program;
    tactyk_dblock__cull(0);
    free(plctx);
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
        error("PL -- unnamed struct", dblock);
    }

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
                error("PL -- Invalid Integer", tok);
            }

            break;
        }
        case 4: {
            mem_name = dblock->token->next;
            st_name = dblock->token->next->next;
            layout = tactyk_dblock__get(ctx->struct_table, st_name);
            struct tactyk_dblock__DBlock *tok = dblock->token->next->next->next;
            if (!tactyk_dblock__try_parseuint(&scale, tok)) {
                error("PL -- Invalid Integer", tok);
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
            error("PL -- Invalid mem entry", dblock);
            break;
        }
    }

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

    assert(ctx->memspec_lowlevel_buffer->element_count == ctx->memspec_highlevel_table->element_count);

    *m_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(ctx->memspec_lowlevel_buffer);
    *m_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(ctx->memspec_highlevel_table, mem_name);

    struct tactyk_asmvm__memblock_lowlevel *mem_ll = *m_ll;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = *m_hl;

    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = scale;
    mem_hl->memblock_id = id;
    mem_hl->data = NULL;
    //m_hl->data = calloc(scale, layout->byte_stride);
    mem_hl->definition = layout;

    mem_ll->array_bound = (scale-1) * layout->byte_stride + 1;
    mem_ll->element_bound = layout->byte_stride - 7;
    mem_ll->memblock_index = id;
    mem_ll->type = 0;
    mem_ll->base_address = NULL;

    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(ectx->memblock_table, mem_name, memid);
}


bool tactyk_pl__mem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {

    struct tactyk_asmvm__memblock_lowlevel *m_ll;
    struct tactyk_asmvm__memblock_highlevel *m_hl;

    tactyk_pl__define_mem(ctx, dblock, &m_ll, &m_hl);

    m_ll->type = TACTYK_ASMVM__MEMBLOCK_TYPE__ALLOC;
    m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__ALLOC;

    m_hl->data = calloc(m_hl->num_entries, m_hl->definition->byte_stride);

    m_ll->base_address = m_hl->data;

    return true;
}

// define external memory
// This differs from the "internal" memory specification in that it does not allocate a block of memory.
// It is expected that the host application will allocate the memory
//      the host application is also free to set the memory specification as needed
//      (all bounds checking is performed at runtime)
bool tactyk_pl__extmem(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {

    struct tactyk_asmvm__memblock_lowlevel *m_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *m_hl = NULL;

    tactyk_pl__define_mem(ctx, dblock, &m_ll, &m_hl);

    m_ll->type = TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL;
    m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL;

    return true;
}

bool tactyk_pl__text(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;

    if ( (dblock->token == NULL) || (dblock->token->next == NULL) ) {
        error("PL -- unnamed struct", dblock);
    }

    struct tactyk_dblock__DBlock *name = dblock->token->next;

    int64_t id = ctx->memspec_highlevel_table->element_count;

    struct tactyk_asmvm__memblock_lowlevel *mem_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(ctx->memspec_lowlevel_buffer);
    struct tactyk_asmvm__memblock_highlevel *mem_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(ctx->memspec_highlevel_table, name);

    mem_ll->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;
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
    tactyk_dblock__fix(tbuf);

    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = 1;
    mem_hl->memblock_id = id;
    mem_hl->data = tbuf->data;

    mem_ll->array_bound = 1;
    mem_ll->element_bound = tbuf->length+8;
    mem_ll->memblock_index = id;
    mem_ll->type = 0;
    mem_ll->base_address = tbuf->data;

    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(ectx->memblock_table, name, memid);

    {
        char struct_sz_name[1040];
        char struct_raw_name[1024];
        tactyk_dblock__export_cstring(struct_raw_name, 1024, name);
        snprintf(struct_sz_name, 1040, "%s_size", struct_raw_name);

        struct tactyk_dblock__DBlock *struct_sz_const = tactyk_dblock__from_int(tbuf->length+8);
        tactyk_dblock__put(ectx->const_table, struct_sz_name, struct_sz_const);
    }

    return true;
}

//void tactyk_pl__data(struct tactyk_emit__Context *emitctx, struct tactyk_pl__thing *__tokens) {
bool tactyk_pl__data(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    //struct tactyk_emit__Context *ectx = ctx->emitctx;
    //struct tactyk_asmvm__memblock_lowlevel *m_ll;
    //struct tactyk_asmvm__memblock_highlevel *m_hl;

    //m_ll->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;
    //m_hl->type = TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC;
    error("PL -- data directive not implemented.", NULL);
    return true;
    /*
    struct tactyk_pl__thing *tok = &__tokens[0];

    if (tok->text[0] == 0) {
        fprintf(stderr, "ERROR:  name required for data block!\n");
        exit(1);
    }

    char *name = tactyk_textbuf__store(tkpl_sttext, tok->text);

    int64_t id = tactyk_pl__symdat->num_memblocks++;

    struct tactyk_asmvm__memblock *mb = &tactyk_pl__symdat->memlist[id];
    tactyk_table__add_strkey(tactyk_pl__symdat->memtbl, name, mb);

    tok = &__tokens[1];
    tactyk_pl__tokenize_block_simple(tok);

    int64_t num_items = MAX_BLOCK_TOKENS;
    for (int64_t i = 0; i < MAX_BLOCK_TOKENS; i++) {
        char *entry = tok->block_tokens[i];
        if ((entry == NULL) || (entry[0] == 0)) {
            num_items = i;
            break;
        }
    }

    // if a struct matching the data-block's name is defined, fetch it
    struct tactyk_asmvm__struct *def =  tactyk_table__get_strkey(tactyk_pl__symdat->structtbl, name);

    // if no such struct is defined, invent one.  Default structure is a flat array of 8-byte words large enough to fit the entire data block.
    if (def == NULL) {
        def = &tactyk_pl__symdat->structlist[tactyk_pl__symdat->num_structs++];
        tactyk_table__add_strkey(tactyk_pl__symdat->structtbl, name, def);

        def->num_properties = num_items;

        def->name = name;
        struct tactyk_asmvm__property *props = calloc(def->num_properties, sizeof(struct tactyk_asmvm__property));
        def->properties = props;

        // define generic properties (for now).
        for (int64_t i = 0; i < def->num_properties; i++) {
            //props[i].dtype = TXDTYPE_INT;
            props[i].byte_width = 8;
            props[i].byte_offset = i*8;
            props[i].name = "";
            //mb->data[i]
        }
        def->byte_stride = def->num_properties * 8;
        mb->num_entries = 1;
    }
    else {
        // calculate the width in "structs" of the contents of the data block.
        mb->num_entries = (num_items+(def->num_properties)-1)/def->num_properties+1;

        // if an empty data block, force a single entry.
        if (mb->num_entries == 0) {
            mb->num_entries = 1;
        }
    }

    // allocate the block
    mb->data = calloc(mb->num_entries, def->byte_stride);
    mb->definition = def;
    mb->memblock_id = next_memblock_id++;


    // parse tokens from the simply-tokenized block and copy them into the newly allocated memory.
    for (int64_t i = 0; i < MAX_BLOCK_TOKENS; i++) {
        char *item = tok->block_tokens[i];

        // if a terminal token, end.
        if ((item == NULL)  || (item[0] == 0)) {
            break;
        }
        // skip struct entries which are only for designating layout/spacing.
        //      This is represented in the input as '.'
        //      This is not presently represented part of the struct definition
        //      (would like it to be, but adding a symbol to denote which struct entries should be disregarded when parsing data blocks would make struct syntax complex for little gain)
        if ((item[0] == '.') && (item[1] == 0)) {
            continue;
        }
        // use information from the struct to determine where to put the item.
        int64_t entry_id = i/def->num_properties;
        int64_t prop_index = i%def->num_properties;
        struct tactyk_asmvm__property *prop = &def->properties[prop_index];
        int64_t nbytes = prop->byte_width;
        if (nbytes <= 0) {
            continue;
        }
        if (nbytes > 8) {
            nbytes = 8;
        }

        // reduce to parsed item and offset.
        int64_t ofs = prop->byte_offset;
        int64_t value = atoll(item);

        // write to memory.
        memcpy(
            &mb->data[(entry_id*def->byte_stride) + ofs],   // entry position + property offset
            (int8_t*)&value,                                // bytes of parsed value
            nbytes                                          // property width
        );
    }

    // Attach the to the program memory layout.
    struct tactyk_asmvm__memblock_spec *mspec = &tactyk_pl__prog->memory_layout[mb->memblock_id];
    mb->spec = mspec;
    mspec->base_address = mb->data;
    mspec->array_bound = 1;
    mspec->element_bound = def->byte_stride-7;


    // lastly, add an identifier for the data to the program symbol table.
    struct tactyk_asmvm__identifier *identifier = &tactyk_pl__symdat->memblock_idlist[tactyk_pl__symdat->num_identifiers++];
    strncpy(identifier->txt, name, MAX_IDENTIFIER_LENGTH);
    identifier->value = id;
    char _token[TACTYK_PL__RAW_TEXT_MAX_LENGTH];
    snprintf(_token, TACTYK_PL__RAW_TEXT_MAX_LENGTH-1, "%s", name );
    tactyk_table__add_strkey(tactyk_pl__symdat->memblock_idtbl, tactyk_textbuf__store(tkpl_sttext, _token), identifier);
    */
}

//void resolve_instruction_struct(struct unprocessed_command *ucmd) {
//void tactyk_pl__struct(struct tactyk_emit__Context *emitctx, struct tactyk_pl__thing *__tokens) {
bool tactyk_pl__struct(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;

    if ( (dblock->token == NULL) || (dblock->token->next == NULL) ) {
        tactyk_emit__error(ectx, "unnamed struct");
        exit(1);
    }

    struct tactyk_dblock__DBlock *st_name = dblock->token->next;

    struct tactyk_asmvm__struct *st = (struct tactyk_asmvm__struct*) tactyk_dblock__new_managedobject(ctx->struct_table, st_name);

    tactyk_dblock__export_cstring(st->name, MAX_IDENTIFIER_LENGTH, st_name);
    st->num_properties = tactyk_dblock__count_children(dblock);

    st->properties = calloc(st->num_properties, sizeof(struct tactyk_asmvm__property));

    uint64_t offset = 0;
    uint64_t stride = 0;

    dblock = dblock->child;
    uint64_t property_index = 0;

    char directive = ' ';
    struct tactyk_dblock__DBlock *p_name = NULL;
    uint64_t p_width;

    while (dblock != NULL) {
        switch(tactyk_dblock__count_tokens(dblock)) {
            case 2: {
                directive = ' ';
                p_name = dblock->token->next;
                if (!tactyk_dblock__try_parseuint(&p_width, dblock->token)) {
                    error("PL -- Invalid Integer", dblock->token);
                }
                break;
            }
            case 3: {
                uint8_t *data = (uint8_t*)dblock->token->data;
                directive = data[0];
                p_name = dblock->token->next->next;
                if (!tactyk_dblock__try_parseuint(&p_width, dblock->token->next)) {
                    error("PL -- Invalid Integer", dblock->token->next);
                }
                break;
            }
            case 0:
            case 1:
            default: {
                error("PL -- Invalid struct entry", dblock);
                break;
            }
        }

        assert(property_index < st->num_properties);
        struct tactyk_asmvm__property *prop = &st->properties[property_index];
        property_index += 1;

        tactyk_dblock__export_cstring(prop->name, MAX_IDENTIFIER_LENGTH, p_name);
        //char *prop_name = tactyk_dblock__export_cstring(p_name);

        //prop->name = prop_name;
        prop->byte_width = p_width;
        prop->byte_offset = offset;

        char prop_qname[1024];
        sprintf(prop_qname, "%s.%s", st->name, prop->name);

        struct tactyk_dblock__DBlock *propid = tactyk_dblock__from_int(offset);
        tactyk_dblock__put(ectx->const_table, prop_qname, propid);

        switch(directive) {
            case '.': {
                if ( (offset + p_width) > stride) {
                    stride = (offset + p_width);
                }
                break;
            }
            case '>': {
                if ( (offset + p_width) > stride) {
                    stride = (offset + p_width);
                }
                offset = 0;
                break;
            }
            default: {
                offset += p_width;
                if ( offset > stride) {
                    stride = offset;
                }
                break;
            }
        }

        dblock = dblock->next;
    }
    {
        char struct_sz_name[1024];
        sprintf(struct_sz_name, "%s_size", st->name);
        st->byte_stride = stride;

        struct tactyk_dblock__DBlock *struct_sz_const = tactyk_dblock__from_int(stride);
        tactyk_dblock__put(ectx->const_table, struct_sz_name, struct_sz_const);
    }
    return true;
}

bool tactyk_pl__const(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {

    struct tactyk_emit__Context *ectx = ctx->emitctx;

    if ( (dblock->token == NULL) || (dblock->token->next == NULL) || (dblock->token->next->next == NULL) ) {
        error("PL -- Invalid const", dblock);
    }

    struct tactyk_dblock__DBlock *name = dblock->token->next;
    struct tactyk_dblock__DBlock *value = dblock->token->next->next;

    union tactyk_util__float_int v;
    if (tactyk_dblock__try_parseint(&v.ival, value)) {
        tactyk_dblock__put(ectx->const_table, name, value);
        return true;
    }
    if (tactyk_dblock__try_parsedouble(&v.fval, value)) {
        value = tactyk_dblock__from_uint(v.ival);
        tactyk_dblock__put(ectx->fconst_table, name, value);
        return true;
    }
    return true;
}
bool tactyk_pl__ld_visa_constants(struct tactyk_pl__Context *ctx, struct tactyk_dblock__DBlock *dblock) {
    struct tactyk_emit__Context *ectx = ctx->emitctx;
    if (ectx->has_visa_constants == false) {
        ectx->has_visa_constants = true;
        struct tactyk_dblock__DBlock *ctable = ectx->visa_token_constants;
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
}
