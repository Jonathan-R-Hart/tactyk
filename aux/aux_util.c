#include <unistd.h>

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"

#include "aux_util.h"

void aux_util__configure(struct tactyk_emit__Context *emit_context) {
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand", aux_util__rand_a);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-a", aux_util__rand_a);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-b", aux_util__rand_b);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-c", aux_util__rand_c);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-d", aux_util__rand_d);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-e", aux_util__rand_e);
    tactyk_emit__add_tactyk_apifunc(emit_context, "rand-f", aux_util__rand_f);
    
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep", aux_util__sleep_a);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-a", aux_util__sleep_a);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-b", aux_util__sleep_b);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-c", aux_util__sleep_c);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-d", aux_util__sleep_d);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-e", aux_util__sleep_e);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sleep-f", aux_util__sleep_f);
}

void aux_util__rand_a(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rA = tactyk__rand_uint64();
}
void aux_util__rand_b(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rB = tactyk__rand_uint64();
}
void aux_util__rand_c(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rC = tactyk__rand_uint64();
}
void aux_util__rand_d(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rD = tactyk__rand_uint64();
}
void aux_util__rand_e(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rE = tactyk__rand_uint64();
}
void aux_util__rand_f(struct tactyk_asmvm__Context *ctx) {
    ctx->reg.rF = tactyk__rand_uint64();
}

void aux_util__sleep_a(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rA*1000);
}
void aux_util__sleep_b(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rB*1000);
}
void aux_util__sleep_c(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rC*1000);
}
void aux_util__sleep_d(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rD*1000);
}
void aux_util__sleep_e(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rE*1000);
}
void aux_util__sleep_f(struct tactyk_asmvm__Context *ctx) {
    usleep(ctx->reg.rF*1000);
}







