#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

#include "tactyk.h"

#define TACTYK_M_CAPACITY 65536

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

struct tactyk_m_entry {
    void *ptr;
    uint64_t sz;
};

uint64_t tactyk_m_count;
struct tactyk_m_entry *tactyk_m_tbl;

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

// generate and return a pointer to a random address within the "tactyk" subset of user space
//      (address range 0x0000000100000000 to 0x00007fff00000000)
// The lower 4 GB of user space is clipped to avoid mapping address values which tactyk might place on stack registers
// The upper 4 GB is clipped to avoid potentially colliding with the top of user space.
void* tactyk__mk_random_base_address() {
    uint64_t addr = 0;
    do {
        addr = tactyk__rand_uint64() & 0x00007fffffffffff;
    } while ((addr <= 0xffffffff) || (addr >= 0x00007fff00000000));
    return (void*) addr;
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
    tactyk_m_count = 0;
    uint64_t t_addr = tactyk__mk_random_base_address();
    tactyk_m_tbl = (struct tactyk_m_entry*) mmap(t_addr, sizeof(struct tactyk_m_entry) * TACTYK_M_CAPACITY, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    #endif // USE_TACTYK_ALLOCATOR
}

void* talloc(uint64_t num, uint64_t sz) {
    #ifdef USE_TACTYK_ALLOCATOR
    if ( (((num>>1) + (sz>>1)) >> 63 ) != 0) {
        error("TACTYK-talloc -- integer overflow detected", NULL);
    }
    uint64_t m_size = num*sz;
    uint64_t t_addr = tactyk__mk_random_base_address();
    void *ptr = mmap(t_addr, m_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    uint64_t pivot = (((uint64_t)ptr)>>16) & (TACTYK_M_CAPACITY-1);

    for (int64_t i = pivot; i < TACTYK_M_CAPACITY; i += 1) {
        struct tactyk_m_entry *entry = &tactyk_m_tbl[i];
        if ((entry->ptr) == NULL) {
            entry->ptr = ptr;
            entry->sz = m_size;
            return ptr;
        }
    }
    for (int64_t i = 0; i < pivot; i += 1) {
        struct tactyk_m_entry *entry = &tactyk_m_tbl[i];
        if ((entry->ptr) == NULL) {
            entry->ptr = ptr;
            entry->sz = m_size;
            return ptr;
        }
    }
    return NULL;
    #else
    return calloc(num, sz);
    #endif // USE_TACTYK_ALLOCATOR
}
void tfree(void *ptr) {
    #ifdef USE_TACTYK_ALLOCATOR
    uint64_t pivot = (((uint64_t)ptr)>>16) & (TACTYK_M_CAPACITY-1);
    for (int64_t i = pivot; i < TACTYK_M_CAPACITY; i += 1) {
        struct tactyk_m_entry *entry = &tactyk_m_tbl[i];
        if ((entry->ptr) == ptr) {
            entry->ptr = NULL;
            munmap(ptr, entry->sz);
            entry->sz = 0;
            return;
        }
    }
    for (int64_t i = 0; i < pivot; i += 1) {
        struct tactyk_m_entry *entry = &tactyk_m_tbl[i];
        if ((entry->ptr) == ptr) {
            entry->ptr = NULL;
            munmap(ptr, entry->sz);
            entry->sz = 0;
            return;
        }
    }
    #else
    free(ptr);
    #endif // USE_TACTYK_ALLOCATOR
    //printf("tfree.done\n");
}
