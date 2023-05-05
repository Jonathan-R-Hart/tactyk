
#include <stdio.h>
#include <string.h>

#include "aux_printit.h"
#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

FILE *stream;

void aux_printit__configure(struct tactyk_emit__Context *emitctx) {
    stream = stdout;
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-newline", aux_prinit__newline);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-space", aux_prinit__space);
    
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem", aux_prinit__textref_addr1);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-addr1", aux_prinit__textref_addr1);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-addr2", aux_prinit__textref_addr2);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-addr3", aux_prinit__textref_addr3);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-addr4", aux_prinit__textref_addr4);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-bounded", aux_prinit__textref_bounded_addr1);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-bounded-addr1", aux_prinit__textref_bounded_addr1);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-bounded-addr2", aux_prinit__textref_bounded_addr2);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-bounded-addr3", aux_prinit__textref_bounded_addr3);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-mem-bounded-addr4", aux_prinit__textref_bounded_addr4);
    
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text", aux_prinit__text_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-a", aux_prinit__text_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-b", aux_prinit__text_b);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-c", aux_prinit__text_c);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-d", aux_prinit__text_d);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-e", aux_prinit__text_e);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-text-f", aux_prinit__text_f);
    
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int", aux_prinit__int_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-a", aux_prinit__int_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-b", aux_prinit__int_b);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-c", aux_prinit__int_c);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-d", aux_prinit__int_d);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-e", aux_prinit__int_e);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-int-f", aux_prinit__int_f);
    
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint", aux_prinit__uint_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-a", aux_prinit__uint_a);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-b", aux_prinit__uint_b);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-c", aux_prinit__uint_c);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-d", aux_prinit__uint_d);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-e", aux_prinit__uint_e);
    tactyk_emit__add_tactyk_apifunc(emitctx, "print-uint-f", aux_prinit__uint_f);
}

// space and newline print functions
void aux_prinit__newline(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "\n");
}
void aux_prinit__space(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, " ");
}

//
void aux_printit__print_string(uint8_t *text, uint64_t maxlen) {
    // simple case:  if a null terminator exists within the input buffer, print it directly
    if (memchr(text, 0, maxlen) != NULL) {
        fprintf(stream, "%s", text);
        return;
    }
    
    // otherwise, print it in 1024-byte blocks, using an intermediate buffer with a guaranteed null terminator.
    char buf[1025];
    memset(buf, 0, 1025);
    
    for (uint64_t i = 0; i < maxlen; i += 1024) {
        uint64_t amt = maxlen-i;
        if (amt > 1024) { 
            amt = 1024; 
        }
        else {
            buf[amt] = 0;
        }
        memcpy(buf, &text[i], amt);
        fprintf(stream, "%s", buf);
    }
}

// print a string using a memblock reference
void aux_prinit__textref_addr1(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[0];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR1, mb->element_bound);
    fflush(stream);
}
void aux_prinit__textref_addr2(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[1];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR2, mb->element_bound);
    fflush(stream);
}
void aux_prinit__textref_addr3(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[2];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR3, mb->element_bound);
    fflush(stream);
}
void aux_prinit__textref_addr4(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[3];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR4, mb->element_bound);
    fflush(stream);
}

void aux_prinit__textref_bounded_addr1(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[0];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    uint64_t limit = ctx->reg.rA;
    if (limit > mb->element_bound) {
        limit = mb->element_bound;
    }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR1, limit);
    fflush(stream);
}
void aux_prinit__textref_bounded_addr2(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[1];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    uint64_t limit = ctx->reg.rA;
    if (limit > mb->element_bound) {
        limit = mb->element_bound;
    }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR2, limit);
    fflush(stream);
}
void aux_prinit__textref_bounded_addr3(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[2];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    uint64_t limit = ctx->reg.rA;
    if (limit > mb->element_bound) {
        limit = mb->element_bound;
    }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR3, limit);
    fflush(stream);
}
void aux_prinit__textref_bounded_addr4(struct tactyk_asmvm__Context *ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mb = &ctx->active_memblocks[3];
    if (mb->base_address == NULL) { return; }
    if (mb->element_bound == 0) { return; }
    uint64_t limit = ctx->reg.rA;
    if (limit > mb->element_bound) {
        limit = mb->element_bound;
    }
    aux_printit__print_string(  (uint8_t*)ctx->reg.rADDR4, limit);
    fflush(stream);
}

// print up to 8 chars from a register value (intended to work with literal 'string assignments).
void aux_prinit__text_a(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 };
    buf[0] = ctx->reg.rA;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}
void aux_prinit__text_b(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 };
    buf[0] = ctx->reg.rB;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}
void aux_prinit__text_c(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 };
    buf[0] = ctx->reg.rC;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}
void aux_prinit__text_d(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 };
    buf[0] = ctx->reg.rD;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}
void aux_prinit__text_e(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 }; 
    buf[0] = ctx->reg.rE;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}
void aux_prinit__text_f(struct tactyk_asmvm__Context *ctx) {
    uint64_t buf[2] = { 0,0 };
    buf[0] = ctx->reg.rF;
    fprintf(stream, "%s", (char*) buf);
    fflush(stream);
}

// print signed integers
void aux_prinit__int_a(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rA);
    fflush(stream);
}
void aux_prinit__int_b(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rB);
    fflush(stream);
}
void aux_prinit__int_c(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rC);
    fflush(stream);
}
void aux_prinit__int_d(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rD);
    fflush(stream);
}
void aux_prinit__int_e(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rE);
    fflush(stream);
}
void aux_prinit__int_f(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%jd", (int64_t)ctx->reg.rF);
    fflush(stream);
}

// print unsigned integers
void aux_prinit__uint_a(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rA);
    fflush(stream);
}
void aux_prinit__uint_b(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rB);
    fflush(stream);
}
void aux_prinit__uint_c(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rC);
    fflush(stream);
}
void aux_prinit__uint_d(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rD);
    fflush(stream);
}
void aux_prinit__uint_e(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rE);
    fflush(stream);
    fflush(stream);
}
void aux_prinit__uint_f(struct tactyk_asmvm__Context *ctx) {
    fprintf(stream, "%ju", ctx->reg.rF);
    fflush(stream);
}
