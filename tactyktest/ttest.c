#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <time.h>

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

typedef bool (tactyk_test__VALUE_ADJUSTER) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
typedef uint64_t (tactyk_test__VALUE_TESTER) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);

// abstract test handler
struct tactyk_test_entry {
    char *name;
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

struct tactyk_test__Program {
    struct tactyk_dblock__DBlock *name;
    struct tactyk_pl__Context *plctx;
    struct tactyk_asmvm__Program *program;
};

struct tactyk_asmvm__Program *tprg;
struct tactyk_asmvm__Context *vmctx;

// a binary copy of vmctx to be used to scan for unexpected state transitions.
// This is to be updated by the tactyk_test any time tactyk_test checks or alters a property from the real vmctx.
// Unexpected state transitions are to be captured by scanning vmctx and shadow_vmctx for any deviations.
struct tactyk_asmvm__Context *shadow_vmctx;
struct tactyk_asmvm__memblock_lowlevel *shadow_memblocks;
double precision;


uint64_t tactyk_test__exec_test_commands(struct tactyk_dblock__DBlock *test_spec);
uint64_t tactyk_test__next(struct tactyk_dblock__DBlock *test_spec);
void tactyk_test__exit(uint64_t test_result);

uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RECV_CCALL(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RECV_TCALL(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__ALLOC(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__ERROR(struct tactyk_dblock__DBlock *spec);

bool tactyk_test__SET_CONTEXT_STATUS(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_CONTEXT_STATUS(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);

void tactyk_test__mk_var_test(char *name, tactyk_test__VALUE_ADJUSTER setter, tactyk_test__VALUE_TESTER *tester);
bool tactyk_test__SET_DATA_REGISTER (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
bool tactyk_test__SET_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
bool tactyk_test__SET_ADDR (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
bool tactyk_test__SET_MEM (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_DATA_REGISTER(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_ADDR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_MEM(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
void tactyk_test__mk_data_register_test(char *name, uint64_t ofs);
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs);

void tactyk_test__report(char *msg);

struct tactyk_dblock__DBlock *base_tests;

struct tactyk_emit__Context *emitctx;
struct tactyk_asmvm__VM *vm;
struct tactyk_dblock__DBlock *programs;
struct tactyk_dblock__DBlock *shadow_memblock_sets;
struct tactyk_dblock__DBlock *test_functions;

struct tactyk_dblock__DBlock *DEFAULT_NAME;

double DEFAULT_PRECISION = DBL_EPSILON * 256.0;

struct tactyk_dblock__DBlock **active_test_spec;

#define TACTYK_TEST__FNAME_BUFSIZE (1<<10)
#define TACTYK_TEST__REPORT_BUFSIZE (1<<10)
#define TACTYK_TEST__DUMP_BUFSIZE (1<<14)
struct tactyk_test__Status {
    uint64_t pid;
    uint64_t testid;
    time_t start_time;
    double max_age;
    uint64_t test_result;
    bool ran;
    char dump_context_expectation[TACTYK_TEST__DUMP_BUFSIZE];
    char dump_stack_expectation[TACTYK_TEST__DUMP_BUFSIZE];
    char dump_context_observed[TACTYK_TEST__DUMP_BUFSIZE];
    char dump_stack_observed[TACTYK_TEST__DUMP_BUFSIZE];
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
    printf("%s\n\n", TACTYK_TEST__DESCRIPTION);
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

    FILE *tout = fopen("ttest.log", "w");
    fprintf(tout, "TACTYK-TEST RESULTS\n");
    time_t test_starttime = time(NULL);
    fprintf(tout, "%ju job(s)\n", max_active_jobs);
    fprintf(tout, "%s", asctime(localtime(&test_starttime)));
    fprintf(tout, "-------------------------\n\n");

    // prepare one block of shared memory for each slot.
    //  This is to be used to get test results from child processes.
    tstate_list = calloc(max_active_jobs, sizeof(void*));
    for (uint64_t i = 0; i < max_active_jobs; i += 1) {
        struct tactyk_test__Status *ts = mmap(NULL, sizeof(struct tactyk_test__Status), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
        //sprintf(ts->report, "");
        ts->test_result = TACTYK_TESTSTATE__INACTIVE;
        tstate_list[i] = ts;
    }

    uint64_t passed = 0;
    uint64_t failed = 0;
    uint64_t errored = 0;

    while (tests_completed < test_count) {
        time_t now = time(NULL);
        //printf("tdiff = %f\n", sec);
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
                                fprintf(tout, "ERROR:  fork() failed.  Is %ju processes perhaps too much?\n", max_active_jobs);
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

                    fprintf(tout, "TEST ERROR\n");
                    fprintf(tout, "TEST: %s\n", tstate->fname);
                    if (tstate->report[0] != 0) {
                        fprintf(tout, "REPORT: %s\n", tstate->report);
                    }
                    if (tstate->error[0] != 0) {
                        fprintf(tout, "ERROR: %s\n", tstate->error);
                    }
                    if (tstate->warning[0] != 0) {
                        fprintf(tout, "WARNING: %s\n", tstate->warning);
                    }
                    fprintf(tout, "\n");

                    printf("\\");
                    fflush(stdout);

                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__EXIT: {
                    errored += 1;
                    tests_completed += 1;

                    fprintf(tout, "%s\n", tstate->fname);
                    fprintf(tout, "TEST-FRAMEWORK ERROR:  Invalid test result returned (TACTYK_TESTSTATE__EXIT)\n");
                    if (tstate->report[0] != 0) {
                        fprintf(tout, "REPORT: %s\n", tstate->report);
                    }
                    if (tstate->error[0] != 0) {
                        fprintf(tout, "ERROR: %s\n", tstate->error);
                    }
                    if (tstate->warning[0] != 0) {
                        fprintf(tout, "WARNING: %s\n", tstate->warning);
                    }
                    fprintf(tout, "\n");

                    printf("/");
                    fflush(stdout);

                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__PASS: {
                    passed += 1;
                    tests_completed += 1;

                    if ( (tstate->report[0] != 0) | (tstate->error[0] != 0) | (tstate->warning[0] != 0) ) {
                        fprintf(tout, "PASS WITH RESERVATIONS\n");
                        fprintf(tout, "TEST: %s\n", tstate->fname);
                        if (tstate->report[0] != 0) {
                            fprintf(tout, "REPORT: %s\n", tstate->report);
                        }
                        if (tstate->error[0] != 0) {
                            fprintf(tout, "ERROR: %s\n", tstate->error);
                        }
                        if (tstate->warning[0] != 0) {
                            fprintf(tout, "WARNING: %s\n", tstate->warning);
                        }
                    }
                    printf(".");
                    fflush(stdout);
                    //printf("PASS:  '%s'\n", tstate->fname);
                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__FAIL: {
                    failed += 1;
                    tests_completed += 1;

                    fprintf(tout, "FAILURE\n");
                    fprintf(tout, "TEST: %s\n", tstate->fname);

                    if (tstate->report[0] != 0) {
                        fprintf(tout, "REPORT: %s\n", tstate->report);
                    }
                    if (tstate->error[0] != 0) {
                        fprintf(tout, "ERROR: %s\n", tstate->error);
                    }
                    if (tstate->warning[0] != 0) {
                        fprintf(tout, "WARNING: %s\n", tstate->warning);
                    }
                    if (tstate->dump_context_expectation[0] != 0) {
                        fprintf(tout, "EXPECTED CONTEXT STATE:\n%s\n", tstate->dump_context_expectation);
                        fprintf(tout, "OBSERVED CONTEXT STATE:\n%s\n", tstate->dump_context_observed);

                        fprintf(tout, "EXPECTED STACK:\n%s\n", tstate->dump_stack_expectation);
                        fprintf(tout, "OBSERVED STACK:\n%s\n", tstate->dump_stack_observed);
                    }
                    fprintf(tout, "\n");

                    printf("X");
                    fflush(stdout);
                    //printf("FAIL:  '%s'\n", tstate->fname);
                    tactyk_test__reset_state(tstate);
                    break;
                }
                case TACTYK_TESTSTATE__INITIALIZING:
                case TACTYK_TESTSTATE__RUNNING: {
                    int status = 0;
                    pid_t pid = waitpid(tstate->pid, &status, WNOHANG);
                    if (WIFSIGNALED(status)) {
                        int sig = WTERMSIG(status);
                        failed += 1;
                        tests_completed += 1;
                        fprintf(tout, "FAILURE\n");
                        fprintf(tout, "TEST: %s\n", tstate->fname);
                        fprintf(tout, "TERMINATED BY SIGNAL:  %s\n", strsignal(sig));
                        fprintf(tout, "\n");

                        printf("X");
                        fflush(stdout);

                        tactyk_test__reset_state(tstate);
                    }
                    else {
                        double age = difftime(now, tstate->start_time);
                        if (age >= tstate->max_age) {
                            failed += 1;
                            tests_completed += 1;

                            fprintf(tout, "FAILURE\n");
                            fprintf(tout, "TEST: %s\n", tstate->fname);
                            fprintf(tout, "TEST PROCESSES HUNG (run time exceeded %f seconds)\n", tstate->max_age);
                            fprintf(tout, "\n");

                            printf("X");
                            fflush(stdout);

                            kill(tstate->pid, 1);
                            int status;
                            waitpid(-1, &status, 0);
                            tactyk_test__reset_state(tstate);
                        }
                    }
                    break;
                }
                default: {
                    errored += 1;
                    tests_completed += 1;

                    fprintf(tout, "TEST ERROR\n");
                    fprintf(tout, "TEST: %s\n", tstate->fname);
                    fprintf(tout, "REPORT:  Invalid test result returned (%ju)\n", tstate->test_result);
                    fprintf(tout, "\n");

                    printf("/");
                    fflush(stdout);

                    tactyk_test__reset_state(tstate);
                    break;
                }
            }
        }
        // 0.1 seconds
        usleep(100000);
    }


    time_t test_endtime = time(NULL);
    double test_age = difftime(test_endtime, test_starttime);

    fprintf(tout, "-------------------------\n");
    fprintf(tout, "TEST RESULTS: \n");
    fprintf(tout, "Total tests: %ju\n", test_count);
    fprintf(tout, "passed: %ju\n", passed);
    fprintf(tout, "failed: %ju\n", failed);
    fprintf(tout, "test errors: %ju\n", errored);
    fprintf(tout, "total run time:  %f seconds\n", test_age);

    printf("\n\nTEST RESULTS: \n");
    printf("Total tests: %ju\n", test_count);
    printf("passed: %ju\n", passed);
    printf("failed: %ju\n", failed);
    printf("test errors: %ju\n", errored);
    printf("total run time:  %f seconds\n", test_age);

    // release shared memory.
    for (uint64_t i = 0; i < max_active_jobs; i += 1) {
        struct tactyk_test__Status *ts = tstate_list[i];
        munmap(ts, sizeof(struct tactyk_test__Status));
    }
    free(tstate_list);
    if (passed == test_count) {
        return 0;
    }
    else if (errored == 0) {
        return 1;
    }
    else {
        return 2;
    }
}
void tactyk_test__prepare(struct tactyk_test__Status *tstate) {
    memset(tstate, 0, sizeof(struct tactyk_test__Status));
    strncpy(tstate->fname, testfilenames[tests_started], TACTYK_TEST__FNAME_BUFSIZE-1);
    tstate->start_time = time(NULL);
    tstate->max_age = 4.0;
    tstate->pid = 0;
    tstate->test_result = TACTYK_TESTSTATE__PREPARING;
    tstate->testid = tests_started;
    tstate->ran = false;
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
    int64_t space_remaining = TACTYK_TEST__REPORT_BUFSIZE - strlen(msg)-3;

    if ((space_remaining > 0) && (data != NULL)) {
        test_state->warning[strlen(msg)+0] = ':';
        test_state->warning[strlen(msg)+1] = ' ';
        tactyk_dblock__export_cstring(&test_state->warning[strlen(msg)+2], space_remaining, data);
    }
}

void tactyk_test__error_handler(char *msg, void *data) {
    memset(test_state->error, 0, TACTYK_TEST__REPORT_BUFSIZE);
    strncpy(test_state->error, msg, TACTYK_TEST__REPORT_BUFSIZE);
    int64_t space_remaining = TACTYK_TEST__REPORT_BUFSIZE - strlen(msg)-3;

    if ((space_remaining > 0) && (data != NULL)) {
        test_state->error[strlen(msg)+0] = ':';
        test_state->error[strlen(msg)+1] = ' ';
        tactyk_dblock__export_cstring(&test_state->error[strlen(msg)+2], space_remaining, data);
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
        // unexpected errors are a test failure.
        tactyk_test__report("Unexpected error");
        tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
    }
    else {
        // failure at the end of the test
        //  (only expected to occur if a test ends with an EXEC command it triggers an error)
        tactyk_test__report("Unexpected error at end of test");
        tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
    }
}

void tactyk_test__run(struct tactyk_test__Status *tstate) {
    test_state = tstate;
    tstate->test_result = TACTYK_TESTSTATE__INITIALIZING;

    FILE *f = fopen(tstate->fname, "r");
    fseek(f, 0, SEEK_END);
    int64_t len = ftell(f);
    char *src = calloc(len+1, sizeof(char));
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
    vmctx = tactyk_asmvm__new_context(vm);
    shadow_vmctx = calloc(1, sizeof(struct tactyk_asmvm__Context));

    tactyk_debug__configure_api(emitctx);
    tactyk_emit_svc__configure(emitctx);

    programs = tactyk_dblock__new_table(64);
    shadow_memblock_sets = tactyk_dblock__new_table(64);
    DEFAULT_NAME = tactyk_dblock__from_safe_c_string("DEFAULT");
    precision = DEFAULT_PRECISION;
    test_functions = tactyk_dblock__new_table(64);

    tactyk_dblock__put(test_functions, "PROGRAM", tactyk_test__PROGRAM);
    tactyk_dblock__put(test_functions, "EXEC", tactyk_test__EXEC);
    tactyk_dblock__put(test_functions, "RECV-CCALL", tactyk_test__RECV_CCALL);
    tactyk_dblock__put(test_functions, "RECV-TCALL", tactyk_test__RECV_TCALL);
    tactyk_dblock__put(test_functions, "TEST", tactyk_test__TEST);
    tactyk_dblock__put(test_functions, "STATE", tactyk_test__STATE);
    tactyk_dblock__put(test_functions, "ALLOC", tactyk_test__ALLOC);
    tactyk_dblock__put(test_functions, "DATA", tactyk_test__DATA);
    tactyk_dblock__put(test_functions, "REF", tactyk_test__REF);

    base_tests = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_test_entry));
    tactyk_test__mk_var_test("status", tactyk_test__SET_CONTEXT_STATUS, tactyk_test__TEST_CONTEXT_STATUS);

    struct tactyk_test_entry *addr1_test = tactyk_dblock__new_managedobject(base_tests, "addr1");
    addr1_test->name = "addr1";
    addr1_test->adjust = tactyk_test__SET_ADDR;
    addr1_test->test = tactyk_test__TEST_ADDR;
    addr1_test->element_offset = 1;
    addr1_test->array_offset = 0;
    struct tactyk_test_entry *addr2_test = tactyk_dblock__new_managedobject(base_tests, "addr2");
    addr2_test->name = "addr2";
    addr2_test->adjust = tactyk_test__SET_ADDR;
    addr2_test->test = tactyk_test__TEST_ADDR;
    addr2_test->element_offset = 2;
    addr2_test->array_offset = 0;
    struct tactyk_test_entry *addr3_test = tactyk_dblock__new_managedobject(base_tests, "addr3");
    addr3_test->name = "addr3";
    addr3_test->adjust = tactyk_test__SET_ADDR;
    addr3_test->test = tactyk_test__TEST_ADDR;
    addr3_test->element_offset = 3;
    addr3_test->array_offset = 0;
    struct tactyk_test_entry *addr4_test = tactyk_dblock__new_managedobject(base_tests, "addr4");
    addr4_test->name = "addr4";
    addr4_test->adjust = tactyk_test__SET_ADDR;
    addr4_test->test = tactyk_test__TEST_ADDR;
    addr4_test->element_offset = 4;
    addr4_test->array_offset = 0;

    struct tactyk_test_entry *mem_test = tactyk_dblock__new_managedobject(base_tests, "mem");
    mem_test->name = "mem";
    mem_test->adjust = tactyk_test__SET_MEM;
    mem_test->test = tactyk_test__TEST_MEM;
    mem_test->element_offset = 0;
    mem_test->array_offset = 0;

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
    struct tactyk_dblock__DBlock *test_src = tactyk_dblock__from_safe_c_string(src);
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
    tactyk_dblock__set_persistence_code(programs, 1000);
    tactyk_dblock__set_persistence_code(shadow_memblock_sets, 1000);

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
    if (test_state->ran == true) {
        FILE *stream = fmemopen(test_state->dump_context_observed, TACTYK_TEST__DUMP_BUFSIZE, "w");
        tactyk_debug__write_context(vmctx, stream);
        fflush(stream);
        fclose(stream);

        stream = fmemopen(test_state->dump_context_expectation, TACTYK_TEST__DUMP_BUFSIZE, "w");
        tactyk_debug__write_context(shadow_vmctx, stream);
        fflush(stream);
        fclose(stream);

        stream = fmemopen(test_state->dump_stack_observed, TACTYK_TEST__DUMP_BUFSIZE, "w");
        tactyk_debug__write_vmstack(vmctx, stream);
        fflush(stream);
        fclose(stream);

        stream = fmemopen(test_state->dump_stack_expectation, TACTYK_TEST__DUMP_BUFSIZE, "w");
        tactyk_debug__write_vmstack(shadow_vmctx, stream);
        fflush(stream);
        fclose(stream);
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
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, token);
            sprintf(test_state->report, "Undefined test function: %s\n", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
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
    struct tactyk_dblock__DBlock *name = spec->token->next;
    if (name == NULL) {
        name = DEFAULT_NAME;
    }
    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__load_dblock(plctx, spec->child);
    tprg = tactyk_pl__build(plctx);
    tactyk_asmvm__add_program(vmctx, tprg);

    vmctx->memblocks = (struct tactyk_asmvm__memblock_lowlevel*) tprg->memory_layout_ll->data;
    shadow_memblocks = (struct tactyk_asmvm__memblock_lowlevel*) calloc(TACTYK_ASMVM__MEMBLOCK_CAPACITY, sizeof(struct tactyk_asmvm__memblock_lowlevel));
    memcpy(shadow_memblocks, vmctx->memblocks, TACTYK_ASMVM__MEMBLOCK_CAPACITY * sizeof(struct tactyk_asmvm__memblock_lowlevel));
    for (uint64_t i = 0; i < TACTYK_ASMVM__MEMBLOCK_CAPACITY; i += 1) {
        struct tactyk_asmvm__memblock_lowlevel *mll = &vmctx->memblocks[i];
        struct tactyk_asmvm__memblock_lowlevel *shadow_mll = &shadow_memblocks[i];

        if (mll->base_address != NULL) {
            uint64_t sz = mll->array_bound + mll->element_bound -1;
            uint8_t *shadow_bytes = calloc(1, sz);
            shadow_mll->base_address = memcpy(shadow_bytes, mll->base_address, sz);
        }
    }
    tactyk_dblock__put(shadow_memblock_sets, name, shadow_memblocks);

    tactyk_dblock__put(programs, name, tprg);

    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *program_name;
    struct tactyk_dblock__DBlock *func_name = spec->token->next;
    if (tactyk_dblock__count_tokens(spec) == 3) {
        program_name = func_name;
        func_name = func_name->next;
        tprg = tactyk_dblock__get(programs, program_name);
        shadow_memblocks = tactyk_dblock__get(shadow_memblock_sets, program_name);
        if (tprg == NULL) {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, program_name);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "EXEC -- program '%s' is undefined", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    if ( (vmctx == NULL) || (tprg == NULL) ) {
        tactyk_test__report("Program not built");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (func_name == NULL) {
        tactyk_asmvm__prepare_invoke(shadow_vmctx, tprg, "MAIN");
        tactyk_asmvm__call(vmctx, tprg, "MAIN");
    }
    else {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, func_name);

        memcpy(shadow_vmctx, vmctx, sizeof(struct tactyk_asmvm__Context));
        if (!tactyk_asmvm__prepare_invoke(shadow_vmctx, tprg, buf)) {
            char bufpn[64];
            tactyk_dblock__export_cstring(bufpn, 64, program_name);
            char buffn[64];
            tactyk_dblock__export_cstring(buffn, 64, func_name);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "EXEC -- Can not call invalid function '%s.%s'", bufpn, buffn);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
        tactyk_asmvm__call(vmctx, tprg, buf);
    }
    // By default, expect the program to exit normally (by placing the 'STATUS_HALT' code in the shadow context)

    shadow_vmctx->STATUS = 4;
    test_state->ran = true;
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

        uint64_t tresult = test->test(test, td);
        if (tresult != TACTYK_TESTSTATE__PASS) {
            return tresult;
        }

        td = td->next;
    }

    // not part of the test.
    shadow_vmctx->instruction_index = vmctx->instruction_index;

    uint8_t *real_bytes = (uint8_t*) vmctx;
    uint8_t *shadow_bytes = (uint8_t*) shadow_vmctx;
    uint64_t *real_qwords = (uint64_t*) vmctx;
    uint64_t *shadow_qwords = (uint64_t*) shadow_vmctx;

    // scan the context structure, but leave out runtime registers and diagnostic data.
    //      diagnostic data is not part of testing (It's intended to aid with debugging)
    //      Runtime registers is whatever C leaves on the registers when calling into TACTYK.
    //          This would better be handled through a runtime environment correctness test to be performed within callbacks and/or after returning from tactyk.
    for (int64_t ofs = 0; ofs < offsetof(struct tactyk_asmvm__Context, runtime_registers); ofs += 1) {

        // if a deviation is found, the test fails.
        if (real_bytes[ofs] != shadow_bytes[ofs]) {
            int64_t qwpos = ofs / 8;

            // Check offset-ranges to identify which section of the context data structure the deviation was found in.

            // general context data.
            if (ofs < offsetof(struct tactyk_asmvm__Context, reg)) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "Context-state deviation at offset %jd (qword #%jd), expected=%jd observed=%jd",
                    ofs, qwpos, shadow_qwords[qwpos], real_qwords[qwpos]
                );
            }
            else {
                int64_t rel_ofs = ofs - offsetof(struct tactyk_asmvm__Context, reg);
                // main register file (standard x86 registers)
                if (rel_ofs < offsetof(struct tactyk_asmvm__register_bank, xa)) {
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "Register-file deviation at offset %jd (qword #%jd), expected=%jd observed=%jd",
                        rel_ofs, rel_ofs/8, shadow_qwords[qwpos], real_qwords[qwpos]
                    );
                }

                // xmm register file
                else {
                    int64_t rel_ofs = ofs - offsetof(struct tactyk_asmvm__Context, reg) - offsetof(struct tactyk_asmvm__register_bank, xa);
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "XMM Register-file deviation at offset %jd (qword #%jd), expected=%f observed=%f",
                        rel_ofs, rel_ofs/8, *((double*)&shadow_qwords[qwpos]), *((double*)&real_qwords[qwpos])
                    );
                }
            }
            return TACTYK_TESTSTATE__FAIL;
        }
    }

    for (uint64_t i = 0; i < TACTYK_ASMVM__MEMBLOCK_CAPACITY; i++) {
        struct tactyk_asmvm__memblock_lowlevel *mbll = &vmctx->memblocks[i];
        struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = &shadow_memblocks[i];
        //printf("p1=%p p2=%p\n", mbll, shadow_mbll);
        if (mbll->array_bound != shadow_mbll->array_bound) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "memblock %ju: array_bound deviation, expected=%u observed=%u",
                i, shadow_mbll->array_bound, mbll->array_bound
            );
        }
        if (mbll->element_bound != shadow_mbll->element_bound) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "memblock %ju: element_bound deviation, expected=%u observed=%u",
                i, shadow_mbll->element_bound, mbll->element_bound
            );
        }
        if (mbll->memblock_index != shadow_mbll->memblock_index) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "memblock %ju: memblock_index deviation, expected=%u observed=%u",
                i, shadow_mbll->memblock_index, mbll->memblock_index
            );
        }
        if (mbll->type != shadow_mbll->type) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "memblock %ju: type deviation, expected=%u observed=%u",
                i, shadow_mbll->type, mbll->type
            );
        }
        uint64_t len = mbll->array_bound + mbll->element_bound -1;
        if (mbll->base_address != NULL) {
            for (uint64_t j = 0; j < len; j += 1) {
                if (mbll->base_address[j] != shadow_mbll->base_address[j]) {
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "Memblock %ju data deviation at offset %ju, expected=%u observed=%u",
                        i, j, shadow_mbll->base_address[j], mbll->base_address[j]
                    );
                    return TACTYK_TESTSTATE__FAIL;
                }
            }
        }
    }

    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec) {
    if ( vmctx == NULL) {
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

        if (!test->adjust(test, td)) {
            tactyk_test__report("state adjustment rejected or not implemented");
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

// If an "ERROR" command is executed, it means the system has not recognized an expected error condition, so this should always fail.
//  (If the system does generate an error, that gets captured by the testing framework error handler and recognized by looking ahead)
uint64_t tactyk_test__ERROR(struct tactyk_dblock__DBlock *spec) {
    return TACTYK_TESTSTATE__FAIL;
}

bool tactyk_test__SET_CONTEXT_STATUS(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return false;
    }
    vmctx->STATUS= (uint64_t) ival;
    shadow_vmctx->STATUS = (uint64_t) ival;
    return true;
}
uint64_t tactyk_test__TEST_CONTEXT_STATUS(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_vmctx->STATUS = vmctx->STATUS;
    if (vmctx->STATUS == (uint64_t) ival) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "context status deviation, expected:%ju observed:%ju", vmctx->STATUS, (uint64_t)ival);
        return TACTYK_TESTSTATE__FAIL;
    }
}

uint64_t read_spec__binary_data(struct tactyk_dblock__DBlock **out, struct tactyk_dblock__DBlock *spec) {

    struct tactyk_dblock__DBlock *data_array = tactyk_dblock__new(0);
    *out = data_array;

    struct tactyk_dblock__DBlock *token = NULL;

    while (spec != NULL) {
        token = spec->token;
        if (tactyk_dblock__equals_c_string(token, "byte")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_byte(data_array, (uint8_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "word")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_word(data_array, (uint16_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "dword")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_dword(data_array, (uint32_t)val);
                token = token->next;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "qword")) {
            token = token->next;
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_qword(data_array, (uint64_t)val);
                token = token->next;
            }
        }
        else {
            while (token != NULL) {
                int64_t val = 0;
                if (!tactyk_dblock__try_parseint(&val, token)) {
                    goto parse_fail;
                }
                tactyk_dblock__append_qword(data_array, (uint64_t)val);
                token = token->next;
            }
        }
        spec = spec->next;
    }
    return TACTYK_TESTSTATE__PASS;

    parse_fail: {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, token);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not an integer", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
}

uint64_t tactyk_test__TEST_MEM(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *name = spec->token->next;

    // if the name is the name of a loaded program, then a "foreign" memblock is specified, so use the "foreign" program's memblock table.
    //  Also:  I should probably generate a test errror for programs which share the same name as a memblock.
    struct tactyk_asmvm__Program *src_program = tactyk_dblock__get(programs, name);
    struct tactyk_asmvm__memblock_lowlevel *shadow_mbs = NULL;
    if (src_program == NULL) {
        src_program = tprg;
        shadow_mbs = shadow_memblocks;
    }
    else {
        shadow_mbs = tactyk_dblock__get(shadow_memblock_sets, name);
        name = name->next;
    }

    struct tactyk_asmvm__memblock_highlevel *mbhl = tactyk_dblock__get(src_program->memory_layout_hl, name);
    struct tactyk_asmvm__memblock_lowlevel *mbll = tactyk_dblock__index(src_program->memory_layout_ll, mbhl->memblock_id);
    struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = &shadow_mbs[mbhl->memblock_id];

    uint64_t len = mbll->array_bound + mbll->element_bound -1;
    uint64_t slen = shadow_mbll->array_bound + shadow_mbll->element_bound -1;
    if (len != slen) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memblock unexpectedly replaced or resized -- expected length:%ju, observed length:%ju",
            len, slen
        );
        return TACTYK_TESTSTATE__FAIL;
    }

    struct tactyk_dblock__DBlock *iindex = name->next;
    if (iindex != NULL) {
        if (tactyk_dblock__equals_c_string(iindex, "*")) {
            // arbitrarilly accept the entire memblock
            for (uint64_t i = 0; i < len; i += 1) {
                shadow_mbll->base_address[i] = mbll->base_address[i];
            }
        }
        else {
            uint64_t idx = 0;
            if (!tactyk_dblock__try_parseuint(&idx, iindex)) {
                char buf[64];
                tactyk_dblock__export_cstring(buf, 64, iindex);
                snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not an integer", buf);
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            struct tactyk_dblock__DBlock *item_type = iindex->next;
            struct tactyk_dblock__DBlock *expected_value = item_type->next;
            if (expected_value == NULL) {
                expected_value = item_type;
                item_type = NULL;
            }
            int64_t ival = 0;
            if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
                char buf[64];
                tactyk_dblock__export_cstring(buf, 64, expected_value);
                snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not an integer", buf);
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            if ( (item_type == NULL) || tactyk_dblock__equals_c_string(item_type, "byte") ) {
                if (mbll->base_address[idx] != (uint8_t)ival) {
                    char buf[64];
                    tactyk_dblock__export_cstring(buf, 64, name);
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "deviation in memblock '%s' at offset %ju - expected value:%u, observed value:%u",
                        buf, idx, (uint8_t)ival, mbll->base_address[idx]
                    );
                    return TACTYK_TESTSTATE__FAIL;
                }
                shadow_mbll->base_address[idx] = (uint8_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "word") ) {
                if (*((uint16_t*) &mbll->base_address[idx]) != (uint16_t)ival) {
                    char buf[64];
                    tactyk_dblock__export_cstring(buf, 64, name);
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "deviation in memblock '%s' at offset %ju - expected value:%u, observed value:%u",
                        buf, idx, (uint16_t)ival, *((uint16_t*) &mbll->base_address[idx])
                    );
                    return TACTYK_TESTSTATE__FAIL;
                }
               *((uint16_t*) &shadow_mbll->base_address[idx]) = (uint16_t)ival;
            }

            else if ( tactyk_dblock__equals_c_string(item_type, "dword") ) {
                if (*((uint32_t*) &mbll->base_address[idx]) != (uint32_t)ival) {
                    char buf[64];
                    tactyk_dblock__export_cstring(buf, 64, name);
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "deviation in memblock '%s' at offset %ju - expected value:%u, observed value:%u",
                        buf, idx, (uint32_t)ival, *((uint32_t*) &mbll->base_address[idx])
                    );
                    return TACTYK_TESTSTATE__FAIL;
                }
               *((uint32_t*) &shadow_mbll->base_address[idx]) = (uint32_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "qword") ) {
                if (*((uint64_t*) &mbll->base_address[idx]) != (uint64_t)ival) {
                    char buf[64];
                    tactyk_dblock__export_cstring(buf, 64, name);
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "deviation in memblock '%s' at offset %ju - expected value:%ju, observed value:%ju",
                        buf, idx, (uint64_t)ival, *((uint64_t*) &mbll->base_address[idx])
                    );
                    return TACTYK_TESTSTATE__FAIL;
                }
               *((uint64_t*) &shadow_mbll->base_address[idx]) = (uint64_t)ival;
            }
            else {
                char buf[64];
                tactyk_dblock__export_cstring(buf, 64, item_type);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "Unrecognized data type: %s",
                    buf
                );

                return TACTYK_TESTSTATE__TEST_ERROR;
            }

        }
    }


    struct tactyk_dblock__DBlock *mspec = spec->child;
    if (mspec != NULL) {
        // actually test the entire memblock
    }

    return TACTYK_TESTSTATE__PASS;
}

bool tactyk_test__SET_MEM(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;
    return false;
}

uint64_t tactyk_test__ALLOC(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *name = spec->token->next;
    struct tactyk_dblock__DBlock *data;

    if (read_spec__binary_data(&data, spec->child) == TACTYK_TESTSTATE__TEST_ERROR) {
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    struct tactyk_asmvm__memblock_highlevel *mhl = tactyk_dblock__get(tprg->memory_layout_hl, name);
    uint64_t idx = mhl->memblock_id;
    struct tactyk_asmvm__memblock_lowlevel *mll = &vmctx->memblocks[idx];

    uint64_t len = data->length;
    uint8_t *bytes = tactyk_dblock__release(data);
    uint8_t *shadow_bytes = calloc(1, len);
    memcpy(shadow_bytes, bytes, len);

    mhl->data = bytes;
    mll->base_address = bytes;
    mll->array_bound = 1;
    mll->element_bound = len;

    struct tactyk_asmvm__memblock_lowlevel *shadow_mll = &shadow_memblocks[idx];
    shadow_mll->base_address = shadow_bytes;
    shadow_mll->array_bound = 1;
    shadow_mll->element_bound = len;
    //mll = mhl->memblock;

    return TACTYK_TESTSTATE__PASS;
}


bool tactyk_test__SET_ADDR (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;

    if (tactyk_dblock__count_peers(value) < 2) {
        tactyk_test__report("Not enough arguments to specify a memory binding.");
        return false;
    }

    struct tactyk_dblock__DBlock *name = value;
    struct tactyk_dblock__DBlock *reg_ofs = value->next;

    struct tactyk_asmvm__memblock_highlevel *mbhl = tactyk_dblock__get(vmctx->hl_program_ref->memory_layout_hl, name);
    struct tactyk_asmvm__memblock_lowlevel *mbll = &vmctx->memblocks[mbhl->memblock_id];
    struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = &shadow_memblocks[mbhl->memblock_id];

    struct tactyk_asmvm__memblock_lowlevel *target = NULL;
    struct tactyk_asmvm__memblock_lowlevel *shadow_target = NULL;

    int64_t ofs = 0;
    uint64_t abound = mbll->array_bound;
    uint64_t ebound = mbll->element_bound;
    if (!tactyk_dblock__try_parseint(&ofs, reg_ofs)) {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, reg_ofs);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "address-offset '%s' is not an integer.", buf);
        return false;
    }
    switch(entry->element_offset) {
        case 1: {
            vmctx->reg.rADDR1 = (uint64_t*) &mbll->base_address[ofs];
            shadow_vmctx->reg.rADDR1 = (uint64_t*) &shadow_mbll->base_address[ofs];
            break;
        }
        case 2: {
            vmctx->reg.rADDR2 = (uint64_t*) &mbll->base_address[ofs];
            shadow_vmctx->reg.rADDR2 = (uint64_t*) &shadow_mbll->base_address[ofs];
            break;
        }
        case 3: {
            vmctx->reg.rADDR3 = (uint64_t*) &mbll->base_address[ofs];
            shadow_vmctx->reg.rADDR3 = (uint64_t*) &shadow_mbll->base_address[ofs];
            break;
        }
        case 4: {
            vmctx->reg.rADDR4 = (uint64_t*) &mbll->base_address[ofs];
            shadow_vmctx->reg.rADDR4 = (uint64_t*) &shadow_mbll->base_address[ofs];
            break;
        }
    }

    target = &vmctx->memblocks[entry->element_offset];
    target->base_address = mbll->base_address;
    target->array_bound = abound;
    target->element_bound = ebound;

    shadow_target = &shadow_memblocks[entry->element_offset];
    shadow_target->base_address = shadow_mbll->base_address;
    shadow_target->array_bound = abound;
    shadow_target->element_bound = ebound;

    return true;
}

uint64_t tactyk_test__TEST_ADDR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    uint64_t num_tokens = tactyk_dblock__count_peers(expected_value);
    if (num_tokens < 2) {
        tactyk_test__report("Not enough arguments to specify a memory binding.");
        return false;
    }
    struct tactyk_dblock__DBlock *name = expected_value;
    struct tactyk_dblock__DBlock *reg_ofs = expected_value->next;

    // if the name is the name of a loaded program, then a "foreign" memblock is specified, so use the "foreign" program's memblock table.
    //  Also:  I should probably generate a test errror for programs which share the same name as a memblock.
    struct tactyk_asmvm__Program *src_program = tactyk_dblock__get(programs, name);
    struct tactyk_asmvm__memblock_lowlevel *shadow_mbs = NULL;
    if (src_program == NULL) {
        src_program = tprg;
        shadow_mbs = shadow_memblocks;
    }
    else {
        shadow_mbs = tactyk_dblock__get(shadow_memblock_sets, name);
        reg_ofs = reg_ofs->next;
        name = name->next;
    }

    struct tactyk_asmvm__memblock_highlevel *mbhl = NULL;
    struct tactyk_asmvm__memblock_lowlevel *mbll = NULL;
    struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = NULL;

    uint64_t mb_index = 0;
    if (tactyk_dblock__try_parseuint(&mb_index, name)) {
        mbhl = tactyk_dblock__index_allocated(src_program->memory_layout_hl->store, mb_index);
        mbll = tactyk_dblock__index_allocated(src_program->memory_layout_ll, mb_index);
        shadow_mbll = &shadow_mbs[mb_index];
    }
    else {
        mbhl = tactyk_dblock__get(src_program->memory_layout_hl, name);
        mbll = tactyk_dblock__index(src_program->memory_layout_ll, mbhl->memblock_id);
        shadow_mbll = &shadow_mbs[mbhl->memblock_id];
    }

    struct tactyk_asmvm__memblock_lowlevel *target = &vmctx->active_memblocks[entry->element_offset-1];
    struct tactyk_asmvm__memblock_lowlevel *shadow_target = &shadow_vmctx->active_memblocks[entry->element_offset-1];
    int64_t expected_ofs = 0;
    uint64_t expected_abound = 0;
    uint64_t expected_ebound = 0;

    if (!tactyk_dblock__try_parseint(&expected_ofs, reg_ofs)) {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, reg_ofs);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "address-offset '%s' is not an integer.", buf);
        return false;
    }
    if (num_tokens >= 4) {
        struct tactyk_dblock__DBlock *array_bound = reg_ofs->next;
        struct tactyk_dblock__DBlock *element_bound = reg_ofs->next->next;
        if (!tactyk_dblock__try_parseuint(&expected_abound, array_bound)) {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, array_bound);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "array-bound '%s' is not an integer.", buf);
            return false;
        }
        if (!tactyk_dblock__try_parseuint(&expected_ebound, element_bound)) {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, element_bound);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "element-bound '%s' is not an integer.", buf);
            return false;
        }
    }
    else {
        expected_abound = mbll->array_bound;
        expected_ebound = mbll->element_bound;
    }

    if ( target->memblock_index != mbll->memblock_index ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock id=%u observed memblock id=%u",
            mbll->memblock_index, target->memblock_index
        );
        return TACTYK_TESTSTATE__FAIL;
    }
    if ( target->base_address != mbll->base_address ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock ptr=%p observed memblock ptr=%p",
            mbll->base_address, target->base_address
        );
        return TACTYK_TESTSTATE__FAIL;
    }
    if ( target->array_bound != expected_abound ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock array-bound=%ju observed memblock array-bound=%u",
            expected_abound, target->array_bound
        );
        return TACTYK_TESTSTATE__FAIL;
    }
    if ( target->element_bound != expected_ebound ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock element-bound=%ju observed memblock element-bound=%u",
            expected_ebound, target->element_bound
        );
        return TACTYK_TESTSTATE__FAIL;
    }
    if ( target->type != mbll->type ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock type=%u observed memblock type=%u",
            mbll->type, target->type
        );
        return TACTYK_TESTSTATE__FAIL;
    }

    int64_t observed_ofs = 0;

    switch(entry->element_offset) {
        case 1: {
            observed_ofs = vmctx->reg.rADDR1 - (uint64_t*)target->base_address;
            shadow_vmctx->reg.rADDR1 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 2: {
            observed_ofs = vmctx->reg.rADDR2 - (uint64_t*)target->base_address;
            shadow_vmctx->reg.rADDR2 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 3: {
            observed_ofs = vmctx->reg.rADDR3 - (uint64_t*)target->base_address;
            shadow_vmctx->reg.rADDR3 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 4: {
            observed_ofs = vmctx->reg.rADDR4 - (uint64_t*)target->base_address;
            shadow_vmctx->reg.rADDR4 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
    }

    shadow_target->base_address = target->base_address;
    shadow_target->array_bound = expected_abound;
    shadow_target->element_bound = expected_ebound;
    shadow_target->memblock_index = target->memblock_index;
    shadow_target->type = target->type;

    if (observed_ofs != expected_ofs) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock offset=%jd observed memblock offset=%jd",
            expected_ofs, observed_ofs
        );
        return TACTYK_TESTSTATE__FAIL;
    }

    return TACTYK_TESTSTATE__PASS;
}

bool tactyk_test__SET_DATA_REGISTER (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return false;
    }
    switch(entry->element_offset) {
        case 0: {
            vmctx->reg.rA = (uint64_t) ival;
            shadow_vmctx->reg.rA = (uint64_t) ival;
            return true;
        }
        case 1: {
            vmctx->reg.rB = (uint64_t) ival;
            shadow_vmctx->reg.rB = (uint64_t) ival;
            return true;
        }
        case 2: {
            vmctx->reg.rC = (uint64_t) ival;
            shadow_vmctx->reg.rC = (uint64_t) ival;
            return true;
        }
        case 3: {
            vmctx->reg.rD = (uint64_t) ival;
            shadow_vmctx->reg.rD = (uint64_t) ival;
            return true;
        }
        case 4: {
            vmctx->reg.rE = (uint64_t) ival;
            shadow_vmctx->reg.rE = (uint64_t) ival;
            return true;
        }
        case 5: {
            vmctx->reg.rF = (uint64_t) ival;
            shadow_vmctx->reg.rF = (uint64_t) ival;
            return true;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return false;
        }
    }
}

uint64_t tactyk_test__TEST_DATA_REGISTER(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    uint64_t uival = (uint64_t)ival;
    uint64_t stval = 0;
    switch(valtest_spec->element_offset) {
        case 0: {
            stval = vmctx->reg.rA;
            shadow_vmctx->reg.rA = vmctx->reg.rA;
            break;
        }
        case 1: {
            stval = vmctx->reg.rB;
            shadow_vmctx->reg.rB = vmctx->reg.rB;
            break;
        }
        case 2: {
            stval = vmctx->reg.rC;
            shadow_vmctx->reg.rC = vmctx->reg.rC;
            break;
        }
        case 3: {
            stval = vmctx->reg.rD;
            shadow_vmctx->reg.rD = vmctx->reg.rD;
            break;
        }
        case 4: {
            stval = vmctx->reg.rE;
            shadow_vmctx->reg.rE = vmctx->reg.rE;
            break;
        }
        case 5: {
            stval = vmctx->reg.rF;
            shadow_vmctx->reg.rF = vmctx->reg.rF;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }

    if (stval == uival) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "deviation on register %s, expected:%ju observed:%ju", valtest_spec->name, uival, stval);
        return TACTYK_TESTSTATE__FAIL;
    }
}

bool tactyk_test__SET_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;

    double fval = 0;
    if (!tactyk_dblock__try_parsedouble(&fval, value)) {
        tactyk_test__report("Test parameter is not a floating point number");
        return false;
    }
    switch(entry->element_offset) {
        case 0: {
            vmctx->reg.xa.f64[0] = fval;
            shadow_vmctx->reg.xa.f64[0] = fval;
            break;
        }
        case 1: {
            vmctx->reg.xb.f64[0] = fval;
            shadow_vmctx->reg.xb.f64[0] = fval;
            break;
        }
        case 2: {
            vmctx->reg.xc.f64[0] = fval;
            shadow_vmctx->reg.xc.f64[0] = fval;
            break;
        }
        case 3: {
            vmctx->reg.xd.f64[0] = fval;
            shadow_vmctx->reg.xd.f64[0] = fval;
            break;
        }
        case 4: {
            vmctx->reg.xe.f64[0] = fval;
            shadow_vmctx->reg.xe.f64[0] = fval;
            break;
        }
        case 5: {
            vmctx->reg.xf.f64[0] = fval;
            shadow_vmctx->reg.xf.f64[0] = fval;
            break;
        }
        case 6: {
            vmctx->reg.xg.f64[0] = fval;
            shadow_vmctx->reg.xg.f64[0] = fval;
            break;
        }
        case 7: {
            vmctx->reg.xh.f64[0] = fval;
            shadow_vmctx->reg.xh.f64[0] = fval;
            break;
        }
        case 8: {
            vmctx->reg.xi.f64[0] = fval;
            shadow_vmctx->reg.xi.f64[0] = fval;
            break;
        }
        case 9: {
            vmctx->reg.xj.f64[0] = fval;
            shadow_vmctx->reg.xj.f64[0] = fval;
            break;
        }
        case 10: {
            vmctx->reg.xk.f64[0] = fval;
            shadow_vmctx->reg.xk.f64[0] = fval;
            break;
        }
        case 11: {
            vmctx->reg.xl.f64[0] = fval;
            shadow_vmctx->reg.xl.f64[0] = fval;
            break;
        }
        case 12: {
            vmctx->reg.xm.f64[0] = fval;
            shadow_vmctx->reg.xm.f64[0] = fval;
            break;
        }
        case 13: {
            vmctx->reg.xn.f64[0] = fval;
            shadow_vmctx->reg.xn.f64[0] = fval;
            break;
        }
        case 14: {
            vmctx->reg.xTEMPA.f64[0] = fval;
            shadow_vmctx->reg.xTEMPA.f64[0] = fval;
            break;
        }
        case 15: {
            vmctx->reg.xTEMPB.f64[0] = fval;
            shadow_vmctx->reg.xTEMPB.f64[0] = fval;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__TEST_XMM_REGISTER_FLOAT (struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    double fval = 0;
    if (!tactyk_dblock__try_parsedouble(&fval, expected_value)) {
        tactyk_test__report("Test parameter is not a floating point number");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    double stval = 0;
    switch(valtest_spec->element_offset) {
        case 0: {
            stval = vmctx->reg.xa.f64[0];
            shadow_vmctx->reg.xa.f64[0] = vmctx->reg.xa.f64[0];
            break;
        }
        case 1: {
            stval = vmctx->reg.xb.f64[0];
            shadow_vmctx->reg.xb.f64[0] = vmctx->reg.xb.f64[0];
            break;
        }
        case 2: {
            stval = vmctx->reg.xc.f64[0];
            shadow_vmctx->reg.xc.f64[0] = vmctx->reg.xc.f64[0];
            break;
        }
        case 3: {
            stval = vmctx->reg.xd.f64[0];
            shadow_vmctx->reg.xd.f64[0] = vmctx->reg.xd.f64[0];
            break;
        }
        case 4: {
            stval = vmctx->reg.xe.f64[0];
            shadow_vmctx->reg.xe.f64[0] = vmctx->reg.xe.f64[0];
            break;
        }
        case 5: {
            stval = vmctx->reg.xf.f64[0];
            shadow_vmctx->reg.xf.f64[0] = vmctx->reg.xf.f64[0];
            break;
        }
        case 6: {
            stval = vmctx->reg.xg.f64[0];
            shadow_vmctx->reg.xg.f64[0] = vmctx->reg.xg.f64[0];
            break;
        }
        case 7: {
            stval = vmctx->reg.xh.f64[0];
            shadow_vmctx->reg.xh.f64[0] = vmctx->reg.xh.f64[0];
            break;
        }
        case 8: {
            stval = vmctx->reg.xi.f64[0];
            shadow_vmctx->reg.xi.f64[0] = vmctx->reg.xi.f64[0];
            break;
        }
        case 9: {
            stval = vmctx->reg.xj.f64[0];
            shadow_vmctx->reg.xj.f64[0] = vmctx->reg.xj.f64[0];
            break;
        }
        case 10: {
            stval = vmctx->reg.xk.f64[0];
            shadow_vmctx->reg.xk.f64[0] = vmctx->reg.xk.f64[0];
            break;
        }
        case 11: {
            stval = vmctx->reg.xl.f64[0];
            shadow_vmctx->reg.xl.f64[0] = vmctx->reg.xl.f64[0];
            break;
        }
        case 12: {
            stval = vmctx->reg.xm.f64[0];
            shadow_vmctx->reg.xm.f64[0] = vmctx->reg.xm.f64[0];
            break;
        }
        case 13: {
            stval = vmctx->reg.xn.f64[0];
            shadow_vmctx->reg.xn.f64[0] = vmctx->reg.xn.f64[0];
            break;
        }
        case 14: {
            stval = vmctx->reg.xTEMPA.f64[0];
            shadow_vmctx->reg.xTEMPA.f64[0] = vmctx->reg.xTEMPA.f64[0];
            break;
        }
        case 15: {
            stval = vmctx->reg.xTEMPB.f64[0];
            shadow_vmctx->reg.xTEMPB.f64[0] = vmctx->reg.xTEMPB.f64[0];
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    stval = fabs(stval - fval);
    if (stval <= precision) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "deviation on register %s, expected:%f observed:%f", valtest_spec->name, fval, stval);
        return TACTYK_TESTSTATE__FAIL;
    }
}

void tactyk_test__mk_var_test(char *name, tactyk_test__VALUE_ADJUSTER setter, tactyk_test__VALUE_TESTER *tester) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->adjust = setter;
    entry->test = tester;
    entry->element_offset = 0;
    entry->array_offset = 0;
}

void tactyk_test__mk_data_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->adjust = tactyk_test__SET_DATA_REGISTER;
    entry->test = tactyk_test__TEST_DATA_REGISTER;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs) {
    struct tactyk_test_entry *entry = tactyk_dblock__new_managedobject(base_tests, name);
    entry->name = name;
    entry->adjust = tactyk_test__SET_XMM_REGISTER_FLOAT;
    entry->test = tactyk_test__TEST_XMM_REGISTER_FLOAT;
    entry->element_offset = ofs;
    entry->array_offset = 0;
}
