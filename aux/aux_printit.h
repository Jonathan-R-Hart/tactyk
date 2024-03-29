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
void aux_print__newline(struct tactyk_asmvm__Context *ctx);
void aux_print__space(struct tactyk_asmvm__Context *ctx);

// print a string using a memblock reference
void aux_print__textref_addr1(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_addr2(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_addr3(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_addr4(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_bounded_addr1(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_bounded_addr2(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_bounded_addr3(struct tactyk_asmvm__Context *ctx);
void aux_print__textref_bounded_addr4(struct tactyk_asmvm__Context *ctx);

// print up to 8 chars from a register value (intended to work with literal 'string assignments).
void aux_print__text_a(struct tactyk_asmvm__Context *ctx);
void aux_print__text_b(struct tactyk_asmvm__Context *ctx);
void aux_print__text_c(struct tactyk_asmvm__Context *ctx);
void aux_print__text_d(struct tactyk_asmvm__Context *ctx);
void aux_print__text_e(struct tactyk_asmvm__Context *ctx);
void aux_print__text_f(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xa(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xb(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xc(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xd(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xe(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xf(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xg(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xh(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xi(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xj(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xk(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xl(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xm(struct tactyk_asmvm__Context *ctx);
void aux_print__text_xn(struct tactyk_asmvm__Context *ctx);

// print signed integers
void aux_print__int_a(struct tactyk_asmvm__Context *ctx);
void aux_print__int_b(struct tactyk_asmvm__Context *ctx);
void aux_print__int_c(struct tactyk_asmvm__Context *ctx);
void aux_print__int_d(struct tactyk_asmvm__Context *ctx);
void aux_print__int_e(struct tactyk_asmvm__Context *ctx);
void aux_print__int_f(struct tactyk_asmvm__Context *ctx);

// print unsigned integers
void aux_print__uint_a(struct tactyk_asmvm__Context *ctx);
void aux_print__uint_b(struct tactyk_asmvm__Context *ctx);
void aux_print__uint_c(struct tactyk_asmvm__Context *ctx);
void aux_print__uint_d(struct tactyk_asmvm__Context *ctx);
void aux_print__uint_e(struct tactyk_asmvm__Context *ctx);
void aux_print__uint_f(struct tactyk_asmvm__Context *ctx);

// print 64-bit floating point numbers
void aux_print__float64_xa(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xb(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xc(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xd(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xe(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xf(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xg(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xh(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xi(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xj(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xk(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xl(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xm(struct tactyk_asmvm__Context *ctx);
void aux_print__float64_xn(struct tactyk_asmvm__Context *ctx);

#define f(suffix) void aux_print__float80_##suffix(struct tactyk_asmvm__Context *ctx)
    f(sa); f(se); f(si); f(sm); f(sq); f(su); f(sy);  f(s28);
    f(sb); f(sf); f(sj); f(sn); f(sr); f(sv); f(sz);  f(s29);
    f(sc); f(sg); f(sk); f(so); f(ss); f(sw); f(s26); f(s30);
    f(sd); f(sh); f(sl); f(sp); f(st); f(sx); f(s27); f(s31);
#undef f

#endif //AUX_PRINTIT_H



