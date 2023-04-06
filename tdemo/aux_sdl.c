
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "tactyk.h"
#include "aux_testlib.h"
#include "tactyk_emit.h"
#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"
#include "tactyk_util.h"
#include "aux_sdl.h"

struct aux_sdl__Context {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Rect draw_area;
    uint64_t texw;
    uint64_t texh;
    uint64_t bufsize;
    uint8_t *framebuffer;
};

struct tactyk_dblock__DBlock *ctx_store;

void aux_sdl__configure(struct tactyk_emit__Context *emit_context) {
    ctx_store = tactyk_dblock__new_allocated_container(8, sizeof(struct aux_sdl__Context));
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_init", aux_sdl__init);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_new", aux_sdl__new);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_update", aux_sdl__update_buffer);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_render", aux_sdl__render);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_release", aux_sdl__release);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_isclosing", aux_sdl__is_window_closing);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_quit", aux_sdl__quit);
}

void aux_sdl__init(struct tactyk_asmvm__Context *asmvm_ctx) {
    int err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (err != 0) {
        error("AUX-SDL-init -- SDL init failed", NULL);
    }
}

void aux_sdl__new(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct aux_sdl__Context *sdlctx = tactyk_dblock__index(ctx_store, asmvm_ctx->regbank_A.rA);
    if (sdlctx->window != NULL) {
        warn("AUX-SDL-new -- SDL Context is already initialized", NULL);
        return;
    }
    sdlctx->draw_area.x = 0;
    sdlctx->draw_area.y = 0;
    sdlctx->draw_area.w = asmvm_ctx->regbank_A.rB;
    sdlctx->draw_area.h = asmvm_ctx->regbank_A.rC;

    sdlctx->texw = tactyk_util__next_pow2(sdlctx->draw_area.w);
    sdlctx->texh = tactyk_util__next_pow2(sdlctx->draw_area.h);

    sdlctx->window = SDL_CreateWindow("TACTYK-SDL Window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        sdlctx->draw_area.w, sdlctx->draw_area.h,
        SDL_WINDOW_RESIZABLE
    );

    sdlctx->renderer = SDL_CreateRenderer(sdlctx->window, -1, 0);
    sdlctx->texture = SDL_CreateTexture(sdlctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, sdlctx->texw, sdlctx->texh);

    struct tactyk_asmvm__memblock_highlevel *mem_hl;
    struct tactyk_asmvm__memblock_lowlevel *mem_ll;

    tactyk_asmvm__get_mblock(asmvm_ctx, "video", &mem_hl, &mem_ll);

    asmvm_ctx->regbank_A.rB = sdlctx->texw;
    asmvm_ctx->regbank_A.rC = sdlctx->texh;

    sdlctx->bufsize = sdlctx->texw * sdlctx->texh * sizeof(uint32_t);
    uint8_t *fb = calloc(sdlctx->bufsize+8, 1);

    mem_ll->base_address = fb;
    mem_ll->array_bound = 1;
    mem_ll->element_bound = sdlctx->bufsize;
    //mem_ll->left = 0;
    //mem_ll->type = 0;

    mem_hl->data = fb;
    sdlctx->framebuffer = fb;

    // probably should update the rest of mem_hl
}

void aux_sdl__update_buffer(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct aux_sdl__Context *sdlctx = tactyk_dblock__index(ctx_store, asmvm_ctx->regbank_A.rA);
    if (sdlctx->window == NULL) {
        error("AUX-SDL-update -- SDL context is uninitialized", NULL);
        return;
    }
    int pitch;
    int8_t* pixels;
    SDL_LockTexture(sdlctx->texture, NULL,  (void **) &pixels, &pitch);

    memcpy ( pixels, sdlctx->framebuffer, sdlctx->bufsize );
    SDL_UnlockTexture(sdlctx->texture);
}
void aux_sdl__render(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct aux_sdl__Context *sdlctx = tactyk_dblock__index(ctx_store, asmvm_ctx->regbank_A.rA);
    if (sdlctx->window == NULL) {
        error("AUX-SDL-render -- SDL context is uninitialized", NULL);
        return;
    }
    SDL_RenderClear(sdlctx->renderer);
    SDL_RenderCopy(sdlctx->renderer, sdlctx->texture, &sdlctx->draw_area, &sdlctx->draw_area);
    SDL_RenderPresent(sdlctx->renderer);
}

void aux_sdl__release(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct aux_sdl__Context *sdlctx = tactyk_dblock__index(ctx_store, asmvm_ctx->regbank_A.rA);
    if (sdlctx->window == NULL) {
        error("AUX-SDL-release -- SDL context is uninitialized", NULL);
        return;
    }

    SDL_DestroyWindow( sdlctx->window );
    SDL_DestroyRenderer( sdlctx->renderer );
    SDL_Quit();
}
void aux_sdl__is_window_closing(struct tactyk_asmvm__Context *asmvm_ctx) {
    SDL_Event event;
    if (SDL_PollEvent( &event ) != 0 ) {
        if( event.type == SDL_QUIT ) {
            asmvm_ctx->regbank_A.rA = 1;
            return;
        }
    }
    asmvm_ctx->regbank_A.rA = 0;
}
void aux_sdl__quit(struct tactyk_asmvm__Context *asmvm_ctx) {
    SDL_Quit();
}















