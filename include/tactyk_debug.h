#ifndef TACTYK_DEBUG_H
#define TACTYK_DEBUG_H

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"

void tactyk_debug__configure_api(struct tactyk_emit__Context *emitctx);

void tactyk_debug__print_context(struct tactyk_asmvm__Context *ctx);
void tactyk_debug__print_mbind(struct tactyk_asmvm__Context *ctx);

void tactyk_debug__break(struct tactyk_asmvm__Context *ctx);


#endif // TACTYK_DEBUG_H
