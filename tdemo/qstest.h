//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef QS_TEST__INCLUDE_GUARD
#define QS_TEST__INCLUDE_GUARD

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "tactyk_asmvm.h"
//#include "asm/tactyk_asmvm_symbols.h"
#include "tactyk_pl.h"

void run_qsort_tests(struct tactyk_emit__Context *emitctx, int64_t len, int64_t seed, struct tactyk_asmvm__Context *ctx);

void cqsort(int64_t *data, int64_t low, int64_t high);
int64_t compute_hash(int64_t *data, int64_t len);
void randfill(int64_t *data, int64_t len, int64_t seed);
void print_a_few(int64_t *data, int64_t amount);
void checkit(int64_t *data, int64_t len);
#endif /* QS_TEST__INCLUDE_GUARD */
