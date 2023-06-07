#include "tactyk_asmvm.h"

void aux_sdl__configure(struct tactyk_emit__Context *emit_context);

void aux_sdl__init(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__get_framebuffer(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__get_event_view(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__new(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__render(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__update_buffer(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__release(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__quit(struct tactyk_asmvm__Context *asmvm_ctx);
void aux_sdl__consume_events(struct tactyk_asmvm__Context *asmvm_ctx);
