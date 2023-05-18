#ifndef TACTYK_DEBUG_H
#define TACTYK_DEBUG_H

#include <stdio.h>

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"


#define TACTYK_DEBUG__XMM_DISPLAYMODE__FLOAT64 0
#define TACTYK_DEBUG__XMM_DISPLAYMODE__FLOAT32 1
#define TACTYK_DEBUG__XMM_DISPLAYMODE__INT32 8
#define TACTYK_DEBUG__XMM_DISPLAYMODE__INT64 9
#define TACTYK_DEBUG__XMM_DISPLAYMODE__STRING 16

void tactyk_debug__configure_api(struct tactyk_emit__Context *emitctx);

void tactyk_debug__write_context(struct tactyk_asmvm__Context *ctx, FILE *stream);
void tactyk_debug__write_mbind(struct tactyk_asmvm__Context *ctx, FILE *stream);
void tactyk_debug__write_vmstack(struct tactyk_asmvm__Stack *st, FILE *stream);
void tactyk_debug__write_vmprograms(struct tactyk_asmvm__Context *ctx, FILE *stream);

void tactyk_debug__set_display_mode(struct tactyk_asmvm__Context *ctx);
void tactyk_debug__print_context(struct tactyk_asmvm__Context *ctx);
void tactyk_debug__print_mbind(struct tactyk_asmvm__Context *ctx);
void tactyk_debug__print_vmstack(struct tactyk_asmvm__Context *ctx);
void tactyk_debug__print_vmprograms(struct tactyk_asmvm__Context *ctx);

void tactyk_debug__break(struct tactyk_asmvm__Context *ctx);

extern uint64_t tactyk_debug__xmm_display_mode;

#endif // TACTYK_DEBUG_H
