#include <stdint.h>
#include <assert.h>

#include "ttest.h"


uint64_t tactyk_test__RECV_CCALL_1(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f) {
    ccall_args[0] = a;
    ccall_args[1] = b;
    ccall_args[2] = c;
    ccall_args[3] = d;
    ccall_args[4] = e;
    ccall_args[5] = f;
    callback_id = 1;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
    return ccall_retval;
}
uint64_t tactyk_test__RECV_CCALL_2(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f) {
    ccall_args[0] = a;
    ccall_args[1] = b;
    ccall_args[2] = c;
    ccall_args[3] = d;
    ccall_args[4] = e;
    ccall_args[5] = f;
    callback_id = 2;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
    return ccall_retval;
}
uint64_t tactyk_test__RECV_CCALL_3(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f) {
    ccall_args[0] = a;
    ccall_args[1] = b;
    ccall_args[2] = c;
    ccall_args[3] = d;
    ccall_args[4] = e;
    ccall_args[5] = f;
    callback_id = 3;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
    return ccall_retval;
}
uint64_t tactyk_test__RECV_CCALL_4(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f) {
    ccall_args[0] = a;
    ccall_args[1] = b;
    ccall_args[2] = c;
    ccall_args[3] = d;
    ccall_args[4] = e;
    ccall_args[5] = f;
    callback_id = 4;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
    return ccall_retval;
}
void tactyk_test__RECV_TCALL_5(struct tactyk_asmvm__Context *ctx) {
    callback_id = 5;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
}
void tactyk_test__RECV_TCALL_6(struct tactyk_asmvm__Context *ctx) {
    callback_id = 6;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
}
void tactyk_test__RECV_TCALL_7(struct tactyk_asmvm__Context *ctx) {
    callback_id = 7;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
}
void tactyk_test__RECV_TCALL_8(struct tactyk_asmvm__Context *ctx) {
    callback_id = 8;
    test_continuation = NULL;
    struct tactyk_dblock__DBlock *spec = *active_test_spec;
    assert(spec != NULL);
    if (spec->child != NULL) {
        tactyk_test__exec_test_commands(spec->child);
        spec->child = test_continuation;
    }
    *active_test_spec = spec;
}
