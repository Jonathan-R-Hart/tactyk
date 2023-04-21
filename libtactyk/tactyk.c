#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

#include "tactyk.h"
#include "tactyk_alloc.h"

// simple and safe random number generator
//          -- safe if safe means favoring a secure PRNG over something one has personally invented.
//          -- not so safe if safe is also supposed to mean 'unable to overwrite arbtrary amounts of arbitrarilly positioned memory.'
//          -- Some would say this is not a very portable approach, but the entire product line at issue got decertified for personal use years ago -- "free isn't freedom".
FILE *dev_urand;
void sys_rand(void *ptr, uint64_t nbytes) {
    uint64_t x = fread(ptr, nbytes, 1, dev_urand);
    x *= 2;
}

uint64_t tactyk__rand_uint64() {
    uint64_t rand;
    sys_rand(&rand, 8);
    return rand;
}

uint64_t tactyk__rand_range(uint64_t max) {

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
    uint64_t max_rnd = 0xffffffffffffffff / max;
    max_rnd *= max;
    uint64_t result;
    do {
        result = tactyk__rand_uint64();
    } while (result > max_rnd);
    result %= max;
    return result;
}

// overridable error handler
tactyk__error_handler error;
tactyk__error_handler warn;
static jmp_buf tactyk_err_jbuf;

void tactyk__default_warning_handler(char *msg, void *data) {
    if (data == NULL) {
        printf("WARNING -- %s\n", msg);
    }
    else {
        printf("WARNING -- %s: ", msg);
        tactyk_dblock__println(data);
    }
}

void tactyk__default_error_handler(char *msg, void *data) {
    if (data == NULL) {
        printf("ERROR -- %s\n", msg);
    }
    else {
        printf("ERROR -- %s: ", msg);
        tactyk_dblock__println(data);
    }
    longjmp(tactyk_err_jbuf, 1);
}


void tactyk_init() {
    error = tactyk__default_error_handler;
    warn = tactyk__default_warning_handler;

    // standalone erorr handling is to exit()
    // when invoked as library,t error handling should be to return NULL
    //      (the host application will have to override the error handler to get messages out)
    if (setjmp(tactyk_err_jbuf)) {
        printf("Error out.\n");
        exit(1);
    }

    dev_urand = fopen("/dev/urandom", "r");

    tactyk_dblock__init();

    #ifdef USE_TACTYK_ALLOCATOR
    tactyk_alloc__init();
    #endif // USE_TACTYK_ALLOCATOR
}

