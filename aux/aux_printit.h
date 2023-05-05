#ifndef AUX_PRINTIT_H
#define AUX_PRINTIT_H

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

/*
    Tactyk-auxilliary print functions
    These are intended to help produce text-based output
*/

void aux_printit__configure(struct tactyk_emit__Context *emitctx);

// space and newline print functions
void aux_prinit__newline(struct tactyk_asmvm__Context *ctx);
void aux_prinit__space(struct tactyk_asmvm__Context *ctx);

// print a string using a memblock reference
void aux_prinit__textref_addr1(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_addr2(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_addr3(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_addr4(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_bounded_addr1(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_bounded_addr2(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_bounded_addr3(struct tactyk_asmvm__Context *ctx);
void aux_prinit__textref_bounded_addr4(struct tactyk_asmvm__Context *ctx);

// print up to 8 chars from a register value (intended to work with literal 'string assignments).
void aux_prinit__text_a(struct tactyk_asmvm__Context *ctx);
void aux_prinit__text_b(struct tactyk_asmvm__Context *ctx);
void aux_prinit__text_c(struct tactyk_asmvm__Context *ctx);
void aux_prinit__text_d(struct tactyk_asmvm__Context *ctx);
void aux_prinit__text_e(struct tactyk_asmvm__Context *ctx);
void aux_prinit__text_f(struct tactyk_asmvm__Context *ctx);

// print signed integers
void aux_prinit__int_a(struct tactyk_asmvm__Context *ctx);
void aux_prinit__int_b(struct tactyk_asmvm__Context *ctx);
void aux_prinit__int_c(struct tactyk_asmvm__Context *ctx);
void aux_prinit__int_d(struct tactyk_asmvm__Context *ctx);
void aux_prinit__int_e(struct tactyk_asmvm__Context *ctx);
void aux_prinit__int_f(struct tactyk_asmvm__Context *ctx);

// print unsigned integers
void aux_prinit__uint_a(struct tactyk_asmvm__Context *ctx);
void aux_prinit__uint_b(struct tactyk_asmvm__Context *ctx);
void aux_prinit__uint_c(struct tactyk_asmvm__Context *ctx);
void aux_prinit__uint_d(struct tactyk_asmvm__Context *ctx);
void aux_prinit__uint_e(struct tactyk_asmvm__Context *ctx);
void aux_prinit__uint_f(struct tactyk_asmvm__Context *ctx);

#endif //AUX_PRINTIT_H



