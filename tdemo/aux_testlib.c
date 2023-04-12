//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tactyk.h"
#include "tactyk_debug.h"
#include "tactyk_emit.h"
#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"
#include "tactyk_util.h"

#include "aux_testlib.h"

void aux_configure(struct tactyk_emit__Context *emit_context) {

    tactyk_emit__add_tactyk_apifunc(emit_context, "readfile", aux__read_file);

    tactyk_emit__add_c_apifunc(emit_context, "printchar", aux__term_write_char);
    tactyk_emit__add_c_apifunc(emit_context, "printint", aux__term_write_int);
    tactyk_emit__add_c_apifunc(emit_context, "printuint", aux__term_write_uint);
    tactyk_emit__add_c_apifunc(emit_context, "printfloat", aux__term_write_float);
    tactyk_emit__add_c_apifunc(emit_context, "sleep", aux_sleep);
    tactyk_emit__add_c_apifunc(emit_context, "rand", aux_rand);

}

uint64_t aux_rand() {
    uint64_t rnd = tactyk__rand_uint64();
    return rnd;
}

void aux_sleep(uint64_t milliseconds) {
    usleep(milliseconds*1000);
}

FILE* aux_open_file__from_ctxref(struct tactyk_asmvm__Context *asmvm_ctx, char *mode) {
    //volatile struct tactyk_asmvm__memblock_lowlevel *fname_mem = &asmvm_ctx->active_memblocks[0];
    char fname[265];
    memset(fname, 0, 265);
    sprintf(fname, "examples/");
    //uint64_t len = fname_mem->element_bound;
    uint64_t len = asmvm_ctx->active_memblocks[0].element_bound;
    if (len > 255) {
        len = 255;
    }

    // simple data fetch - assumes that the to fetch is located at the base address
    //memcpy(fname, fname_mem->base_address, len);

    // correct data fetch - respects script-specified offset
    //      (mblock.array_bound and mblock.element_bound are used for mandatory bounds checking, so this should be memort-safe)
    memcpy(&fname[9], asmvm_ctx->reg.rADDR1, len);

    // this should be replaced with a proper validation (the requested file must exists and must not be outside of the correct directory).
    for (uint64_t i = 0; i < (len-3); i += 1) {
        if (strncmp(&fname[i], "/..", 3) == 0) {
            error("AUX-READFILE -- Filename includes possible attempt to climb out of rsc directory", NULL);
        }
    }

    FILE *f = fopen(fname, mode);
    return f;
}

void aux__read_file(struct tactyk_asmvm__Context *asmvm_ctx) {
    FILE *f = aux_open_file__from_ctxref(asmvm_ctx, "r");
    fseek(f, 0, SEEK_END);
    int64_t len = ftell(f)+8;
    uint8_t *f_data = calloc(len, sizeof(char));
    fseek(f,0, SEEK_SET);
    fread(f_data, len, 1, f);
    fclose(f);

    struct tactyk_asmvm__memblock_highlevel *m_hl;
    struct tactyk_asmvm__memblock_lowlevel *m_ll;

    tactyk_asmvm__get_mblock(asmvm_ctx, "file", &m_hl, &m_ll);
    // for now, don't care about the high-level memory block specification, because there is no high-level code that depends on it for anything but the reference to the low-level spec.
    //      the low-level specification is what the compiled program uses.

    // release the data buffer from the previous file
    if (m_ll->base_address != NULL) {
        free(m_ll->base_address);
    }

    m_ll->base_address = f_data;
    m_ll->array_bound = 1;
    m_ll->element_bound = len-7;
}
void aux__write_file(struct tactyk_asmvm__Context *asmvm_ctx) {

}

/*
    TTY output.
    NOT IMPLEMENTED.  For now, this just uses printf for the two most basic operations.

    This should use Ncurses (or some simpler library) for text output with freely positionable cursor and variable colors.

    This is to be implemented after the project is set up for multiple build targets (to avoid placing extraneous dependencies into the scripting engine).
*/
void aux__term_write_int(int64_t val) {
    printf("%jd", val);
    fflush(stdout);
}
void aux__term_write_uint(uint64_t val) {
    printf("%ju", val);
    fflush(stdout);
}

void aux__term_write_float(double val) {
    printf("%f", val);
    fflush(stdout);
}
void aux__term_write_char(int64_t val) {
    printf("%c", (char)val);
    fflush(stdout);
}

void aux__term_int(int64_t val) {

}
void aux__term_char(int64_t val) {

}
void aux__term_position(int64_t x, int64_t y) {

}
void aux__term_color(int64_t x) {

}
void aux__term(int64_t x, int64_t y, int64_t color, int64_t ch) {

}
void aux__term_write(int64_t x, int64_t y, int64_t color, int64_t ch) {

}

