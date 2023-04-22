#include <math.h>
#include <float.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>

#include "ttest.h"
#include "tactyk.h"
#include "tactyk_emit.h"
#include "tactyk_visa.h"
#include "tactyk_pl.h"
#include "tactyk_debug.h"
#include "tactyk_emit_svc.h"


struct tactyk_asmvm__Program *tprg;
struct tactyk_asmvm__Context *vmctx;

// a binary copy of vmctx to be used to scan for unexpected state transitions.
// This is to be updated by the tactyk_test any time tactyk_test checks or alters a property from the real vmctx.
// Unexpected state transitions are to be captured by scanning vmctx and shadow_vmctx for any deviations.
struct tactyk_asmvm__Context *shadow_vmctx;
struct tactyk_asmvm__memblock_lowlevel *shadow_memblocks;
struct tactyk_asmvm__MicrocontextStash *shadow_mctxstack;
struct tactyk_asmvm__Stack *shadow_ctx_stack;
uint32_t *shadow_lwcall_stack;
double precision;
uint64_t callback_id;
uint64_t ccall_args[6];
int64_t ccall_retval;

struct tactyk_dblock__DBlock *base_tests;

struct tactyk_emit__Context *emitctx;
struct tactyk_asmvm__VM *vm;
struct tactyk_dblock__DBlock *programs;
struct tactyk_dblock__DBlock *shadow_memblock_sets;
struct tactyk_dblock__DBlock *test_functions;

struct tactyk_dblock__DBlock *DEFAULT_NAME;

double DEFAULT_PRECISION = DBL_EPSILON * 256.0;

struct tactyk_dblock__DBlock **active_test_spec;

// test command immediately following a "CONTINUE" command
// This is used by callbacks [from tactyk] to update test-command control flow so that multiple callbacks can be handled.
struct tactyk_dblock__DBlock *test_continuation;

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
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, test_spec->token);
        if (strncmp(buf, "ERROR", 6) == 0) {
            tactyk_test__exit(TACTYK_TESTSTATE__PASS);
        }
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
    test_continuation = NULL;

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
    shadow_mctxstack = (struct tactyk_asmvm__MicrocontextStash*) calloc(TACTYK_ASMVM__MCTX_STACK_SIZE, sizeof(struct tactyk_asmvm__MicrocontextStash));
    shadow_lwcall_stack = (uint32_t*) calloc(TACTYK_ASMVM__LWCALL_STACK_SIZE, sizeof(uint32_t));
    shadow_ctx_stack = calloc(1, sizeof(struct tactyk_asmvm__Stack));
    shadow_ctx_stack->stack_lock = 0;
    shadow_ctx_stack->stack_position = -1;
    tactyk_debug__configure_api(emitctx);
    tactyk_emit_svc__configure(emitctx);
    tactyk_emit__add_c_apifunc(emitctx, "cfunc-1", tactyk_test__RECV_CCALL_1);
    tactyk_emit__add_c_apifunc(emitctx, "cfunc-2", tactyk_test__RECV_CCALL_2);
    tactyk_emit__add_c_apifunc(emitctx, "cfunc-3", tactyk_test__RECV_CCALL_3);
    tactyk_emit__add_c_apifunc(emitctx, "cfunc-4", tactyk_test__RECV_CCALL_4);
    tactyk_emit__add_tactyk_apifunc(emitctx, "tfunc-5", tactyk_test__RECV_TCALL_5);
    tactyk_emit__add_tactyk_apifunc(emitctx, "tfunc-6", tactyk_test__RECV_TCALL_6);
    tactyk_emit__add_tactyk_apifunc(emitctx, "tfunc-7", tactyk_test__RECV_TCALL_7);
    tactyk_emit__add_tactyk_apifunc(emitctx, "tfunc-8", tactyk_test__RECV_TCALL_8);

    programs = tactyk_dblock__new_table(64);
    shadow_memblock_sets = tactyk_dblock__new_table(64);
    DEFAULT_NAME = tactyk_dblock__from_safe_c_string("DEFAULT");
    precision = DEFAULT_PRECISION;
    test_functions = tactyk_dblock__new_table(64);

    tactyk_dblock__put(test_functions, "PROGRAM", tactyk_test__PROGRAM);
    tactyk_dblock__put(test_functions, "EXEC", tactyk_test__EXEC);
    tactyk_dblock__put(test_functions, "TEST", tactyk_test__TEST);
    tactyk_dblock__put(test_functions, "ALLOC", tactyk_test__ALLOC);
    tactyk_dblock__put(test_functions, "DATA", tactyk_test__DATA);
    tactyk_dblock__put(test_functions, "REF", tactyk_test__REF);
    tactyk_dblock__put(test_functions, "CONTINUE", tactyk_test__CONTINUE);
    tactyk_dblock__put(test_functions, "RETURN", tactyk_test__RETURN);
    tactyk_dblock__put(test_functions, "RESUME", tactyk_test__RESUME);

    base_tests = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_test_entry));
    tactyk_test__mk_var_test("status", tactyk_test__TEST_CONTEXT_STATUS);
    tactyk_test__mk_var_test("stack-lock", tactyk_test__TEST_STACKLOCK);
    tactyk_test__mk_var_test("stack-pos", tactyk_test__TEST_STACKPOSITION);

    tactyk_test__mk_var_test("stack-entry", tactyk_test__TEST_STACK__STACK_ENTRY);

    struct tactyk_test_entry *addr1_test = tactyk_dblock__new_managedobject(base_tests, "addr1");
    addr1_test->name = "addr1";
    addr1_test->test = tactyk_test__TEST_ADDR;
    addr1_test->offset = 1;
    struct tactyk_test_entry *addr2_test = tactyk_dblock__new_managedobject(base_tests, "addr2");
    addr2_test->name = "addr2";
    addr2_test->test = tactyk_test__TEST_ADDR;
    addr2_test->offset = 2;
    struct tactyk_test_entry *addr3_test = tactyk_dblock__new_managedobject(base_tests, "addr3");
    addr3_test->name = "addr3";
    addr3_test->test = tactyk_test__TEST_ADDR;
    addr3_test->offset = 3;
    struct tactyk_test_entry *addr4_test = tactyk_dblock__new_managedobject(base_tests, "addr4");
    addr4_test->name = "addr4";
    addr4_test->test = tactyk_test__TEST_ADDR;
    addr4_test->offset = 4;

    struct tactyk_test_entry *mem_test = tactyk_dblock__new_managedobject(base_tests, "mem");
    mem_test->name = "mem";
    mem_test->test = tactyk_test__TEST_MEM;
    mem_test->offset = 0;

    struct tactyk_test_entry *cb_test = tactyk_dblock__new_managedobject(base_tests, "callback");
    cb_test->name = "callback";
    cb_test->test = tactyk_test__TEST_CALLBACK;
    cb_test->offset = 0;

    struct tactyk_test_entry *stash_test = tactyk_dblock__new_managedobject(base_tests, "stash");
    stash_test->name = "stash";
    stash_test->test = tactyk_test__TEST_STASH;
    stash_test->offset = 0;

    struct tactyk_test_entry *lwcs_test = tactyk_dblock__new_managedobject(base_tests, "lwcstack");
    lwcs_test->name = "lwcstack";
    lwcs_test->test = tactyk_test__TEST_LWCALL_STACK;
    lwcs_test->offset = 0;

    struct tactyk_test_entry *program_test = tactyk_dblock__new_managedobject(base_tests, "program");
    program_test->name = "program";
    program_test->test = tactyk_test__TEST_CONTEXT_PROGRAM;
    program_test->offset = 0;

    tactyk_test__mk_register_test("rLWCSI", 1);
    tactyk_test__mk_register_test("rMCSI", 2);
    tactyk_test__mk_register_test("rA", 10);
    tactyk_test__mk_register_test("rB", 11);
    tactyk_test__mk_register_test("rC", 12);
    tactyk_test__mk_register_test("rD", 13);
    tactyk_test__mk_register_test("rE", 14);
    tactyk_test__mk_register_test("rF", 15);
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

    tactyk_test__mk_ccallarg_test("arg0", 0);
    tactyk_test__mk_ccallarg_test("arg1", 1);
    tactyk_test__mk_ccallarg_test("arg2", 2);
    tactyk_test__mk_ccallarg_test("arg3", 3);
    tactyk_test__mk_ccallarg_test("arg4", 4);
    tactyk_test__mk_ccallarg_test("arg5", 5);

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

    ccall_retval = 0;
    callback_id = 0;
    memset(ccall_args, 0, sizeof(ccall_args));

    uint64_t test_result = tactyk_test__exec_test_commands(test_spec);
    tactyk_test__exit(test_result);
}

uint64_t tactyk_test__exec_test_commands(struct tactyk_dblock__DBlock *test_spec) {
    while (test_spec != NULL) {
        if (test_continuation != NULL) {
            tactyk_test__report("Expected callback not received");
            tactyk_test__exit(TACTYK_TESTSTATE__FAIL);
        }
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
            case TACTYK_TESTSTATE__CONTINUE: {
                test_continuation = test_spec->next;
                return TACTYK_TESTSTATE__CONTINUE;
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
        tactyk_debug__write_vmstack(vmctx->stack, stream);
        fflush(stream);
        fclose(stream);

        stream = fmemopen(test_state->dump_stack_expectation, TACTYK_TEST__DUMP_BUFSIZE, "w");
        tactyk_debug__write_vmstack(shadow_ctx_stack, stream);
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
            *active_test_spec = test_spec;
            tresult = tfunc(test_spec);
            *active_test_spec = test_spec;
            return tresult;
        }
    }
    else {
        return TACTYK_TESTSTATE__EXIT;
    }
}
