#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

#include "tactyk.h"
#include "tactyk_alloc.h"
#include "tactyk_pl.h"
#include "tactyk_report.h"

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

// overridable error handler
tactyk__error_handler error;
tactyk__error_handler warn;
static jmp_buf tactyk_err_jbuf;

void tactyk__default_warning_handler(char *msg, void *data) {
    if (msg != NULL) {
        if (data == NULL) {
            printf("WARNING -- %s\n", msg);
        }
        else {
            printf("WARNING -- %s: ", msg);
            tactyk_dblock__println(data);
        }
    }
    printf("WARNING-REPORT\n");
    printf("--------------\n");
    printf("%s\n", tactyk_report__get());
}

void tactyk__default_error_handler(char *msg, void *data) {
    if (msg != NULL) {
        if (data == NULL) {
            printf("ERROR -- %s\n", msg);
        }
        else {
            printf("ERROR -- %s: ", msg);
            tactyk_dblock__println(data);
        }
    }
    printf("ERROR-REPORT\n");
    printf("------------\n");
    printf("%s\n", tactyk_report__get());
    exit(0);

    longjmp(tactyk_err_jbuf, 1);
}


void tactyk_init() {
    error = tactyk__default_error_handler;
    warn = tactyk__default_warning_handler;
    
    tactyk_report__init();

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

    tactyk_pl__init();
}

