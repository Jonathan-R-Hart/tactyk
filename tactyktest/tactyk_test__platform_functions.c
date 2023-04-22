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
    char prefix[256];
    sprintf(prefix, "");

    #define CHK(ACCESSOR, FORMAT, DESCRIPTION) \
        if (SHADOW_OBJ->ACCESSOR != REAL_OBJ->ACCESSOR) { \
            snprintf( \
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE, \
                "deviation: %s%s, expected: "FORMAT", observed: "FORMAT, \
                prefix, DESCRIPTION, SHADOW_OBJ->ACCESSOR, REAL_OBJ->ACCESSOR \
            ); \
            return TACTYK_TESTSTATE__FAIL; \
        }

    #define SHADOW_OBJ shadow_vmctx
    #define REAL_OBJ vmctx
    CHK(max_instruction_pointer, "%ju", "instruction count")
    CHK(subcontext, "%p", "subcontext")
    CHK(memblocks, "%p", "memblocks pointer")
    CHK(memblock_count, "%ju", "memblock count")
    CHK(lwcall_stack, "%p", "lwcall stack pointer")
    CHK(microcontext_stack, "%p", "mctx stack pointer")
    CHK(microcontext_stack_offset, "%ju", "mctx stack offset")
    CHK(lwcall_stack_floor, "%u", "lwcall stack floor")
    CHK(mctx_stack_floor, "%u", "mctx stack floor")
    CHK(vm, "%p", "vm pointer")
    CHK(stack, "%p", "main stack pointer")
    CHK(program_map, "%p", "program instruction map")
    CHK(hl_program_ref, "%p", "high-level program pointer")
    CHK(instruction_index, "%ju", "program instruction index")
    CHK(STATUS, "%ju", "execution status")
    CHK(signature, "%ju", "context pointer validation signature")
    CHK(extra, "%ju", "extra")

    CHK(reg.rA, "%jd", "Register rA");
    CHK(reg.rB, "%jd", "Register rB");
    CHK(reg.rC, "%jd", "Register rC");
    CHK(reg.rD, "%jd", "Register rD");
    CHK(reg.rE, "%jd", "Register rE");
    CHK(reg.rF, "%jd", "Register rF");

    CHK(reg.xA.f64[0], "%f", "Register xA-low");
    CHK(reg.xA.f64[1], "%f", "Register xA-high");
    CHK(reg.xB.f64[0], "%f", "Register xB-low");
    CHK(reg.xB.f64[1], "%f", "Register xB-high");
    CHK(reg.xC.f64[0], "%f", "Register xC-low");
    CHK(reg.xC.f64[1], "%f", "Register xC-high");
    CHK(reg.xD.f64[0], "%f", "Register xD-low");
    CHK(reg.xD.f64[1], "%f", "Register xD-high");
    CHK(reg.xE.f64[0], "%f", "Register xE-low");
    CHK(reg.xE.f64[1], "%f", "Register xE-high");

    CHK(reg.xF.f64[0], "%f", "Register xF-low");
    CHK(reg.xF.f64[1], "%f", "Register xF-high");
    CHK(reg.xG.f64[0], "%f", "Register xG-low");
    CHK(reg.xG.f64[1], "%f", "Register xG-high");
    CHK(reg.xH.f64[0], "%f", "Register xH-low");
    CHK(reg.xH.f64[1], "%f", "Register xH-high");
    CHK(reg.xI.f64[0], "%f", "Register xI-low");
    CHK(reg.xI.f64[1], "%f", "Register xI-high");
    CHK(reg.xJ.f64[0], "%f", "Register xJ-low");
    CHK(reg.xJ.f64[1], "%f", "Register xJ-high");

    CHK(reg.xK.f64[0], "%f", "Register xK-low");
    CHK(reg.xK.f64[1], "%f", "Register xK-high");
    CHK(reg.xL.f64[0], "%f", "Register xL-low");
    CHK(reg.xL.f64[1], "%f", "Register xL-high");
    CHK(reg.xM.f64[0], "%f", "Register xM-low");
    CHK(reg.xM.f64[1], "%f", "Register xM-high");
    CHK(reg.xN.f64[0], "%f", "Register xN-low");
    CHK(reg.xN.f64[1], "%f", "Register xN-high");

    CHK(reg.xTEMPA.f64[0], "%f", "Register xTEMPA-low");
    CHK(reg.xTEMPA.f64[1], "%f", "Register xTEMPA-high");
    CHK(reg.xTEMPB.f64[0], "%f", "Register xTEMPB-low");
    CHK(reg.xTEMPB.f64[1], "%f", "Register xTEMPB-high");

    #undef SHADOW_OBJ
    #undef REAL_OBJ
    #define REAL_OBJ (&vmctx->active_memblocks[0])
    #define SHADOW_OBJ (&shadow_vmctx->active_memblocks[0])
    CHK(base_address, "%p", "memblock #1 base address");
    CHK(array_bound, "%u", "memblock #1 array bound");
    CHK(element_bound, "%u", "memblock #1 element bound");
    CHK(memblock_index, "%u", "memblock #1 index");
    CHK(offset, "%u", "memblock #1 offset");
    #undef SHADOW_OBJ
    #undef REAL_OBJ
    #define REAL_OBJ (&vmctx->active_memblocks[1])
    #define SHADOW_OBJ (&shadow_vmctx->active_memblocks[1])
    CHK(base_address, "%p", "memblock #2 base address");
    CHK(array_bound, "%u", "memblock #2 array bound");
    CHK(element_bound, "%u", "memblock #2 element bound");
    CHK(memblock_index, "%u", "memblock #2 index");
    CHK(offset, "%u", "memblock #2 offset");
    #undef SHADOW_OBJ
    #undef REAL_OBJ
    #define REAL_OBJ (&vmctx->active_memblocks[2])
    #define SHADOW_OBJ (&shadow_vmctx->active_memblocks[2])
    CHK(base_address, "%p", "memblock #3 base address");
    CHK(array_bound, "%u", "memblock #3 array bound");
    CHK(element_bound, "%u", "memblock #3 element bound");
    CHK(memblock_index, "%u", "memblock #3 index");
    CHK(offset, "%u", "memblock #3 offset");
    #undef SHADOW_OBJ
    #undef REAL_OBJ
    #define REAL_OBJ (&vmctx->active_memblocks[3])
    #define SHADOW_OBJ (&shadow_vmctx->active_memblocks[3])
    CHK(base_address, "%p", "memblock #4 base address");
    CHK(array_bound, "%u", "memblock #4 array bound");
    CHK(element_bound, "%u", "memblock #4 element bound");
    CHK(memblock_index, "%u", "memblock #4 index");
    CHK(offset, "%u", "memblock #4 offset");
    #undef SHADOW_OBJ
    #undef REAL_OBJ

    for (uint64_t i = 0; i < TACTYK_ASMVM__MEMBLOCK_CAPACITY; i++) {
        struct tactyk_asmvm__memblock_lowlevel *mbll = &vmctx->memblocks[i];
        struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = &shadow_memblocks[i];
        uint64_t len = mbll->array_bound + mbll->element_bound -1;
        if (mbll->base_address != NULL) {
            for (uint64_t j = 0; j < len; j += 1) {
                if (mbll->base_address[j] != shadow_mbll->base_address[j]) {
                    snprintf(
                        test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                        "deviation: memblock #%ju [data], offset: %ju, expected: %u observed: %u",
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
                "deviation: lwcall stack at offset %ju: expected: %u observed: %u",
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
                    "deviation: mctx stash #%ju, memblock #%ju base address: expected: %p observed: %p",
                    i, j, shadow_mbll->base_address, mbll->base_address
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->array_bound != shadow_mbll->array_bound) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "deviation: mctx stash #%ju, memblock #%ju array bound: expected: %u observed: %u",
                    i, j, shadow_mbll->array_bound, mbll->array_bound
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->element_bound != shadow_mbll->element_bound) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "deviation: mctx stash #%ju, memblock #%ju element bound: expected: %u observed: %u",
                    i, j, shadow_mbll->element_bound, mbll->element_bound
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->memblock_index != shadow_mbll->memblock_index) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "deviation: mctx stash #%ju, memblock #%ju index: expected: %u observed: %u",
                    i, j, shadow_mbll->memblock_index, mbll->memblock_index
                );
                return TACTYK_TESTSTATE__FAIL;
            }
            if (mbll->offset != shadow_mbll->offset) {
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "deviation: mctx stash #%ju, memblock #%ju offset: expected: %u observed: %u",
                    i, j, shadow_mbll->offset, mbll->offset
                );
                return TACTYK_TESTSTATE__FAIL;
            }
        }

        sprintf(prefix, "stash #%ju ", i);
        #undef DESCRIPTION_PREFIX
        #define DESCRIPTION_PREFIX stash_prefix
        #define REAL_OBJ stash
        #define SHADOW_OBJ shadow_stash
        CHK(a.i64[0], "%jd", "a-low");
        CHK(a.i64[1], "%jd", "a-high");
        CHK(b.i64[0], "%jd", "b-low");
        CHK(b.i64[1], "%jd", "b-high");
        CHK(c.i64[0], "%jd", "c-low");
        CHK(c.i64[1], "%jd", "c-high");
        CHK(d.i64[0], "%jd", "d-low");
        CHK(d.i64[1], "%jd", "d-high");
        CHK(e.i64[0], "%jd", "e-low");
        CHK(e.i64[1], "%jd", "e-high");

        CHK(f.i64[0], "%jd", "f-low");
        CHK(f.i64[1], "%jd", "f-high");
        CHK(g.i64[0], "%jd", "g-low");
        CHK(g.i64[1], "%jd", "g-high");
        CHK(h.i64[0], "%jd", "h-low");
        CHK(h.i64[1], "%jd", "h-high");
        CHK(i.i64[0], "%jd", "i-low");
        CHK(i.i64[1], "%jd", "i-high");
        CHK(j.i64[0], "%jd", "j-low");
        CHK(j.i64[1], "%jd", "j-high");

        CHK(k.i64[0], "%jd", "k-low");
        CHK(k.i64[1], "%jd", "k-high");
        CHK(l.i64[0], "%jd", "l-low");
        CHK(l.i64[1], "%jd", "l-high");
        CHK(m.i64[0], "%jd", "m-low");
        CHK(m.i64[1], "%jd", "m-high");
        CHK(n.i64[0], "%jd", "n-low");
        CHK(n.i64[1], "%jd", "n-high");
        CHK(o.i64[0], "%jd", "o-low");
        CHK(o.i64[1], "%jd", "o-high");

        CHK(p.i64[0], "%jd", "p-low");
        CHK(p.i64[1], "%jd", "p-high");
        CHK(q.i64[0], "%jd", "q-low");
        CHK(q.i64[1], "%jd", "q-high");
        CHK(r.i64[0], "%jd", "r-low");
        CHK(r.i64[1], "%jd", "r-high");
        CHK(s.i64[0], "%jd", "s-low");
        CHK(s.i64[1], "%jd", "s-high");
        CHK(t.i64[0], "%jd", "t-low");
        CHK(t.i64[1], "%jd", "t-high");

        CHK(u.i64[0], "%jd", "u-low");
        CHK(u.i64[1], "%jd", "u-high");
        CHK(v.i64[0], "%jd", "v-low");
        CHK(v.i64[1], "%jd", "v-high");
        CHK(w.i64[0], "%jd", "w-low");
        CHK(w.i64[1], "%jd", "w-high");
        CHK(x.i64[0], "%jd", "x-low");
        CHK(x.i64[1], "%jd", "x-high");
        CHK(y.i64[0], "%jd", "y-low");
        CHK(y.i64[1], "%jd", "y-high");

        CHK(z.i64[0], "%jd", "z-low");
        CHK(z.i64[1], "%jd", "z-high");
        #undef DESCRIPTION_PREFIX
        #undef SHADOW_OBJ
        #undef REAL_OBJ
    }
    sprintf(prefix, "");

    #define REAL_OBJ vmctx->stack
    #define SHADOW_OBJ shadow_ctx_stack
    CHK(stack_lock, "%ju", "primary stack lock");
    CHK(stack_position, "%jd", "primary stack position");
    #undef SHADOW_OBJ
    #undef REAL_OBJ

    for (uint64_t i = 0; i < TACTYK_ASMVM__VM_STACK_SIZE; i += 1) {
        struct tactyk_asmvm__vm_stack_entry *se = &vmctx->stack->entries[i];
        struct tactyk_asmvm__vm_stack_entry *shse = &shadow_ctx_stack->entries[i];

        sprintf(prefix, "primary stack item #%ju ", i);
        #define DESCRIPTION_PREFIX prefix
        #define REAL_OBJ se
        #define SHADOW_OBJ shse
        CHK(dest_command_map, "%p", "destination instruction map");
        CHK(dest_function_map, "%p", "destination function map");
        CHK(source_command_map, "%p", "source instruction map");
        CHK(dest_jump_index, "%ju", "jump target");
        CHK(source_return_index, "%ju", "return target");
        CHK(source_lwcallstack_floor, "%u", "source lwcall stack floor");
        CHK(source_mctxstack_floor, "%u", "source mctx stack floor");
        #undef SHADOW_OBJ
        #undef REAL_OBJ
        #undef DESCRIPTION_PREFIX
    }

    #undef CHK

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