#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

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

#define TACTYK_TESTSTATE__EXIT 0
#define TACTYK_TESTSTATE__PASS 1
#define TACTYK_TESTSTATE__FAIL 2
#define TACTYK_TESTSTATE__TEST_ERROR 3
#define TACTYK_TESTSTATE__RUNNING 4
#define TACTYK_TESTSTATE__PREPARING 5
#define TACTYK_TESTSTATE__INITIALIZING 6
#define TACTYK_TESTSTATE__INACTIVE 7

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

typedef bool (tactyk_test__VALUE_ADJUSTER) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *value);
typedef uint64_t (tactyk_test__VALUE_TESTER) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *expected_value);

// abstract test handler
struct tactyk_test_entry {
    tactyk_test__VALUE_ADJUSTER *adjust;        // Setter to use during STATE opreation
    tactyk_test__VALUE_TESTER *test;            // Test function to use for TEST operation
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


uint64_t tactyk_test__exec_test_commands(struct tactyk_dblock__DBlock *test_spec);
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
uint64_t tactyk_test__ERROR(struct tactyk_dblock__DBlock *spec);

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

struct tactyk_dblock__DBlock **active_test_spec;

#ifdef TACTYK_DEBUG
static bool debugmode = true;
#else
static bool debugmode = false;
#endif // TACTYK_DEBUG

#define TACTYK_TEST__FNAME_BUFSIZE (1<<10)
#define TACTYK_TEST__REPORT_BUFSIZE (1<<10)
struct tactyk_test__Status {
    uint64_t pid;
    uint64_t testid;
    uint64_t age;
    uint64_t max_age;
    uint64_t test_result;
    char fname[TACTYK_TEST__FNAME_BUFSIZE];
    char error[TACTYK_TEST__REPORT_BUFSIZE];
    char warning[TACTYK_TEST__REPORT_BUFSIZE];
    char report[TACTYK_TEST__REPORT_BUFSIZE];
    //uint64_t test_src_size;
    //char test_data[TACTYK_TEST__SRC_BUFSIZE];
};

void tactyk_test__prepare(struct tactyk_test__Status *tstate);
void tactyk_test__run(struct tactyk_test__Status *tstate);
void tactyk_test__reset_state(struct tactyk_test__Status *tstate);
void tactyk_test__await_start(struct tactyk_test__Status *tstate);

uint64_t test_count;
uint64_t tests_started;
uint64_t tests_completed;
struct tactyk_test__Status **tstate_list;
uint64_t num_;
char **testfilenames;

uint64_t max_active_jobs = 1;

int main(int argc, char *argv[], char *envp[]) {
    printf("%s\n", TACTYK_TEST__DESCRIPTION);
    tests_completed = 0;
    tests_started = 0;
    testfilenames = calloc(argc, sizeof(void*));
    for (uint64_t i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strncmp(arg, "--jobs=", 7) == 0) {
            if (!tactyk_util__try_parseuint(&max_active_jobs, &arg[7], false)) {
                printf("ERROR:  malformed argument:  '%s'\n", arg);
                exit(1);
            }
        }
        else {
            FILE *f = fopen(arg, "r");
            if (f == NULL) {
                printf("WARNING -- TEST FILE NOT FOUND: '%s'\n", arg);
                exit(1);
            }
            else {
                testfilenames[test_count] = arg;
                fclose(f);
                test_count += 1;
            }
        }
    }
    if (test_count == 0) {
        printf("Nothing to test.  Goodbye!\n");
        return 0;
    }
    if ( max_active_jobs >= test_count ) {
        max_active_jobs = test_count;
    }
    if (max_active_jobs <= 0) {
        max_active_jobs = 1;
    }
    // prepare one block of shared memory for each slot.
    //  This is to be used to get test results from child processes.
    tstate_list = calloc(max_active_jobs, sizeof(void*));
    for (uint64_t i = 0; i < max_active_jobs; i += 1) {
        struct tactyk_test__Status *ts = mmap(NULL, sizeof(struct tactyk_test__Status), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
        sprintf(ts->report, "");
        ts->test_result = TACTYK_TESTSTATE__INACTIVE;
        tstate_list[i] = ts;
    }

    uint64_t passed = 0;
    uint64_t failed = 0;
    uint64_t errored = 0;

    while (tests_completed < test_count) {
        for (uint64_t i = 0; i < max_active_jobs; i++) {
            struct tactyk_test__Status *tstate = tstate_list[i];
            switch(tstate->test_result) {
                // empty slot found.
                case TACTYK_TESTSTATE__INACTIVE: {
                    if (tests_started < test_count) {
                        // read file
                        tactyk_test__prepare(tstate);
                        int64_t pid = fork();
                        switch(pid) {
                            case -1: {
                                printf("ERROR:  fork() failed.  Is %ju processes perhaps too much?\n", max_active_jobs);
                                exit(1);
                                break;
                            }
                            case 0: {
                                // [ child process ]
                                tactyk_test__run(tstate);
                                break;
                            }
                            default: {
                                // [ parent process ]
                                tstate->pid = pid;
                                tactyk_test__await_start(tstate);
                                break;
                            }
                        }
                    }
                    else {
                        // All tests have at least been initiated.  Do nothing.
                    }
                    break;
                }
                case TACTYK_TESTSTATE__TEST_ERROR: {
                    errored += 1;
                    tests_completed += 1;
                    printf("TACTYK TEST ERROR -- '%s':\n", tstate->fname);
                    puts(tstate->report);
                    tactyk_test__reset_state(tstate);
                    printf("\n");
                    break;
                }
                case TACTYK_TESTSTATE__EXIT: {
                    errored += 1;
                    tests_completed += 1;
                    printf("TACTYK TEST ERROR -- '%s':\n", tstate->fname);
                    printf("Test returned invalid test result 'TEST_RESULT__EXIT'\n");
                    printf("TACTYK is **supposed** to return TEST_RESULT__PASS or TEST_RESULT__FAIL or TEST_RESULT__ERROR\n");
                    printf("Excellent work, TACTYK!\n");
                    puts(tstate->report);
                    printf("\n");
                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__PASS: {
                    passed += 1;
                    tests_completed += 1;
                    printf("PASS:  '%s'\n", tstate->fname);
                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__FAIL: {
                    failed += 1;
                    tests_completed += 1;
                    printf("FAIL:  '%s'\n", tstate->fname);
                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__INITIALIZING:
                case TACTYK_TESTSTATE__RUNNING: {
                    if (tstate->age >= tstate->max_age) {
                        errored += 1;
                        tests_completed += 1;
                        printf("TACTYK TEST ERROR -- '%s':\n", tstate->fname);
                        printf("Test appears to have hung.\n");
                        kill(tstate->pid, 1);
                        int status;
                        waitpid(-1, &status, 0);
                        tactyk_test__reset_state(tstate);
                    }
                    break;
                }
                default: {
                    errored += 1;
                    tests_completed += 1;
                    printf("TACTYK TEST ERROR -- '%s':\n", tstate->fname);
                    printf("Test returned unrecognized test result '%ju'\n", tstate->test_result);
                    printf("TACTYK is **supposed** to output a TEST_RESULT__PASS or TEST_RESULT__FAIL or TEST_RESULT__ERROR\n");
                    printf("Excellent work, TACTYK!\n");
                    puts(tstate->report);
                    printf("\n");
                    tactyk_test__reset_state(tstate);
                    break;
                }
            }
        }
        // 0.1 seconds
        usleep(100000);
    }

    printf("TEST RESULTS: \n");
    printf("Total tests: %ju\n", test_count);
    printf("passed: %ju\n", passed);
    printf("failed: %ju\n", failed);
    printf("test errors: %ju\n", errored);

    // release shared memory.
    for (uint64_t i = 0; i < max_active_jobs; i += 1) {
        struct tactyk_test__Status *ts = tstate_list[i];
        munmap(ts, sizeof(struct tactyk_test__Status));
    }
    free(tstate_list);
    return 0;
}
void tactyk_test__prepare(struct tactyk_test__Status *tstate) {
    //printf("prepare...\n");
    //printf("tstarted: %ju\n", tests_started);
    //printf("numtests: %ju\n", test_count);
    memset(tstate->fname, 0, TACTYK_TEST__FNAME_BUFSIZE);
    strncpy(tstate->fname, testfilenames[tests_started], TACTYK_TEST__FNAME_BUFSIZE-1);
    //printf("fptr:  %p\n", testfiles);
    //strncpy(tstate->fname, finf->fname, sizeof(finf->fname));
    tstate->max_age = 1000;
    tstate->pid = 0;
    tstate->age = 0;
    tstate->test_result = TACTYK_TESTSTATE__PREPARING;
    tstate->testid = tests_started;
    tests_started += 1;
}

void tactyk_test__reset_state(struct tactyk_test__Status *tstate) {
    tstate->test_result = TACTYK_TESTSTATE__INACTIVE;
    memset(tstate->report, 0, TACTYK_TEST__REPORT_BUFSIZE);
    memset(tstate->fname, 0, TACTYK_TEST__FNAME_BUFSIZE);
}

void tactyk_test__await_start(struct tactyk_test__Status *tstate) {
    while (tstate->test_result == TACTYK_TESTSTATE__PREPARING) {
        usleep(50000);
    }
}

//static jmp_buf tactyk_test_err_jbuf;

struct tactyk_dblock__DBlock *ERROR_TOKEN;
struct tactyk_dblock__DBlock *WARNING_TOKEN;
struct tactyk_test__Status *test_state;

void tactyk_test__warning_handler(char *msg, void *data) {
    memset(test_state->warning, 0, TACTYK_TEST__REPORT_BUFSIZE);
    strncpy(test_state->warning, msg, TACTYK_TEST__REPORT_BUFSIZE);
    int64_t space_remaining = TACTYK_TEST__REPORT_BUFSIZE - strlen(msg)-1;

    if ((space_remaining > 0) && (data != NULL)) {
        tactyk_dblock__export_cstring(&test_state->warning[strlen(msg)], space_remaining, data);
    }

//    struct tactyk_dblock__DBlock *test_spec = *active_test_spec;
//    if ((test_spec != NULL) && (test_spec->next != NULL)) {
//        struct tactyk_dblock__DBlock *token = test_spec = test_spec->next->token;
//        if (tactyk_dblock__equals(token, WARNING_TOKEN)) {
//            *active_test_spec = test_spec->next;
//        }
//    }
}

void tactyk_test__error_handler(char *msg, void *data) {

    memset(test_state->error, 0, TACTYK_TEST__REPORT_BUFSIZE);
    strncpy(test_state->error, msg, TACTYK_TEST__REPORT_BUFSIZE);
    int64_t space_remaining = TACTYK_TEST__REPORT_BUFSIZE - strlen(msg)-1;

    if ((space_remaining > 0) && (data != NULL)) {
        tactyk_dblock__export_cstring(&test_state->error[strlen(msg)], space_remaining, data);
    }

    //longjmp(tactyk_test_err_jbuf, 1);
    struct tactyk_dblock__DBlock *test_spec = *active_test_spec;
    if (test_spec == NULL) {
        assert(test_state->test_result == TACTYK_TESTSTATE__INITIALIZING);
        sprintf(test_state->report, "Unexpected error during test initialization.");
        tactyk_test__exit(TACTYK_TESTSTATE__TEST_ERROR);
    }
    else {
        test_spec = test_spec->next;
    }

    if (test_spec != NULL) {
        if (tactyk_dblock__equals(test_spec->token, ERROR_TOKEN)) {
            if (test_spec->token->next == NULL) {
                // if an expected error is received and the error specifier is the final command, the test passes.
                // TODO:  Error codes (because I dont want to resort to string comparisons to verify the correctness of the error)
                tactyk_test__exit(TACTYK_TESTSTATE__PASS);
            }
            else {
                // if an expected error is received but there are additional commands, it's a test error.
                sprintf(test_state->report, "test truncated by error (unreachable code in test)");
                tactyk_test__exit(TACTYK_TESTSTATE__TEST_ERROR);
            }
        }
        else {
            // unexpected errors are a test failure.
            tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
        }
    }
    else {
        // failure at the end of the test
        //  (only expected to occur if a test ends with an EXEC command it triggers an error)
        tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
    }
}

void tactyk_test__run(struct tactyk_test__Status *tstate) {
    test_state = tstate;
    tstate->test_result = TACTYK_TESTSTATE__INITIALIZING;

    FILE *f = fopen(tstate->fname, "r");
    fseek(f, 0, SEEK_END);
    int64_t len = ftell(f);
    char *src = calloc(len, sizeof(char));
    fseek(f,0, SEEK_SET);
    fread(src, len, 1, f);
    fclose(f);

    active_test_spec = calloc(1, sizeof(void*));
    *active_test_spec = NULL;

    tactyk_init();

    //tstate->test_result = TACTYK_TESTSTATE__PASS;
    //_exit(0);
    error = tactyk_test__error_handler;
    warn = tactyk_test__warning_handler;

    ERROR_TOKEN = tactyk_dblock__from_c_string("ERROR");
    WARNING_TOKEN = tactyk_dblock__from_c_string("WARNING");

    //if (setjmp(tactyk_err_jbuf)) {
    //    _exit(0);
    //}

    // should make configuration mutable - some tests will need to load alternative virtual ISA specifications that include special instructions which test
    //      the integrity of the sandbox.
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

    *active_test_spec = test_spec;
    tstate->test_result = TACTYK_TESTSTATE__RUNNING;

    uint64_t test_result = tactyk_test__exec_test_commands(test_spec);
    tactyk_test__exit(test_result);
}

uint64_t tactyk_test__exec_test_commands(struct tactyk_dblock__DBlock *test_spec) {
    while (test_spec != NULL) {
        *active_test_spec = test_spec;
        uint64_t tresult = tactyk_test__next(test_spec);
        switch(tresult) {
            case TACTYK_TESTSTATE__PASS: {
                break;
            }
            case TACTYK_TESTSTATE__FAIL: {
                tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
                break;
            }
            case TACTYK_TESTSTATE__EXIT: {
                tactyk_test__exit(TACTYK_TESTSTATE__PASS);
                break;
            }
            // impossible condition
            default: {
                tactyk_test__exit(TACTYK_TESTSTATE__TEST_ERROR);
                break;
            }
        }
        test_spec = test_spec->next;
    }
    return TACTYK_TESTSTATE__PASS;
}

void tactyk_test__report(char *msg) {
    memset(test_state->report, 0, TACTYK_TEST__REPORT_BUFSIZE);
    snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE-1, "%s", msg);
}

void tactyk_test__exit(uint64_t test_result) {
    printf("TEST RESULT:  %ju\n", test_result);
    if (test_state->report[0] != 0) {
        printf("REPORT MESSAGE:  '%s'\n", test_state->report);
    }
    if (test_state->warning[0] != 0) {
        printf("ERROR MESSAGE:  '%s'\n", test_state->error);
    }
    if (test_state->warning[0] != 0) {
        printf("WARNING MESSAGE:  '%s'\n", test_state->warning);
    }
    test_state->test_result = test_result;
    _exit(0);
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
            return TACTYK_TESTSTATE__FAIL;
        }
        else {
            tresult = tfunc(test_spec);
            return tresult;
        }
    }
    else {
        return TACTYK_TESTSTATE__EXIT;
    }
}

uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec) {
    if (tctx->plctx == NULL) {
        tctx->plctx = tactyk_pl__new(emitctx);
    }
    tactyk_pl__load_dblock(tctx->plctx, spec->child);
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__BUILD(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *name = spec->token->next;

    // load or instantiate a named context if a name is provided.
    if (name != NULL) {
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
        return TACTYK_TESTSTATE__FAIL;
    }

    struct tactyk_asmvm__Program *prg = tactyk_pl__build(tctx->plctx);
    tctx->program = prg;
    tctx->vmctx = tactyk_asmvm__new_context(vm);
    tactyk_asmvm__add_program(tctx->vmctx, tctx->program);

    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *func_name = spec->token->next;
    if ( (tctx->vmctx == NULL) || (tctx->program == NULL) ) {
        tactyk_test__report("Program not built");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (func_name == NULL) {
        tactyk_asmvm__invoke(tctx->vmctx, tctx->program, "MAIN");
    }
    else {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, func_name);
        tactyk_asmvm__invoke(tctx->vmctx, tctx->program, buf);
    }
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__RECV_CCALL(struct tactyk_dblock__DBlock *spec) {
    // should add room for a function args list to tactyk_test__Context
    // first a check to ensure that this is being called from a callback
    // Then args from the callback should be compared with params from the test specification and fail if anyare wrong.
    // Then the args should be invalidated so subsequent calls don't make
    tactyk_test__report("Test function 'RECV_CCALL' is not implemented");
    return TACTYK_TESTSTATE__TEST_ERROR;

    // callback prologue (for recursive tests)
    return tactyk_test__exec_test_commands(spec->child);
}
uint64_t tactyk_test__RECV_TCALL(struct tactyk_dblock__DBlock *spec) {
    // this isn't special. Just need to verify that this was called from the correct callback.
    tactyk_test__report("Test function 'RECV_TCALL' is not implemented");
    return TACTYK_TESTSTATE__TEST_ERROR;

    // callback prologue (for recursive tests)
    return tactyk_test__exec_test_commands(spec->child);
}
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec) {
    if ( tctx->vmctx == NULL) {
        tactyk_test__report("No asmvm context");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    struct tactyk_dblock__DBlock *td = spec->child;
    while (td != NULL) {
        struct tactyk_dblock__DBlock *item_name = td->token;
        assert(item_name != NULL);
        struct tactyk_dblock__DBlock *item_value = td->token->next;

        if (item_value == NULL) {
            tactyk_test__report("Unspecified item value");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        struct tactyk_test_entry *test = tactyk_dblock__get(base_tests, item_name);
        if (test == NULL) {
            char buf[256];
            sprintf(buf, "No handler for test item: ");
            tactyk_dblock__export_cstring(&buf[strlen(buf)], 256-strlen(buf), item_name );
            tactyk_test__report(buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        uint64_t tresult = test->test(test, item_value);
        if (tresult != TACTYK_TESTSTATE__PASS) {
            return tresult;
        }

        td = td->next;
    }
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec) {
    if ( tctx->vmctx == NULL) {
        tactyk_test__report("No asmvm context");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    struct tactyk_dblock__DBlock *td = spec->child;
    while (td != NULL) {
        struct tactyk_dblock__DBlock *item_name = td->token;
        assert(item_name != NULL);
        struct tactyk_dblock__DBlock *item_value = td->token->next;
        if (item_value == NULL) {
            tactyk_test__report("Unspecified item value");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        struct tactyk_test_entry *test = tactyk_dblock__get(base_tests, item_name);

        if (test == NULL) {
            char buf[256];
            sprintf(buf, "No handler for test item: ");
            tactyk_dblock__export_cstring(&buf[strlen(buf)], 256-strlen(buf), item_name );
            tactyk_test__report(buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        if (!test->adjust(test, item_value)) {
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        td = td->next;
    }
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec) {
    //struct tactyk_dblock__DBlock *mem_target = spec->token->next;
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec) {
    //struct tactyk_dblock__DBlock *mem_target = spec->token->next;
    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__CONTEXT(struct tactyk_dblock__DBlock *spec) {
    return TACTYK_TESTSTATE__PASS;
}

// If an "ERROR" command is executed, it means the system has not recognized an expected error condition, so this should always fail.
//  (If the system does generate an error, that gets captured by the testing framework error handler and recognized by looking ahead)
uint64_t tactyk_test__ERROR(struct tactyk_dblock__DBlock *spec) {
    return TACTYK_TESTSTATE__FAIL;
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
        return TACTYK_TESTSTATE__TEST_ERROR;
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
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    if (pass) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        return TACTYK_TESTSTATE__FAIL;
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
            return TACTYK_TESTSTATE__TEST_ERROR;
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
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    stval = fabs(stval - fval);
    if (stval <= tctx->precision) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        return TACTYK_TESTSTATE__FAIL;
    }
}

void tactyk_test__mk_data_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->adjust = tactyk_test__SET_DATA_REGISTER;
    entry->test = tactyk_test__TEST_DATA_REGISTER;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->adjust = tactyk_test__SET_XMM_REGISTER_FLOAT;
    entry->test = tactyk_test__TEST_XMM_REGISTER_FLOAT;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
