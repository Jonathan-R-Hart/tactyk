#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include "tactyk.h"
#include "tactyk_emit.h"
#include "tactyk_emit_svc.h"
#include "tactyk_visa.h"
#include "tactyk_pl.h"
#include "tactyk_debug.h"
#include "tactyk_dblock.h"
#include "tactyk_alloc.h"

char *test1 = R"""(
# TACTYK Automated testing plan
#
# The testing system should be a collection of test scripts.
# Each script is to specify a sequence of actions to perform
# The PROGRAM opreation defines a script to compile.
# The EXEC operation invokes a TACTYK function.
# The test passes if each TEST and RECV operations recognize resulting state changes in TACTYK.
#   TEST and RECV perform the same thing, but differ somewhat in invocation
#   TEST is to run automatically when a TACTYK program yields or returns
#   RECV is to be invoked directly by a TACTYK program (through a ccall or tcall)
# Supplemental operations are used to force TACTYK to have a specific state (for more consise test setup).
#   These generally are going to be writes to registers, writes to defined memory
#   These are to be run either between RUN operations or at the end of RECV operations.

CONTEXT prg1
PROGRAM
  MAIN:
    add a b
    add xa a
    add xb xa
    # Just for a barely passable result (default precision for floating point tests is DBL_EPSILON * 256)
    add xb 0.00000000000005773159728
    exit

BUILD

STATE
  rA 1
  rB 4
  xA 1.43
  xB 2.48

EXEC MAIN

TEST
  rA 5
  rB 4
  xA 6.43
  xB 8.91
)""";

#define TEST_RESULT__EXIT 0
#define TEST_RESULT__PASS 1
#define TEST_RESULT__FAIL 2
#define TEST_RESULT__TEST_ERROR 3

#define TEST_CLASS_I8 1
#define TEST_CLASS_I16 2
#define TEST_CLASS_I32 3
#define TEST_CLASS_I64 4
#define TEST_CLASS_F32 5
#define TEST_CLASS_F64 6
#define TEST_CLASS_ADDRESS 7

#define OBJECT_REGISTERFILE 0
#define OBJECT_CTX 1
#define OBJECT_CTX_STACK_DECLARATION 2
#define OBJECT_CTX_STACK_ITEMS 3
#define OBJECT_STASH 4
#define OBJECT_VM 5
#define OBJECT_VM_PROGRAM_DECLARATION 6

struct tactyk_test_entry;
union tactyk_test__value;

typedef bool (tactyk_test__STATE_ITEM) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value);
typedef uint64_t (tactyk_test__TEST_ITEM) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value);

// abstract test handler
struct tactyk_test_entry {
    tactyk_test__STATE_ITEM *state_adjuster;        // State adjustment function to invoke for STATE operation
    tactyk_test__TEST_ITEM *test_applicator;        // Test function to use for TEST operation
    uint64_t element_offset;    // object member to access
    uint64_t array_offset;      // If the object is located in an array, which object within the array to access
};

union tactyk_test__value {
    uint8_t i8;
    uint8_t i16;
    uint8_t i32;
    uint8_t i64;
    float f32;
    double f64;
};

typedef uint64_t (*tactyk_emit__test_func)(struct tactyk_dblock__DBlock *data);

struct tactyk_test__Context {
    struct tactyk_pl__Context *plctx;
    struct tactyk_asmvm__Context *vmctx;
    struct tactyk_asmvm__Program *program;
    double precision;
};

struct tactyk_test__Context *tctx;


uint64_t tactyk_test__run(struct tactyk_dblock__DBlock *test_spec);
uint64_t tactyk_test__next(struct tactyk_dblock__DBlock *test_spec);
void tactyk_test__exit(uint64_t test_result);

uint64_t tactyk_test__CONTEXT(struct tactyk_dblock__DBlock *token);
uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__BUILD(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RECV_CCALL(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RECV_TCALL(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec);

bool tactyk_test__SET_DATA_REGISTER (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value);
bool tactyk_test__SET_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value);
uint64_t tactyk_test__TEST_DATA_REGISTER(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value);
uint64_t tactyk_test__TEST_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value);
void tactyk_test__mk_data_register_test(char *name, uint64_t ofs);
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs);

struct tactyk_dblock__DBlock *base_tests;

struct tactyk_emit__Context *emitctx;
struct tactyk_asmvm__VM *vm;
struct tactyk_dblock__DBlock *contexts;
struct tactyk_dblock__DBlock *test_functions;

struct tactyk_dblock__DBlock *DEFAULT_NAME;

double DEFAULT_PRECISION = DBL_EPSILON * 256.0;

int main(int argc, char *argv[], char *envp[]) {
    tactyk_init();
    #ifdef TACTYK_DEBUG
    printf("%s\n", TACTYK_TEST__DESCRIPTION);
    #endif // TACTYK_DEBUG
    tactyk_visa__init("rsc/tactyk_core.visa");

    emitctx = tactyk_emit__init();
    emitctx->visa_file_prefix = "rsc/";

    tactyk_visa__init_emit(emitctx);
    tactyk_pl__init();
    vm = tactyk_asmvm__new_vm();

    tactyk_debug__configure_api(emitctx);
    tactyk_emit_svc__configure(emitctx);

    contexts = tactyk_dblock__new_managedobject_table(64, sizeof(struct tactyk_test__Context));
    DEFAULT_NAME = tactyk_dblock__from_safe_c_string("DEFAULT");
    tctx = tactyk_dblock__new_managedobject(contexts, DEFAULT_NAME);
    tctx->precision = DEFAULT_PRECISION;
    test_functions = tactyk_dblock__new_table(64);

    tactyk_dblock__put(test_functions, "CONTEXT", tactyk_test__CONTEXT);
    tactyk_dblock__put(test_functions, "PROGRAM", tactyk_test__PROGRAM);
    tactyk_dblock__put(test_functions, "BUILD", tactyk_test__BUILD);
    tactyk_dblock__put(test_functions, "EXEC", tactyk_test__EXEC);
    tactyk_dblock__put(test_functions, "RECV_CCALL", tactyk_test__RECV_CCALL);
    tactyk_dblock__put(test_functions, "RECV_TCALL", tactyk_test__RECV_TCALL);
    tactyk_dblock__put(test_functions, "TEST", tactyk_test__TEST);
    tactyk_dblock__put(test_functions, "STATE", tactyk_test__STATE);
    tactyk_dblock__put(test_functions, "DATA", tactyk_test__DATA);
    tactyk_dblock__put(test_functions, "REF", tactyk_test__REF);

    base_tests = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_test_entry));
    tactyk_test__mk_data_register_test("rA", 0);
    tactyk_test__mk_data_register_test("rB", 1);
    tactyk_test__mk_data_register_test("rC", 2);
    tactyk_test__mk_data_register_test("rD", 3);
    tactyk_test__mk_data_register_test("rE", 4);
    tactyk_test__mk_data_register_test("rF", 5);
    tactyk_test__mk_xmm_register_test("xA", 0);
    tactyk_test__mk_xmm_register_test("xB", 1);
    tactyk_test__mk_xmm_register_test("xC", 2);
    tactyk_test__mk_xmm_register_test("xD", 3);
    tactyk_test__mk_xmm_register_test("xE", 4);
    tactyk_test__mk_xmm_register_test("xF", 5);
    tactyk_test__mk_xmm_register_test("xG", 6);
    tactyk_test__mk_xmm_register_test("xH", 7);
    tactyk_test__mk_xmm_register_test("xI", 8);
    tactyk_test__mk_xmm_register_test("xJ", 9);
    tactyk_test__mk_xmm_register_test("xK", 10);
    tactyk_test__mk_xmm_register_test("xL", 11);
    tactyk_test__mk_xmm_register_test("xM", 12);
    tactyk_test__mk_xmm_register_test("xN", 13);
    tactyk_test__mk_xmm_register_test("xTEMPA", 14);
    tactyk_test__mk_xmm_register_test("xTEMPB", 15);

    //struct tactyk_dblock__DBlock *test_src = tactyk_dblock__from_bytes(NULL, fbytes, 0, (uint64_t)len, true);
    struct tactyk_dblock__DBlock *test_src = tactyk_dblock__from_safe_c_string(test1);
    tactyk_dblock__fix(test_src);
    tactyk_dblock__tokenize(test_src, '\n', false);
    struct tactyk_dblock__DBlock *test_spec = tactyk_dblock__remove_blanks(test_src, ' ', '#');
    tactyk_dblock__stratify(test_spec, ' ');
    tactyk_dblock__trim(test_spec);
    tactyk_dblock__tokenize(test_spec, ' ', true);
    tactyk_dblock__set_persistence_code(test_spec, 100);

    tactyk_dblock__set_persistence_code(test_src, 1000);
    tactyk_dblock__set_persistence_code(test_spec, 1000);
    tactyk_dblock__set_persistence_code(base_tests, 1000);
    tactyk_dblock__set_persistence_code(test_functions, 1000);
    tactyk_dblock__set_persistence_code(contexts, 1000);

    uint64_t test_result = tactyk_test__run(test_spec);
    tactyk_test__exit(test_result);
    return 0;
}

uint64_t tactyk_test__run(struct tactyk_dblock__DBlock *test_spec) {
    while (test_spec != NULL) {
        uint64_t tresult = tactyk_test__next(test_spec);
        switch(tresult) {
            case TEST_RESULT__PASS: {
                break;
            }
            case TEST_RESULT__FAIL: {
                tactyk_test__exit(TEST_RESULT__FAIL);
                break;
            }
            case TEST_RESULT__EXIT: {
                tactyk_test__exit(TEST_RESULT__PASS);
                break;
            }
            // impossible condition
            default: {
                tactyk_test__exit(TEST_RESULT__TEST_ERROR);
                break;
            }
        }
        test_spec = test_spec->next;
    }
    return TEST_RESULT__PASS;
}

void tactyk_test__report(char *msg) {
    puts(msg);
}

void tactyk_test__exit(uint64_t test_result) {
    #ifdef TACTYK_DEBUG
    tactyk_debug__print_context(tctx->vmctx);
    #endif // TACTYK_DEBUG
    switch(test_result) {
        case TEST_RESULT__PASS: {
            printf("pass\n");
            exit(0);
            break;
        }
        case TEST_RESULT__FAIL: {
            printf("fail\n");
            exit(1);
            break;
        }
        case TEST_RESULT__TEST_ERROR:
        case TEST_RESULT__EXIT:
        default:{
            printf("test error\n");
            exit(2);
            break;
        }
    }
}

// perform the next action.
// This is called both by the while loop in the main function, and by test callbacks.
//      (the use by test callbacks is for handling RECV tests without either complicating the test framework or disrupting control flow)
uint64_t tactyk_test__next(struct tactyk_dblock__DBlock *test_spec) {
    uint64_t tresult = 0;
    struct tactyk_dblock__DBlock *token = test_spec->token;
    if (token != NULL) {
        tactyk_emit__test_func tfunc = tactyk_dblock__get(test_functions, test_spec->token);
        if (tfunc == NULL) {
            error("Undefined test function", test_spec);
            return TEST_RESULT__FAIL;
        }
        else {
            tresult = tfunc(test_spec);
            return tresult;
        }
    }
    else {
        return TEST_RESULT__EXIT;
    }
}

uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec) {
    if (tctx->plctx == NULL) {
        tctx->plctx = tactyk_pl__new(emitctx);
    }
    tactyk_pl__load_dblock(tctx->plctx, spec->child);
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__BUILD(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *name = spec->token->next;

    // load or instantiate a named context if a name is provided.
    if (name != NULL) {
        printf("new-tctx\n");
        struct tactyk_test__Context *ntctx = tactyk_dblock__get(contexts, name);
        if (ntctx == NULL) {
            ntctx = tactyk_dblock__new_managedobject(contexts, name);
            ntctx->plctx = tctx->plctx;
            ntctx->precision = tctx->precision;
        }
        tctx = ntctx;
    }

    if (tctx->plctx == NULL) {
        tactyk_test__report("No loaded code");
        return TEST_RESULT__FAIL;
    }

    struct tactyk_asmvm__Program *prg = tactyk_pl__build(tctx->plctx);
    tctx->program = prg;
    tctx->vmctx = tactyk_asmvm__new_context(vm);
    tactyk_asmvm__add_program(tctx->vmctx, tctx->program);

    return TEST_RESULT__PASS;
}

uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *func_name = spec->token->next;
    if ( (tctx->vmctx == NULL) || (tctx->program == NULL) ) {
        tactyk_test__report("Program not built");
        return TEST_RESULT__TEST_ERROR;
    }
    if (func_name == NULL) {
        tactyk_asmvm__invoke(tctx->vmctx, tctx->program, "MAIN");
    }
    else {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, func_name);
        tactyk_asmvm__invoke(tctx->vmctx, tctx->program, buf);
    }
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__RECV_CCALL(struct tactyk_dblock__DBlock *spec) {
    // should add room for a function args list to tactyk_test__Context
    // first a check to ensure that this is being called from a callback
    // Then args from the callback should be compared with params from the test specification and fail if anyare wrong.
    // Then the args should be invalidated so subsequent calls don't make
    tactyk_test__report("Test function 'RECV_CCALL' is not implemented");
    return TEST_RESULT__TEST_ERROR;

    // callback prologue (for recursive tests)
    return tactyk_test__run(spec->child);
}
uint64_t tactyk_test__RECV_TCALL(struct tactyk_dblock__DBlock *spec) {
    // this isn't special. Just need to verify that this was called from the correct callback.
    tactyk_test__report("Test function 'RECV_TCALL' is not implemented");
    return TEST_RESULT__TEST_ERROR;

    // callback prologue (for recursive tests)
    return tactyk_test__run(spec->child);
}
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec) {
    if ( tctx->vmctx == NULL) {
        tactyk_test__report("No asmvm context");
        return TEST_RESULT__TEST_ERROR;
    }
    struct tactyk_dblock__DBlock *td = spec->child;
    while (td != NULL) {
        struct tactyk_dblock__DBlock *item_name = td->token;
        assert(item_name != NULL);
        struct tactyk_dblock__DBlock *item_value = td->token->next;

        if (item_value == NULL) {
            tactyk_test__report("Unspecified item value");
            return TEST_RESULT__TEST_ERROR;
        }

        struct tactyk_test_entry *test = tactyk_dblock__get(base_tests, item_name);
        if (test == NULL) {
            char buf[256];
            sprintf(buf, "No handler for test item: ");
            tactyk_dblock__export_cstring(&buf[strlen(buf)], 256-strlen(buf), item_name );
            tactyk_test__report(buf);
            return TEST_RESULT__TEST_ERROR;
        }

        uint64_t tresult = test->test_applicator(test, item_value);
        if (tresult != TEST_RESULT__PASS) {
            return tresult;
        }

        td = td->next;
    }
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec) {
    if ( tctx->vmctx == NULL) {
        tactyk_test__report("No asmvm context");
        return TEST_RESULT__TEST_ERROR;
    }
    struct tactyk_dblock__DBlock *td = spec->child;
    while (td != NULL) {
        struct tactyk_dblock__DBlock *item_name = td->token;
        assert(item_name != NULL);
        struct tactyk_dblock__DBlock *item_value = td->token->next;
        if (item_value == NULL) {
            tactyk_test__report("Unspecified item value");
            return TEST_RESULT__TEST_ERROR;
        }

        struct tactyk_test_entry *test = tactyk_dblock__get(base_tests, item_name);

        if (test == NULL) {
            char buf[256];
            sprintf(buf, "No handler for test item: ");
            tactyk_dblock__export_cstring(&buf[strlen(buf)], 256-strlen(buf), item_name );
            tactyk_test__report(buf);
            return TEST_RESULT__TEST_ERROR;
        }

        if (!test->state_adjuster(test, item_value)) {
            return TEST_RESULT__TEST_ERROR;
        }

        td = td->next;
    }
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *mem_target = spec->token->next;
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *mem_target = spec->token->next;
    return TEST_RESULT__PASS;
}

uint64_t tactyk_test__CONTEXT(struct tactyk_dblock__DBlock *spec) {
    return TEST_RESULT__PASS;
}

bool tactyk_test__SET_DATA_REGISTER (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value) {
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return false;
    }
    switch(entry->element_offset) {
        case 0: {
            tctx->vmctx->reg.rA = (uint64_t) ival;
            return true;
        }
        case 1: {
            tctx->vmctx->reg.rB = (uint64_t) ival;
            return true;
        }
        case 2: {
            tctx->vmctx->reg.rC = (uint64_t) ival;
            return true;
        }
        case 3: {
            tctx->vmctx->reg.rD = (uint64_t) ival;
            return true;
        }
        case 4: {
            tctx->vmctx->reg.rE = (uint64_t) ival;
            return true;
        }
        case 5: {
            tctx->vmctx->reg.rF = (uint64_t) ival;
            return true;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return false;
        }
    }
}

uint64_t tactyk_test__TEST_DATA_REGISTER(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value) {
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TEST_RESULT__TEST_ERROR;
    }
    bool pass = false;
    switch(entry->element_offset) {
        case 0: {
            pass = tctx->vmctx->reg.rA == (uint64_t) ival;
            break;
        }
        case 1: {
            pass = tctx->vmctx->reg.rB == (uint64_t) ival;
            break;
        }
        case 2: {
            pass = tctx->vmctx->reg.rC == (uint64_t) ival;
            break;
        }
        case 3: {
            pass = tctx->vmctx->reg.rD == (uint64_t) ival;
            break;
        }
        case 4: {
            pass = tctx->vmctx->reg.rE == (uint64_t) ival;
            break;
        }
        case 5: {
            pass = tctx->vmctx->reg.rF == (uint64_t) ival;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TEST_RESULT__TEST_ERROR;
        }
    }
    if (pass) {
        return TEST_RESULT__PASS;
    }
    else {
        return TEST_RESULT__FAIL;
    }
}

bool tactyk_test__SET_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value) {
    double fval = 0;
    if (!tactyk_dblock__try_parsedouble(&fval, value)) {
        tactyk_test__report("Test parameter is not a floating point number");
        return false;
    }
    switch(entry->element_offset) {
        case 0: {
            tctx->vmctx->reg.xa.f64[0] = fval;
            break;
        }
        case 1: {
            tctx->vmctx->reg.xb.f64[0] = fval;
            break;
        }
        case 2: {
            tctx->vmctx->reg.xc.f64[0] = fval;
            break;
        }
        case 3: {
            tctx->vmctx->reg.xd.f64[0] = fval;
            break;
        }
        case 4: {
            tctx->vmctx->reg.xe.f64[0] = fval;
            break;
        }
        case 5: {
            tctx->vmctx->reg.xf.f64[0] = fval;
            break;
        }
        case 6: {
            tctx->vmctx->reg.xg.f64[0] = fval;
            break;
        }
        case 7: {
            tctx->vmctx->reg.xh.f64[0] = fval;
            break;
        }
        case 8: {
            tctx->vmctx->reg.xi.f64[0] = fval;
            break;
        }
        case 9: {
            tctx->vmctx->reg.xj.f64[0] = fval;
            break;
        }
        case 10: {
            tctx->vmctx->reg.xk.f64[0] = fval;
            break;
        }
        case 11: {
            tctx->vmctx->reg.xl.f64[0] = fval;
            break;
        }
        case 12: {
            tctx->vmctx->reg.xm.f64[0] = fval;
            break;
        }
        case 13: {
            tctx->vmctx->reg.xn.f64[0] = fval;
            break;
        }
        case 14: {
            tctx->vmctx->reg.xTEMPA.f64[0] = fval;
            break;
        }
        case 15: {
            tctx->vmctx->reg.xTEMPB.f64[0] = fval;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TEST_RESULT__TEST_ERROR;
        }
    }
    return true;
}

uint64_t tactyk_test__TEST_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value) {
    double fval = 0;
    if (!tactyk_dblock__try_parsedouble(&fval, expected_value)) {
        tactyk_test__report("Test parameter is not a floating point number");
        return false;
    }
    double stval = 0;
    switch(entry->element_offset) {
        case 0: {
            stval = tctx->vmctx->reg.xa.f64[0];
            break;
        }
        case 1: {
            stval = tctx->vmctx->reg.xb.f64[0];
            break;
        }
        case 2: {
            stval = tctx->vmctx->reg.xc.f64[0];
            break;
        }
        case 3: {
            stval = tctx->vmctx->reg.xd.f64[0];
            break;
        }
        case 4: {
            stval = tctx->vmctx->reg.xe.f64[0];
            break;
        }
        case 5: {
            stval = tctx->vmctx->reg.xf.f64[0];
            break;
        }
        case 6: {
            stval = tctx->vmctx->reg.xg.f64[0];
            break;
        }
        case 7: {
            stval = tctx->vmctx->reg.xh.f64[0];
            break;
        }
        case 8: {
            stval = tctx->vmctx->reg.xi.f64[0];
            break;
        }
        case 9: {
            stval = tctx->vmctx->reg.xj.f64[0];
            break;
        }
        case 10: {
            stval = tctx->vmctx->reg.xk.f64[0];
            break;
        }
        case 11: {
            stval = tctx->vmctx->reg.xl.f64[0];
            break;
        }
        case 12: {
            stval = tctx->vmctx->reg.xm.f64[0];
            break;
        }
        case 13: {
            stval = tctx->vmctx->reg.xn.f64[0];
            break;
        }
        case 14: {
            stval = tctx->vmctx->reg.xTEMPA.f64[0];
            break;
        }
        case 15: {
            stval = tctx->vmctx->reg.xTEMPB.f64[0];
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TEST_RESULT__TEST_ERROR;
        }
    }
    stval = fabs(stval - fval);
    if (stval <= tctx->precision) {
        return TEST_RESULT__PASS;
    }
    else {
        return TEST_RESULT__FAIL;
    }
}

void tactyk_test__mk_data_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->state_adjuster = tactyk_test__SET_DATA_REGISTER;
    entry->test_applicator = tactyk_test__TEST_DATA_REGISTER;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->state_adjuster = tactyk_test__SET_XMM_REGISTER_FLOAT;
    entry->test_applicator = tactyk_test__TEST_XMM_REGISTER_FLOAT;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
