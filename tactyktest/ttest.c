#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

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

#define OBJECT_CTX 1
#define OBJECT_STASH 2
#define OBJECT_VM 3

// representation of an individual value test
struct tactyk_test_entry {
    uint64_t test_class;        // what logic to apply
    uint64_t object;            // which object to inspect
    uint64_t element_offset;    // offset within the offset to read
    uint64_t array_offset;      // If the object is located in an array, which object within the array
                                //  (the selector chosen with the 'object' field will understand what sort of array it is)
    union {                     // expected value (the tester chosen with the 'test_class' will use the appropriate entry)
        uint8_t i8;
        uint8_t i16;
        uint8_t i32;
        uint8_t i64;
        float f32;
        double f64;
    } expected;
};

typedef uint64_t (*tactyk_emit__test_func)(struct tactyk_dblock__DBlock *data);

struct tactyk_test__Context {
    struct tactyk_pl__Context *plctx;
    struct tactyk_asmvm__Context *vmctx;
    struct tactyk_asmvm__Program *program;
};

struct tactyk_test__Context *tctx;

uint64_t tactyk_test__next();
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

struct tactyk_dblock__DBlock *base_tests;

struct tactyk_emit__Context *emitctx;
struct tactyk_asmvm__VM *vm;
struct tactyk_dblock__DBlock *contexts;
struct tactyk_dblock__DBlock *test_functions;

struct tactyk_dblock__DBlock *DEFAULT_NAME;

struct tactyk_dblock__DBlock *test_spec;

int main(int argc, char *argv[], char *envp[]) {
    tactyk_init();
    printf("%s\n", TACTYK_TEST__DESCRIPTION);
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

    //struct tactyk_dblock__DBlock *test_src = tactyk_dblock__from_bytes(NULL, fbytes, 0, (uint64_t)len, true);
    struct tactyk_dblock__DBlock *test_src = tactyk_dblock__from_safe_c_string(test1);
    tactyk_dblock__fix(test_src);
    tactyk_dblock__tokenize(test_src, '\n', false);
    test_spec = tactyk_dblock__remove_blanks(test_src, ' ', '#');
    tactyk_dblock__stratify(test_spec, ' ');
    tactyk_dblock__trim(test_spec);
    tactyk_dblock__tokenize(test_spec, ' ', true);
    tactyk_dblock__set_persistence_code(test_spec, 100);


    tactyk_dblock__set_persistence_code(test_src, 1000);
    tactyk_dblock__set_persistence_code(test_spec, 1000);
    tactyk_dblock__set_persistence_code(base_tests, 1000);
    tactyk_dblock__set_persistence_code(test_functions, 1000);
    tactyk_dblock__set_persistence_code(contexts, 1000);

    uint64_t test_result = TEST_RESULT__PASS;

    printf("\n");
    while (true) {
        uint64_t tresult = tactyk_test__next();
        switch(tresult) {
            case TEST_RESULT__PASS: {
                test_result = tresult;
                break;
            }
            case TEST_RESULT__FAIL: {
                tactyk_test__exit(TEST_RESULT__FAIL);
                break;
            }
            case TEST_RESULT__EXIT: {
                tactyk_test__exit(test_result);
                break;
            }
            // impossible condition
            default: {
                tactyk_test__exit(TEST_RESULT__TEST_ERROR);
                break;
            }
        }
    }

    printf("done.\n");
    return 0;
}

void tactyk_test__report(char *msg) {
    puts(msg);
}

void tactyk_test__exit(uint64_t test_result) {
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
uint64_t tactyk_test__next() {
    if (test_spec == NULL) {
        return TEST_RESULT__EXIT;
    }
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
            test_spec = test_spec->next;
            return tresult;
        }
    }
    else {
        return TEST_RESULT__EXIT;
    }
}



uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec) {
    printf("PROGRAM: \n");
    //tactyk_dblock__print_structure_simple(spec->child);

    if (tctx->plctx == NULL) {
        tctx->plctx = tactyk_pl__new(emitctx);
    }
    tactyk_pl__load_dblock(tctx->plctx, spec->child);
    printf("PROGRAM-EXIT\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__BUILD(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *name = spec->token->next;

    // load or instantiate a named context if a name is provided.
    if (name != NULL) {
        printf("new-tctx\n");
        tctx = tactyk_dblock__get(contexts, name);
        if (tctx == NULL) {
            tctx = tactyk_dblock__new_managedobject(contexts, name);
        }
    }

    printf("BUILD: ");
    tactyk_dblock__println(spec->token->next);
    if (tctx->plctx == NULL) {
        tactyk_test__report("No loaded code");
        return TEST_RESULT__FAIL;
    }

    struct tactyk_asmvm__Program *prg = tactyk_pl__build(tctx->plctx);
    tctx->program = prg;
    tctx->vmctx = tactyk_asmvm__new_context(vm);
    tactyk_asmvm__add_program(tctx->vmctx, tctx->program);

    printf("BUILD-exit.\n");
    return TEST_RESULT__PASS;

}
uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec) {
    printf("EXEC: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__RECV_CCALL(struct tactyk_dblock__DBlock *spec) {
    printf("RECV-CCALL: ");
    tactyk_dblock__println(spec->token->next);
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__RECV_TCALL(struct tactyk_dblock__DBlock *spec) {
    printf("RECV-TCALL: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec) {
    printf("TEST: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec) {
    printf("STATE: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec) {
    printf("DATA: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec) {
    printf("REF: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
}

uint64_t tactyk_test__CONTEXT(struct tactyk_dblock__DBlock *spec) {
    printf("CONTEXT: ");
    tactyk_dblock__println(spec->token->next);
    printf("\n");
    return TEST_RESULT__PASS;
/*
    if (token == NULL) {
        token = DEFAULT_NAME;
    }
    ctx = tactyk_dblock__get(contexts, token);
    if (ctx == NULL) {
        ctx = tactyk_dblock__new_managedobject(contexts, token);
    }
    return TEST_RESULT__PASS;
    */
}
