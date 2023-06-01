//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_UTIL__INCLUDE_GUARD
#define TACTYK_UTIL__INCLUDE_GUARD

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TACTYK_PSEUDONULL ((void*)1)

union tactyk_util__float_int {
    double fval;
    int64_t ival;
};

uint64_t tactyk_util__next_pow2(uint64_t val);
bool tactyk_util__is_intstring(char* str);
bool tactyk_util__try_parseint(int64_t *out, char *str, bool permissive);
bool tactyk_util__is_uintstring(char* str);
bool tactyk_util__try_parseuint(uint64_t *out, char *str, bool permissive);
bool tactyk_util__try_parsedouble(double *out, char *str, uint64_t max_len);
bool tactyk_util__try_parselongdouble(long double *out, char *str, uint64_t str_len);
void tactyk_util__lcase(char *txt, int32_t max_length);
void tactyk_util__ucase(char *txt, int32_t max_length);
uint64_t tactyk_util__rand_range(uint64_t range);

#endif // TACTYK_UTIL__INCLUDE_GUARD

