#include <stdint.h>
#include <assert.h>

#include "tactyk.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"
#include "tactyk_emit_svc.h"
#include "tactyk_pl.h"

struct tactyk_pl__Context *tactyk_emit_svc__plctx;
struct tactyk_emit__Context *tactyk_emit_svc__emitctx;
struct tactyk_dblock__DBlock *cmd;
struct tactyk_dblock__DBlock *cmd_lasttoken;

struct tactyk_dblock__DBlock* tactyk_emit_svc__get_text(struct tactyk_asmvm__Context *asmvm_ctx, uint64_t slot);
struct tactyk_dblock__DBlock* tactyk_emit_svc__get_token(uint64_t handle);

void tactyk_emit_svc__declare_memblock(struct tactyk_asmvm__Context *asmvm_ctx, struct tactyk_asmvm__memblock_lowlevel **m_ll, struct tactyk_asmvm__memblock_highlevel **m_hl);
void tactyk_emit_svc__define_memblock(struct tactyk_asmvm__Context *asmvm_ctx, struct tactyk_asmvm__memblock_lowlevel **m_ll, struct tactyk_asmvm__memblock_highlevel **m_hl);

void tactyk_emit_svc__configure(struct tactyk_emit__Context *emit_context) {
    tactyk_emit_svc__emitctx = emit_context;
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-new", tactyk_emit_svc__new);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-build", tactyk_emit_svc__build);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-mem-external", tactyk_emit_svc__mem_external);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-mem", tactyk_emit_svc__mem_empty);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-mem-ref", tactyk_emit_svc__mem_ref);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-mem-copy", tactyk_emit_svc__mem_data);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-label", tactyk_emit_svc__label);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-intlabel", tactyk_emit_svc__intlabel);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-cmd", tactyk_emit_svc__cmd);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-1", tactyk_emit_svc__token1);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-2", tactyk_emit_svc__token2);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-3", tactyk_emit_svc__token3);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-4", tactyk_emit_svc__token4);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-5", tactyk_emit_svc__token5);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-token-6", tactyk_emit_svc__token6);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-text", tactyk_emit_svc__text);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-int", tactyk_emit_svc__int);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-float", tactyk_emit_svc__float);
    tactyk_emit__add_tactyk_apifunc(tactyk_emit_svc__emitctx, "emit-cmd-end", tactyk_emit_svc__end_cmd);
}
void tactyk_emit_svc__disconfigure(struct tactyk_emit__Context *emit_context) {
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-new");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-build");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-mem-external");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-mem");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-mem-ref");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-mem-copy");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-label");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-intlabel");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-1");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-2");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-3");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-4");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-5");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-token-6");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-text");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-int");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-float");
    tactyk_dblock__delete(tactyk_emit_svc__emitctx->api_table, "emit-cmd-end");
}

void tactyk_emit_svc__new(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__plctx = tactyk_pl__new(tactyk_emit_svc__emitctx);
}

void tactyk_emit_svc__build(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__Program *program = tactyk_pl__build(tactyk_emit_svc__plctx);
    // attach to asmvm_ctx, assign a handle to register a.
}

void tactyk_emit_svc__mem_external(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = NULL;

    tactyk_emit_svc__declare_memblock(asmvm_ctx, &mem_ll, &mem_hl);
}

void tactyk_emit_svc__mem_ref(struct tactyk_asmvm__Context *asmvm_ctx) {
    // copy a memblock ref from the asmvm_ctx
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = NULL;

    tactyk_emit_svc__declare_memblock(asmvm_ctx, &mem_ll, &mem_hl);

    struct tactyk_asmvm__memblock_lowlevel *refm_ll = &asmvm_ctx->active_memblocks[1];
    struct tactyk_asmvm__memblock_highlevel *refm_hl = tactyk_dblock__index(asmvm_ctx->hl_program_ref->memory_layout_hl, refm_ll->memblock_index);

    if ( (refm_ll->array_bound <= 0) || (refm_ll->element_bound <= 0) ) {
        error("EMIT-SVC -- invalid source memblock", NULL);
    }

    mem_ll->array_bound = refm_ll->array_bound;
    mem_ll->element_bound = refm_ll->element_bound;
    mem_ll->type = refm_ll->type;
    mem_ll->base_address = refm_ll->base_address;

    mem_hl->num_entries = refm_hl->num_entries;
    mem_hl->type = refm_hl->type;
    mem_hl->data = refm_hl->data;
    mem_hl->definition = refm_hl->definition;
}

void tactyk_emit_svc__mem_empty(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = NULL;

    tactyk_emit_svc__define_memblock(asmvm_ctx, &mem_ll, &mem_hl);
}

void tactyk_emit_svc__mem_data(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = NULL;
    tactyk_emit_svc__declare_memblock(asmvm_ctx, &mem_ll, &mem_hl);

    struct tactyk_asmvm__memblock_lowlevel *refm_ll = &asmvm_ctx->active_memblocks[1];
    struct tactyk_asmvm__memblock_highlevel *refm_hl = tactyk_dblock__index(asmvm_ctx->hl_program_ref->memory_layout_hl, refm_ll->memblock_index);

    if ( (refm_ll->array_bound <= 0) || (refm_ll->element_bound <= 0) ) {
        error("EMIT-SVC -- invalid source memblock", NULL);
    }

    uint64_t len = refm_ll->array_bound + refm_ll->element_bound + 7;
    uint8_t *data = calloc(1, len);
    memcpy(data, refm_ll->base_address, len);
}

void tactyk_emit_svc__label(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_dblock__DBlock *lbl = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    tactyk_emit__add_script_label(tactyk_emit_svc__emitctx, lbl);
}
void tactyk_emit_svc__intlabel(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_dblock__DBlock *lbl = tactyk_dblock__from_uint(asmvm_ctx->regbank_A.rA);
    tactyk_emit__add_script_label(tactyk_emit_svc__emitctx, lbl);
}

void tactyk_emit_svc__cmd(struct tactyk_asmvm__Context *asmvm_ctx) {
    cmd = tactyk_dblock__new(0);
    struct tactyk_dblock__DBlock *token = tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA);
    cmd->token = token;
    cmd_lasttoken = token;
}
void tactyk_emit_svc__append_token(struct tactyk_dblock__DBlock *token) {
    if (cmd == NULL) {
        error("EMIT-SVC:  No open command to attach a token to", NULL);
    }
    cmd_lasttoken->next = token;
    cmd_lasttoken = token;
}

void tactyk_emit_svc__token1(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
}
void tactyk_emit_svc__token2(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rB));
}
void tactyk_emit_svc__token3(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rB));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rC));
}
void tactyk_emit_svc__token4(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rB));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rC));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rD));
}
void tactyk_emit_svc__token5(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rB));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rC));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rD));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rE));
}
void tactyk_emit_svc__token6(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rA));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rB));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rC));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rD));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rE));
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_token(asmvm_ctx->regbank_A.rF));
}
void tactyk_emit_svc__text(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_emit_svc__get_text(asmvm_ctx, 0));
}
void tactyk_emit_svc__int(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_dblock__from_uint(asmvm_ctx->regbank_A.rA));
}
void tactyk_emit_svc__float(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__append_token(tactyk_dblock__from_float(asmvm_ctx->regbank_A.xa.f64[0]));
}
void tactyk_emit_svc__end_cmd(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit__add_script_command(tactyk_emit_svc__emitctx, cmd->token, cmd);
    cmd = NULL;
    cmd_lasttoken = NULL;
}

struct tactyk_dblock__DBlock* tactyk_emit_svc__get_text(struct tactyk_asmvm__Context *asmvm_ctx, uint64_t slot) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &asmvm_ctx->active_memblocks[0];
    uint8_t *data = NULL;
    uint64_t ofs = 0;
    uint64_t len = 0;
    switch(slot) {
        case 0: {
            data = (uint8_t*) asmvm_ctx->regbank_A.rADDR1;
            ofs = asmvm_ctx->regbank_A.rA;
            len = asmvm_ctx->regbank_A.rB;
            break;
        }
        case 1: {
            data = (uint8_t*) asmvm_ctx->regbank_A.rADDR2;
            ofs = asmvm_ctx->regbank_A.rC;
            len = asmvm_ctx->regbank_A.rD;
            break;
        }
        case 2: {
            data = (uint8_t*) asmvm_ctx->regbank_A.rADDR3;
            ofs = asmvm_ctx->regbank_A.rE;
            len = asmvm_ctx->regbank_A.rF;
            break;
        }
    }

    uint64_t max_ofs = ofs + len;

    if (data == NULL) {
        error("EMIT-SVC - get_text -- text source is NULL", NULL);
    }

    if ((ofs > mb->element_bound) || (len > mb->element_bound) || (max_ofs > mb->element_bound)) {
        error("EMIT-SVC - get_text -- text range out of bounds", NULL);
    }

    struct tactyk_dblock__DBlock *text = tactyk_dblock__from_bytes(NULL, data, ofs, len, false);
    return text;
}
struct tactyk_dblock__DBlock* tactyk_emit_svc__get_token(uint64_t handle) {
    if (handle >= tactyk_emit_svc__emitctx->token_handle_count) {
        error("EMIT-SVC -- Invalid token handle", NULL);
    }
    struct tactyk_dblock__DBlock *t_base = tactyk_emit_svc__emitctx->visa_token_invmap[handle];
    assert(t_base != NULL);
    assert(t_base->length > 1);
    uint8_t *data = (uint8_t*) t_base->data;
    return tactyk_dblock__from_bytes(NULL, data, 1, t_base->length-1, true);
}

void tactyk_emit_svc__declare_memblock(struct tactyk_asmvm__Context *asmvm_ctx, struct tactyk_asmvm__memblock_lowlevel **m_ll, struct tactyk_asmvm__memblock_highlevel **m_hl) {
    struct tactyk_dblock__DBlock *mem_name = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    int64_t id = tactyk_emit_svc__plctx->memspec_highlevel_table->element_count;

    assert(tactyk_emit_svc__plctx->memspec_lowlevel_buffer->element_count == tactyk_emit_svc__plctx->memspec_highlevel_table->element_count);

    *m_ll = (struct tactyk_asmvm__memblock_lowlevel*) tactyk_dblock__new_object(tactyk_emit_svc__plctx->memspec_lowlevel_buffer);
    *m_hl = (struct tactyk_asmvm__memblock_highlevel*) tactyk_dblock__new_managedobject(tactyk_emit_svc__plctx->memspec_highlevel_table, mem_name);

    struct tactyk_asmvm__memblock_lowlevel *mem_ll = *m_ll;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = *m_hl;

    mem_ll->array_bound = 0;
    mem_ll->element_bound = 0;
    mem_ll->memblock_index = id;
    mem_ll->type = 0;
    mem_ll->base_address = NULL;

    mem_hl->memblock = mem_ll;
    mem_hl->num_entries = 0;
    mem_hl->memblock_id = id;
    mem_hl->data = NULL;
    mem_hl->definition = calloc(1, sizeof(struct tactyk_asmvm__struct));
    mem_hl->definition->byte_stride = 0;
    mem_hl->definition->num_properties = 0;

    struct tactyk_dblock__DBlock *memid = tactyk_dblock__from_int(id);
    tactyk_dblock__put(tactyk_emit_svc__emitctx->memblock_table, mem_name, memid);
}

void tactyk_emit_svc__define_memblock(struct tactyk_asmvm__Context *asmvm_ctx, struct tactyk_asmvm__memblock_lowlevel **m_ll, struct tactyk_asmvm__memblock_highlevel **m_hl) {
    struct tactyk_dblock__DBlock *mem_name = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    uint64_t element_count = asmvm_ctx->regbank_A.rC;
    uint64_t element_size = asmvm_ctx->regbank_A.rD;

    tactyk_emit_svc__declare_memblock(asmvm_ctx, m_ll, m_hl);

    struct tactyk_asmvm__memblock_lowlevel *mem_ll = *m_ll;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = *m_hl;

    mem_ll->array_bound = (element_count-1) * element_size + 1;
    mem_ll->element_bound = element_size - 7;
    mem_hl->num_entries = element_count;
    mem_hl->definition->byte_stride = element_size;
    mem_hl->definition->num_properties = 0;

    uint8_t *data = calloc(element_count, element_size);
    mem_ll->base_address = data;
    mem_hl->data = data;
}


