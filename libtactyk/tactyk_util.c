//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "tactyk.h"
#include "tactyk_util.h"

// Return a power of two which is equal to or greater than the input.
//  source (license public domain):  https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint64_t tactyk_util__next_pow2(uint64_t val) {
    uint64_t v = val-1;
    if ((v&val) == 0) {
        return val;
    }
    v |= v>>1;
    v |= v>>2;
    v |= v>>4;
    v |= v>>8;
    v |= v>>16;
    v |= v>>32;
    return v+1;
}


bool tactyk_util__is_intstring(char* str) {
    int64_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    int64_t i = 0;
    if (str[0] == '-') {
        i += 1;
        if (len == 1) {
            return false;
        }
    }
    for (; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool tactyk_util__try_parseint(int64_t *out, char *str, bool permissive) {
    int32_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    int64_t result = 0;
    int64_t sign = 1;
    int64_t i = 0;
    int64_t ii = 0;
    if (str[0] == '-') {
        sign = -1;
        i += 1;
        ii += 1;
    }
    for (; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '9') {
            if (permissive && (i > ii)) {
                goto yield_output;
            }
            return false;
        }
        int64_t v = c - (int64_t)'0';
        result *= 10;
        result += v;
    }

    yield_output:
    *out = result * sign;
    return true;
}

bool tactyk_util__is_uintstring(char* str) {
    int64_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    int64_t i = 0;
    for (; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool tactyk_util__try_parsedouble(double *out, char *str, uint64_t str_len) {
    char *tail = NULL;
    char *end = &str[str_len];
    errno = 0;
    *out = strtod(str, &tail);
    if (tail != end) {
        return false;
    }
    if (errno == 0) {
        return true;
    }
    else {
        return false;
    }
}
bool tactyk_util__try_parselongdouble(long double *out, char *str, uint64_t str_len) {
    char *tail = NULL;
    char *end = &str[str_len];
    errno = 0;
    *out = strtold(str, &tail);
    if (tail != end) {
        return false;
    }
    if (errno == 0) {
        return true;
    }
    else {
        return false;
    }
}

bool tactyk_util__try_parseuint(uint64_t *out, char *str, bool permissive) {
    int32_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    int64_t result = 0;
    int64_t i = 0;
    for (; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '9') {
            if (permissive && (i > 0)) {
                goto yield_output;
            }
            return false;
        }
        int64_t v = c - (int64_t)'0';
        result *= 10;
        result += v;
    }
    yield_output:
    *out = result;
    return true;
}

void tactyk_util__lcase(char *txt, int32_t max_length)
{
    for (int32_t i = 0; i < max_length; i++)
    {
        char ch = txt[i];
        if (ch == 0)
        {
            return;
        }
        txt[i] = tolower(ch);
    }
}
void tactyk_util__ucase(char *txt, int32_t max_length)
{
    for (int32_t i = 0; i < max_length; i++)
    {
        char ch = txt[i];
        if (ch == 0)
        {
            return;
        }
        txt[i] = toupper(ch);
    }
}

//Pick a random integer between 0 and max
//This must be uniformly random and taken from a secure pseudo-random number generator.
// the secure PRNG is /dev/urandom
// the uniformity is accomplished by rounding the maximum uint64 down to an integer divisible by max,
// then discarding any excessively high random values
//      (if accepted, these high values would slightly bias the result of binary executable randomization randomization)
// NOTE:  This particular method of selecting secure random numbers should not be considered secure, as it also introduces potentially
//        observable effects on system timing (I think that this is a non-issue, but that assessment is best deferred to actual experts)
//        Additionally, division *slow* and the shuffling loop is not vectorizable due to random read-write memory access (the division
//        operation is independent of all non-deterministic aspects of the loop, so it maybe will be optimized a bit).
uint64_t tactyk_util__rand_range(uint64_t range) {
    uint64_t max_rnd = 0xffffffffffffffff / range;
    max_rnd *= range;
    uint64_t result;
    do {
        result = tactyk__rand_uint64();
    } while (result > max_rnd);
    result %= range;
    return result;
}
