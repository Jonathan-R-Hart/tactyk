
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

struct aux_sdl__State {
    uint64_t mpos_x;
    uint64_t mpos_y;
    uint64_t key;
    uint8_t closing;
    uint8_t mousemoved;
    uint8_t unused[6];
    uint64_t padding;
};

struct aux_sdl__Context {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Rect draw_area;
    uint64_t texw;
    uint64_t texh;
    uint64_t bufsize;
    uint8_t *framebuffer;
    struct aux_sdl__State *state;
};

struct aux_sdl__Context *sdlctx;
struct aux_sdl__State *sdlstate;

void aux_sdl__configure(struct tactyk_emit__Context *emit_context) {
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_init", aux_sdl__init);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_new", aux_sdl__new);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_update", aux_sdl__update_buffer);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_render", aux_sdl__render);
    tactyk_emit__add_tactyk_apifunc(emit_context, "fb_release", aux_sdl__release);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_update_state", aux_sdl__consume_events);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl_quit", aux_sdl__quit);
}

void aux_sdl__init(struct tactyk_asmvm__Context *asmvm_ctx) {
    int err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (err != 0) {
        error("AUX-SDL-init -- SDL init failed", NULL);
    }
}

void aux_sdl__new(struct tactyk_asmvm__Context *asmvm_ctx) {
    if (sdlctx != NULL) {
        warn("AUX-SDL-new -- SDL Context is already initialized", NULL);
        return;
    }
    sdlctx = calloc(1, sizeof(struct aux_sdl__Context));
    sdlctx->draw_area.x = 0;
    sdlctx->draw_area.y = 0;
    int64_t mbpos = asmvm_ctx->reg.rA-1;
    if ( (mbpos < 0) || (mbpos > 3) ) {
        error("AUX-SDL -- Invalid active-memblock index", NULL);
        return;
    }

    int64_t stpos = asmvm_ctx->reg.rB-1;
    if ( (stpos < 0) || (stpos > 3) ) {
        error("AUX-SDL -- Invalid active-memblock index", NULL);
        return;
    }


    //active_mblock->
    sdlctx->draw_area.w = asmvm_ctx->reg.rC;
    sdlctx->draw_area.h = asmvm_ctx->reg.rD;

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

    struct tactyk_asmvm__memblock_lowlevel *mem_ll_ctx = &asmvm_ctx->active_memblocks[mbpos];

    //struct tactyk_asmvm__memblock_highlevel *mem_hl;
    //struct tactyk_asmvm__memblock_lowlevel *mem_ll;

    //tactyk_asmvm__get_mblock(asmvm_ctx, "video", &mem_hl, &mem_ll);

    sdlctx->bufsize = sdlctx->texw * sdlctx->texh * sizeof(uint32_t);
    uint8_t *fb = calloc(sdlctx->bufsize+8, 1);

    mem_ll_ctx->base_address = fb;
    mem_ll_ctx->array_bound = 1;
    mem_ll_ctx->element_bound = sdlctx->bufsize;
    tactyk_asmvm__update_dynamic_memblock(asmvm_ctx, mem_ll_ctx, mbpos);
    //mem_hl->data = fb;
    sdlctx->framebuffer = fb;

    asmvm_ctx->reg.rA = sdlctx->texw;
    asmvm_ctx->reg.rB = sdlctx->texh;
    asmvm_ctx->reg.rC = 0;
    asmvm_ctx->reg.rD = 0;

    struct tactyk_asmvm__memblock_lowlevel *mem_ll_st = &asmvm_ctx->active_memblocks[stpos];
    sdlstate = (struct aux_sdl__State*) mem_ll_st->base_address;
    // probably should update the rest of mem_hl
}

void aux_sdl__update_buffer(struct tactyk_asmvm__Context *asmvm_ctx) {
    if (sdlctx == NULL) {
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
    if (sdlctx == NULL) {
        error("AUX-SDL-render -- SDL context is uninitialized", NULL);
        return;
    }
    SDL_RenderClear(sdlctx->renderer);
    SDL_RenderCopy(sdlctx->renderer, sdlctx->texture, &sdlctx->draw_area, &sdlctx->draw_area);
    SDL_RenderPresent(sdlctx->renderer);
}

void aux_sdl__release(struct tactyk_asmvm__Context *asmvm_ctx) {
    if (sdlctx == NULL) {
        error("AUX-SDL-release -- SDL context is uninitialized", NULL);
        return;
    }

    SDL_DestroyWindow( sdlctx->window );
    SDL_DestroyRenderer( sdlctx->renderer );
    SDL_Quit();
}

void aux_sdl__consume_events(struct tactyk_asmvm__Context *asmvm_ctx) {
    SDL_Event event;
    sdlstate->key = 0;
    sdlstate->closing = 0;
    sdlstate->mousemoved = 0;

    while (SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN: {
                sdlstate->key = event.key.keysym.scancode;
                break;
            }
            case SDL_WINDOWEVENT: {
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE: {
                        sdlstate->closing = 1;
                        break;
                    }
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                sdlstate->mpos_x = event.motion.x;
                sdlstate->mpos_y = event.motion.y;
                sdlstate->mousemoved = 1;
                break;
            }
        }
    }
}

void aux_sdl__quit(struct tactyk_asmvm__Context *asmvm_ctx) {
    SDL_Quit();
}














