
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "tactyk.h"
#include "tactyk_emit.h"
#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"
#include "tactyk_util.h"
#include "aux_sdl.h"

struct aux_sdl__State {
    uint32_t mpos_x;
    uint32_t mpos_y;
    uint32_t key_pressed__sdlcode;
    uint32_t key_released__sdlcode;
    uint32_t key_pressed;
    uint32_t key_released;
    uint8_t closing;
    uint8_t mouse_moved;
    uint8_t mouse_button_pressed;
    uint8_t mouse_button_released;
};

struct aux_sdl__Context {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Rect draw_area;
    uint64_t texw;
    uint64_t texh;
    uint64_t texscale;
    uint64_t bufsize;
    uint8_t *framebuffer;
    struct aux_sdl__State *ui_state;
    
    uint64_t fmt_flags;
};

struct aux_sdl__Context *sdlctx;

uint64_t aux_sdl__max_window_width;
uint64_t aux_sdl__max_window_height;

#define FRAMEBUFFER_FORMAT__ELEMENTS 0x1

void aux_sdl__configure(struct tactyk_emit__Context *emit_context, uint64_t max_window_width, uint64_t max_window_height) {
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--get-framebuffer", aux_sdl__get_framebuffer);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--get-eventview", aux_sdl__get_event_view);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--new", aux_sdl__new);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--upload-framebuffer", aux_sdl__upload_framebuffer);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--render", aux_sdl__render);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--release", aux_sdl__release);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--update_ui", aux_sdl__consume_events);
    tactyk_emit__add_tactyk_apifunc(emit_context, "sdl--quit", aux_sdl__quit);
    
    int err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (err != 0) {
        error("AUX-SDL-init -- SDL init failed", NULL);
    }
    aux_sdl__max_window_width = max_window_width;
    aux_sdl__max_window_height = max_window_height;
}

void aux_sdl__new(struct tactyk_asmvm__Context *asmvm_ctx) {
    if (sdlctx != NULL) {
        warn("AUX-SDL-new -- SDL Context is already initialized", NULL);
        return;
    }
    sdlctx = calloc(1, sizeof(struct aux_sdl__Context));
    sdlctx->draw_area.x = 0;
    sdlctx->draw_area.y = 0;
    
    sdlctx->draw_area.w = asmvm_ctx->reg.rA;
    sdlctx->draw_area.h = asmvm_ctx->reg.rB;
    
    sdlctx->texw = tactyk_util__next_pow2(sdlctx->draw_area.w);
    sdlctx->texh = tactyk_util__next_pow2(sdlctx->draw_area.h);
    
    sdlctx->fmt_flags = asmvm_ctx->reg.rD;
    
    sdlctx->window = SDL_CreateWindow("TACTYK-SDL Window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        sdlctx->draw_area.w, sdlctx->draw_area.h,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    
    sdlctx->renderer = SDL_CreateRenderer(sdlctx->window, -1, 0);
    sdlctx->texture = SDL_CreateTexture(sdlctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, sdlctx->texw, sdlctx->texh);
    
    sdlctx->texscale = sizeof(uint32_t);
    sdlctx->bufsize = sdlctx->texw * sdlctx->texh * sizeof(uint32_t);
    sdlctx->framebuffer = calloc(sdlctx->bufsize, 1);
    sdlctx->ui_state = calloc(1, sizeof(struct aux_sdl__State));
}

void aux_sdl__get_framebuffer(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll_ctx = &asmvm_ctx->active_memblocks[0];
    
    mem_ll_ctx->base_address = (uint8_t*) sdlctx->framebuffer;
    mem_ll_ctx->offset = 0;
    mem_ll_ctx->memblock_index = 0xffffffff;
    asmvm_ctx->reg.rADDR1 = (uint64_t*) sdlctx->framebuffer;
    asmvm_ctx->reg.rA = sdlctx->texw;
    asmvm_ctx->reg.rB = sdlctx->texh;
    
    if ((sdlctx->fmt_flags & FRAMEBUFFER_FORMAT__ELEMENTS) == 0) {
        mem_ll_ctx->array_bound = 1;
        mem_ll_ctx->element_bound = sdlctx->bufsize;
    }
    else {
        mem_ll_ctx->array_bound = sdlctx->bufsize - sdlctx->texscale + 1;
        mem_ll_ctx->element_bound = sdlctx->texscale;
    }
}

void aux_sdl__get_event_view(struct tactyk_asmvm__Context *asmvm_ctx) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll_ctx = &asmvm_ctx->active_memblocks[0];
    
    mem_ll_ctx->base_address = (uint8_t*) sdlctx->ui_state;
    mem_ll_ctx->array_bound = 1;
    mem_ll_ctx->element_bound = sizeof(struct aux_sdl__State);
    mem_ll_ctx->offset = 0;
    mem_ll_ctx->memblock_index = 0xffffffff;
    
    asmvm_ctx->reg.rADDR1 = (uint64_t*) sdlctx->ui_state;
}

void aux_sdl__upload_framebuffer(struct tactyk_asmvm__Context *asmvm_ctx) {
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
    sdlctx->ui_state->key_pressed__sdlcode = 0;
    sdlctx->ui_state->key_released__sdlcode = 0;
    sdlctx->ui_state->key_pressed = 0;
    sdlctx->ui_state->key_released = 0;
    sdlctx->ui_state->closing = 0;
    sdlctx->ui_state->mouse_moved = 0;
    sdlctx->ui_state->mouse_button_pressed = 0;
    sdlctx->ui_state->mouse_button_released = 0;

    while (SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN: {
                sdlctx->ui_state->key_pressed__sdlcode = event.key.keysym.scancode;
                sdlctx->ui_state->key_pressed = event.key.keysym.sym;
                break;
            }
            case SDL_KEYUP: {
                sdlctx->ui_state->key_released__sdlcode = event.key.keysym.scancode;
                sdlctx->ui_state->key_released = event.key.keysym.sym;
                break;
            }
            
            case SDL_WINDOWEVENT: {
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE: {
                        sdlctx->ui_state->closing = 1;
                        break;
                    }
                }
                break;
            }
            
            case SDL_MOUSEMOTION: {
                sdlctx->ui_state->mpos_x = event.motion.x;
                sdlctx->ui_state->mpos_y = event.motion.y;
                sdlctx->ui_state->mouse_moved = 1;
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                sdlctx->ui_state->mouse_button_pressed = event.button.button;
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                sdlctx->ui_state->mouse_button_released = event.button.button;
                break;
            }
        }
    }
}

void aux_sdl__quit(struct tactyk_asmvm__Context *asmvm_ctx) {
    SDL_Quit();
}














