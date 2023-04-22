#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "ttest.h"
#include "tactyk_pl.h"
#include "tactyk_dblock.h"
#include "tactyk_asmvm.h"

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
    test_continuation = spec->child;
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


uint64_t tactyk_test__RESUME(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *program_name = spec->token->next;
    if (program_name != NULL) {
        tprg = tactyk_dblock__get(programs, program_name);
        if (tprg == NULL) {
            char bufpn[64];
            tactyk_dblock__export_cstring(bufpn, 64, program_name);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "EXEC -- Can not resume undefined program '%s'", bufpn);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }

    tactyk_asmvm__resume(vmctx);
    shadow_vmctx->STATUS = 4;
    return TACTYK_TESTSTATE__PASS;
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

    // if callback id not cleared, then it means a ccall or tcall is not accounted for
    //   this may occur either in a TEST command nested inside of an EXEC and invoked through a callback, or it may occur after the return form the callback when the next TEST command is handled.
    if (callback_id != 0) {
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "Unexpected callback, id:%ju", callback_id);
        return TACTYK_TESTSTATE__FAIL;
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
        if (mbll->offset != shadow_mbll->offset) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "memblock %ju: offset-field deviation, expected=%u observed=%u",
                i, shadow_mbll->offset, mbll->offset
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

    for (uint64_t i = 0; i < TACTYK_ASMVM__LWCALL_STACK_SIZE; i++) {
        if (vmctx->lwcall_stack[i] != shadow_lwcall_stack[i]) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "lwcall stack deviation at offset %ju: expected=%u observed=%u",
                i, shadow_lwcall_stack[i], vmctx->lwcall_stack[i]
            );
            return TACTYK_TESTSTATE__FAIL;
        }
    }

    for (uint64_t i = 0; i < TACTYK_ASMVM__MCTX_STACK_SIZE; i++) {
        struct tactyk_asmvm__MicrocontextStash *stash = &vmctx->microcontext_stack[i];
        struct tactyk_asmvm__MicrocontextStash *shadow_stash = &shadow_mctxstack[i];

        for (uint64_t j = 0; j < 4; j++) {
            struct tactyk_asmvm__memblock_lowlevel *mbll = &stash->memblocks[i];
            struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = &stash->memblocks[i];
            if (mbll->base_address != shadow_mbll->base_address) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "stash[%ju] deviation - memblock[%ju].base_address: expected=%p, observed=%p",
                    i, j, shadow_mbll->base_address, mbll->base_address
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->array_bound != shadow_mbll->array_bound) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "stash[%ju] deviation - memblock[%ju].array_bound: expected=%u, observed=%u",
                    i, j, shadow_mbll->array_bound, mbll->array_bound
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->element_bound != shadow_mbll->element_bound) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "stash[%ju] deviation - memblock[%ju].element_bound: expected=%u, observed=%u",
                    i, j, shadow_mbll->element_bound, mbll->element_bound
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->memblock_index != shadow_mbll->memblock_index) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "stash[%ju] deviation - memblock[%ju].memblock_index: expected=%u, observed=%u",
                    i, j, shadow_mbll->memblock_index, mbll->memblock_index
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->offset != shadow_mbll->offset) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "stash[%ju] deviation - memblock[%ju].offset: expected=%u, observed=%u",
                    i, j, shadow_mbll->offset, mbll->offset
                );
                return TACTYK_TESTSTATE__FAIL;
            }
        }

        #define STASH_CHK(PROP) \
        if (shadow_stash->PROP != stash->PROP) { \
            snprintf( \
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE, \
                "stash[%ju] deviation - " #PROP ": expected=%jd, observed=%jd", \
                i, shadow_stash->PROP, stash->PROP \
            ); \
            return TACTYK_TESTSTATE__FAIL; \
        }
        STASH_CHK(a.i64[0]);
        STASH_CHK(a.i64[1]);
        STASH_CHK(b.i64[0]);
        STASH_CHK(b.i64[1]);
        STASH_CHK(c.i64[0]);
        STASH_CHK(c.i64[1]);
        STASH_CHK(d.i64[0]);
        STASH_CHK(d.i64[1]);
        STASH_CHK(e.i64[0]);
        STASH_CHK(e.i64[1]);

        STASH_CHK(f.i64[0]);
        STASH_CHK(f.i64[1]);
        STASH_CHK(g.i64[0]);
        STASH_CHK(g.i64[1]);
        STASH_CHK(h.i64[0]);
        STASH_CHK(h.i64[1]);
        STASH_CHK(i.i64[0]);
        STASH_CHK(i.i64[1]);
        STASH_CHK(j.i64[0]);
        STASH_CHK(j.i64[1]);

        STASH_CHK(k.i64[0]);
        STASH_CHK(k.i64[1]);
        STASH_CHK(l.i64[0]);
        STASH_CHK(l.i64[1]);
        STASH_CHK(m.i64[0]);
        STASH_CHK(m.i64[1]);
        STASH_CHK(n.i64[0]);
        STASH_CHK(n.i64[1]);
        STASH_CHK(o.i64[0]);
        STASH_CHK(o.i64[1]);

        STASH_CHK(p.i64[0]);
        STASH_CHK(p.i64[1]);
        STASH_CHK(q.i64[0]);
        STASH_CHK(q.i64[1]);
        STASH_CHK(r.i64[0]);
        STASH_CHK(r.i64[1]);
        STASH_CHK(s.i64[0]);
        STASH_CHK(s.i64[1]);
        STASH_CHK(t.i64[0]);
        STASH_CHK(t.i64[1]);

        STASH_CHK(u.i64[0]);
        STASH_CHK(u.i64[1]);
        STASH_CHK(v.i64[0]);
        STASH_CHK(v.i64[1]);
        STASH_CHK(w.i64[0]);
        STASH_CHK(w.i64[1]);
        STASH_CHK(x.i64[0]);
        STASH_CHK(x.i64[1]);
        STASH_CHK(y.i64[0]);
        STASH_CHK(y.i64[1]);

        STASH_CHK(z.i64[0]);
        STASH_CHK(z.i64[1]);
        #undef STASH_CHK
    }

    if (shadow_ctx_stack->stack_lock != vmctx->stack->stack_lock) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "ctx-stack lock deviation, expected:%ju observed:%ju",
            shadow_ctx_stack->stack_lock, vmctx->stack->stack_lock
        );
        return TACTYK_TESTSTATE__FAIL;
    }
    if (shadow_ctx_stack->stack_position != vmctx->stack->stack_position) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "ctx-stack position deviation, expected:%ju observed:%ju",
            shadow_ctx_stack->stack_position, vmctx->stack->stack_position
        );
        return TACTYK_TESTSTATE__FAIL;
    }

    for (uint64_t i = 0; i < TACTYK_ASMVM__VM_STACK_SIZE; i += 1) {
        struct tactyk_asmvm__vm_stack_entry *se = &vmctx->stack->entries[i];
        struct tactyk_asmvm__vm_stack_entry *shse = &vmctx->stack->entries[i];
        if (shse->dest_command_map != se->dest_command_map) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack dest-command-map deviation, expected:%p observed:%p",
                shse->dest_command_map, se->dest_command_map
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->dest_function_map != se->dest_function_map) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack dest-function-map deviation, expected:%p observed:%p",
                shse->dest_function_map, se->dest_function_map
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->source_command_map != se->source_command_map) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack source-command-map deviation, expected:%p observed:%p",
                shse->source_command_map, se->source_command_map
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->dest_jump_index != se->dest_jump_index) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack dest-jump-index deviation, expected:%ju observed:%ju",
                shse->dest_jump_index, se->dest_jump_index
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->source_return_index != se->source_return_index) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack source-return-index deviation, expected:%ju observed:%ju",
                shse->source_return_index, se->source_return_index
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->source_lwcallstack_floor != se->source_lwcallstack_floor) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack source-lwcs-floor deviation, expected:%u observed:%u",
                shse->source_lwcallstack_floor, se->source_lwcallstack_floor
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        if (shse->source_mctxstack_floor != se->source_mctxstack_floor) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "ctx-stack source-mctx-floor deviation, expected:%u observed:%u",
                shse->source_mctxstack_floor, se->source_mctxstack_floor
            );
            return TACTYK_TESTSTATE__FAIL;
        }
    }

    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__CONTINUE(struct tactyk_dblock__DBlock *spec) {
    return TACTYK_TESTSTATE__CONTINUE;
}

uint64_t tactyk_test__RETURN(struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *retval = spec->token->next;
    ccall_retval = 0;
    if (retval != NULL) {
        tactyk_dblock__try_parseint(&ccall_retval, retval);
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

        if (test->adjust == NULL) {
            tactyk_test__report("state adjustment function is undefined");
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
