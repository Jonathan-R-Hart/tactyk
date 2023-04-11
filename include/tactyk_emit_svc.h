#ifndef TACTYK_EMIT__SVC_H
#define TACTYK_EMIT__SVC_H

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"

/*
    An interface for allowing a tactyk program to make more tactyk programs.
    This is intended to rectify another security concern:  If the compiler toolchain which "natively" targets a
    platform exists outside of the sandbox and is complex, simply loading scripted content from source into a
    sandbox can insert security risks even if the sandbox is secure.

    This compiler interface is to work by duplicating enumerated tokens, combining them into a series of "dblock"
    structures, then feeding them into the tactyk_emit interface the same way tactyk_pl does.

    The token-producing functions in this interface are "convenience" functions.  Each of them will pretty much perform
    the same operation, but with different backing tables.  The actual constraints are enforced by tactyk_emit.
*/

void tactyk_emit_svc__configure(struct tactyk_emit__Context *emit_context);

void tactyk_emit_svc__new(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__build(struct tactyk_asmvm__Context *asmvm_ctx);

void tactyk_emit_svc__mem_external(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__mem_empty(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__mem_data(struct tactyk_asmvm__Context *asmvm_ctx);

void tactyk_emit_svc__intlabel(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__label(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__cmd(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token1(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token2(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token3(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token4(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token5(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__token6(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__text(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__int(struct tactyk_asmvm__Context *asmvm_ctx);
    void tactyk_emit_svc__float(struct tactyk_asmvm__Context *asmvm_ctx);
void tactyk_emit_svc__end_cmd(struct tactyk_asmvm__Context *asmvm_ctx);

#endif



