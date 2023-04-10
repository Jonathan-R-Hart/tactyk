//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef AUX__TESTLIB_H
#define AUX__TESTLIB_H

#include <stdint.h>

#include "tactyk_emit.h"
#include "tactyk_asmvm.h"
#include "tactyk_util.h"

void aux_configure(struct tactyk_emit__Context *emit_context);

uint64_t aux_rand();
void aux_sleep(uint64_t milliseconds);
void aux__dump(struct tactyk_asmvm__Context *asmvm_context);
void aux__break(struct tactyk_asmvm__Context *asmvm_context);

void aux__read_file(struct tactyk_asmvm__Context *asmvm_context);
void aux__write_file(struct tactyk_asmvm__Context *asmvm_context);

void aux__term_write_int(int64_t val);
void aux__term_write_float(double val);
void aux__term_write_char(int64_t val);
void aux__term_int(int64_t val);
void aux__term_char(int64_t val);
void aux__term_position(int64_t x, int64_t y);
void aux__term_color(int64_t x);
void aux__term(int64_t x, int64_t y, int64_t color, int64_t ch);
void aux__term_write(int64_t x, int64_t y, int64_t color, int64_t ch);

#endif // AUX__TESTLIB_H
