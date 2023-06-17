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

#include "ttest.h"

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


uint64_t test_count;
uint64_t tests_started;
uint64_t tests_completed;
struct tactyk_test__Status **tstate_list;
uint64_t num_;
char **testfilenames;

uint64_t max_active_jobs = 1;

bool use_immediate_scrambling = true;
bool use_executable_layout_randomization = true;
bool use_extra_permutations = true;
bool use_exopointers = true;
bool use_temp_register_autoreset = true;

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
        else if (strncmp(arg, "--perf", 6) == 0) {
            printf("Testing with experimental security features disabled.\n");
            use_immediate_scrambling = false;
            use_executable_layout_randomization = false;
            use_extra_permutations = false;
            use_exopointers = false;
            use_temp_register_autoreset = false;
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
                    waitpid(tstate->pid, &status, WNOHANG);
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
    tstate->use_executable_layout_randomization = use_executable_layout_randomization;
    tstate->use_exopointers = use_exopointers;
    tstate->use_extra_permutations = use_extra_permutations;
    tstate->use_immediate_scrambling = use_immediate_scrambling;
    tstate->use_temp_register_autoreset = use_temp_register_autoreset;
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




