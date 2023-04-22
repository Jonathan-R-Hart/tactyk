#ifndef TTEST_H
#define TTEST_H

#include <stdint.h>
#include <stdbool.h>

#include "tactyk_asmvm.h"

#define TACTYK_TEST__FNAME_BUFSIZE (1<<10)
#define TACTYK_TEST__REPORT_BUFSIZE (1<<10)
#define TACTYK_TEST__DUMP_BUFSIZE (1<<14)

#define TACTYK_TESTSTATE__EXIT 0
#define TACTYK_TESTSTATE__PASS 1
#define TACTYK_TESTSTATE__FAIL 2
#define TACTYK_TESTSTATE__TEST_ERROR 3
#define TACTYK_TESTSTATE__RUNNING 4
#define TACTYK_TESTSTATE__PREPARING 5
#define TACTYK_TESTSTATE__INITIALIZING 6
#define TACTYK_TESTSTATE__INACTIVE 7
#define TACTYK_TESTSTATE__CONTINUE 8


struct tactyk_test_entry;

typedef uint64_t (*tactyk_emit__test_func)(struct tactyk_dblock__DBlock *data);

typedef uint64_t (tactyk_test__VALUE_TESTER) (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);


extern struct tactyk_asmvm__Program *tprg;
extern struct tactyk_asmvm__Context *vmctx;

// a binary copy of vmctx to be used to scan for unexpected state transitions.
// This is to be updated by the tactyk_test any time tactyk_test checks or alters a property from the real vmctx.
// Unexpected state transitions are to be captured by scanning vmctx and shadow_vmctx for any deviations.
extern struct tactyk_asmvm__Context *shadow_vmctx;
extern struct tactyk_asmvm__memblock_lowlevel *shadow_memblocks;
extern struct tactyk_asmvm__MicrocontextStash *shadow_mctxstack;
extern struct tactyk_asmvm__Stack *shadow_ctx_stack;
extern uint32_t *shadow_lwcall_stack;
extern double precision;
extern uint64_t callback_id;
extern uint64_t ccall_args[6];
extern int64_t ccall_retval;

extern struct tactyk_dblock__DBlock *base_tests;

extern struct tactyk_emit__Context *emitctx;
extern struct tactyk_asmvm__VM *vm;
extern struct tactyk_dblock__DBlock *programs;
extern struct tactyk_dblock__DBlock *shadow_memblock_sets;
extern struct tactyk_dblock__DBlock *test_functions;

extern struct tactyk_dblock__DBlock *DEFAULT_NAME;

extern double DEFAULT_PRECISION;

extern struct tactyk_dblock__DBlock **active_test_spec;

// test command immediately following a "CONTINUE" command
// This is used by callbacks [from tactyk] to update test-command control flow so that multiple callbacks can be handled.
extern struct tactyk_dblock__DBlock *test_continuation;

//static jmp_buf tactyk_test_err_jbuf;

extern struct tactyk_dblock__DBlock *ERROR_TOKEN;
extern struct tactyk_dblock__DBlock *WARNING_TOKEN;
extern struct tactyk_test__Status *test_state;



// abstract test handler
struct tactyk_test_entry {
    char *name;
    tactyk_test__VALUE_TESTER *test;            // Test function to use for TEST operation
    uint64_t offset;    // object member to access
};

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
};

uint64_t tactyk_test__exec_test_commands(struct tactyk_dblock__DBlock *test_spec);
uint64_t tactyk_test__next(struct tactyk_dblock__DBlock *test_spec);
void tactyk_test__exit(uint64_t test_result);

uint64_t tactyk_test__PROGRAM(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__EXEC(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RECV_CCALL_1(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f);
uint64_t tactyk_test__RECV_CCALL_2(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f);
uint64_t tactyk_test__RECV_CCALL_3(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f);
uint64_t tactyk_test__RECV_CCALL_4(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f);
void tactyk_test__RECV_TCALL_5(struct tactyk_asmvm__Context *ctx);
void tactyk_test__RECV_TCALL_6(struct tactyk_asmvm__Context *ctx);
void tactyk_test__RECV_TCALL_7(struct tactyk_asmvm__Context *ctx);
void tactyk_test__RECV_TCALL_8(struct tactyk_asmvm__Context *ctx);
uint64_t tactyk_test__TEST(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__STATE(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__ALLOC(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__DATA(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__REF(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__ERROR(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__CONTINUE(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RETURN(struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__RESUME(struct tactyk_dblock__DBlock *spec);

bool tactyk_test__SET_CONTEXT_STATUS(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_CONTEXT_STATUS(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);

uint64_t tactyk_test__TEST_STACKLOCK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_STACKPOSITION(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);

uint64_t tactyk_test__TEST_REGISTER(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_XMM_REGISTER_FLOAT (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_ADDR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_MEM(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_CALLBACK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_CCALL_ARGUMENT(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_LWCALL_STACK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_STASH(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);
uint64_t tactyk_test__TEST_STACK__STACK_ENTRY(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec);



void tactyk_test__mk_var_test(char *name, tactyk_test__VALUE_TESTER *tester);
void tactyk_test__mk_register_test(char *name, uint64_t ofs);
void tactyk_test__mk_xmm_register_test(char *name, uint64_t ofs);
void tactyk_test__mk_ccallarg_test(char *name, uint64_t ofs);

void tactyk_test__report(char *msg);


void tactyk_test__prepare(struct tactyk_test__Status *tstate);
void tactyk_test__run(struct tactyk_test__Status *tstate);
void tactyk_test__reset_state(struct tactyk_test__Status *tstate);
void tactyk_test__await_start(struct tactyk_test__Status *tstate);


bool tactyk_test__approximately_eq(double a, double b);

uint64_t read_spec__binary_data(struct tactyk_dblock__DBlock **out, struct tactyk_dblock__DBlock *spec);

#endif // TTEST_H
