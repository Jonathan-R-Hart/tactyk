#ifndef ESVC_TEST_H
#define ESVC_TEST_H

#include "tactyk_asmvm.h"
#include "tactyk_emit.h"

struct tactyk_asmvm__Program* run_esvc_test(struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *ctx);

#endif // ESVC_TEST_H
