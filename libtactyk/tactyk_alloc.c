#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "tactyk.h"
#include "tactyk_alloc.h"

// this may be an excessive number of mempry paged to request from the OS
#define TACTYK_ALLOC__MAX_PAGES 65536

struct tactyk_m_entry {
    void *ptr;
    uint64_t sz;
};

uint64_t tactyk_m_count;
struct tactyk_m_entry *tactyk_m_tbl;

void tactyk_alloc__init() {
    tactyk_m_count = 0;
    void* t_addr = tactyk__mk_random_base_address();
    tactyk_m_tbl = (struct tactyk_m_entry*) mmap(t_addr, sizeof(struct tactyk_m_entry) * TACTYK_ALLOC__MAX_PAGES, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
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

void* tactyk_alloc__allocate(uint64_t num, uint64_t sz) {
    #ifdef USE_TACTYK_ALLOCATOR
    if ( (((num>>1) + (sz>>1)) >> 63 ) != 0) {
        error("TACTYK-talloc -- integer overflow detected", NULL);
    }
    uint64_t m_size = num*sz;
    void* t_addr = tactyk__mk_random_base_address();
    void *ptr = mmap(t_addr, m_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    uint64_t pivot = (((uint64_t)ptr)>>16) & (TACTYK_ALLOC__MAX_PAGES-1);

    for (int64_t i = pivot; i < TACTYK_ALLOC__MAX_PAGES; i += 1) {
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

void tactyk_alloc__free(void *ptr) {
    #ifdef USE_TACTYK_ALLOCATOR
    uint64_t pivot = (((uint64_t)ptr)>>16) & (TACTYK_ALLOC__MAX_PAGES-1);
    for (int64_t i = pivot; i < TACTYK_ALLOC__MAX_PAGES; i += 1) {
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
}
