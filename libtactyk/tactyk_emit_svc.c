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

void tactyk_emit_svc__configure(struct tactyk_emit__Context *emit_context) {
    tactyk_emit_svc__emitctx = emit_context;
}

void tactyk_emit_svc__new(struct tactyk_asmvm__Context *asmvm_ctx) {
    tactyk_emit_svc__plctx = tactyk_pl__new(tactyk_emit_svc__emitctx);
}

void tactyk_emit_svc__build(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__Program *program = tactyk_pl__build(tactyk_emit_svc__plctx);
    // attach to asmvm_ctx, assign a handle to register a.
}

void tactyk_emit_svc__mem_external(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_dblock__DBlock *mname = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    // declare a memblock
}

void tactyk_emit_svc__mem_empty(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_dblock__DBlock *mname = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    uint64_t capacity = asmvm_ctx->regbank_A.rC;

    // make empty memblock
}

void tactyk_emit_svc__mem_data(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_dblock__DBlock *mname = tactyk_emit_svc__get_text(asmvm_ctx, 0);
    struct tactyk_dblock__DBlock *db = tactyk_emit_svc__get_text(asmvm_ctx, 1);
    uint8_t *data = (uint8_t*) db->data;
    uint64_t capacity = db->length;
    tactyk_dblock__release(db);

    // make memblock with data
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
    tactyk_emit__add_script_command(tactyk_emit_svc__emitctx, cmd, NULL);
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
    struct tactyk_dblock__DBlock *t = tactyk_emit_svc__emitctx->visa_token_invmap[handle];
    assert (t != NULL);
    return t;
}




