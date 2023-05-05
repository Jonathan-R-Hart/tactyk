#ifndef AUX_UTIL_H
#define AUX_UTIL_H

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"

void aux_util__configure(struct tactyk_emit__Context *emit_context);

void aux_util__rand_a(struct tactyk_asmvm__Context *ctx);
void aux_util__rand_b(struct tactyk_asmvm__Context *ctx);
void aux_util__rand_c(struct tactyk_asmvm__Context *ctx);
void aux_util__rand_d(struct tactyk_asmvm__Context *ctx);
void aux_util__rand_e(struct tactyk_asmvm__Context *ctx);
void aux_util__rand_f(struct tactyk_asmvm__Context *ctx);

void aux_util__sleep_a(struct tactyk_asmvm__Context *ctx);
void aux_util__sleep_b(struct tactyk_asmvm__Context *ctx);
void aux_util__sleep_c(struct tactyk_asmvm__Context *ctx);
void aux_util__sleep_d(struct tactyk_asmvm__Context *ctx);
void aux_util__sleep_e(struct tactyk_asmvm__Context *ctx);
void aux_util__sleep_f(struct tactyk_asmvm__Context *ctx);

#endif //AUX_UTIL_H
