#ifndef TACTYK_ALLOC__INCLUDE_GUARD
#define TACTYK_ALLOC__INCLUDE_GUARD

void tactyk_alloc__init();
void* tactyk__mk_random_base_address();
void* tactyk_alloc__allocate(uint64_t num, uint64_t sz);
void tactyk_alloc__free(void* ptr);

#endif // TACTYK_ALLOC__INCLUDE_GUARD
