#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "ttest.h"
#include "tactyk_dblock.h"

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
        sprintf(test_state->report, "context status deviation, expected:%ju observed:%ju", (uint64_t)ival, vmctx->STATUS);
        return TACTYK_TESTSTATE__FAIL;
    }
}

uint64_t tactyk_test__TEST_STACKLOCK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    uint64_t uival = 0;
    if (!tactyk_dblock__try_parseuint(&uival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_ctx_stack->stack_lock = vmctx->stack->stack_lock;
    if (vmctx->stack->stack_lock == uival) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "stack lock deviation, expected:%ju observed:%ju", uival, vmctx->stack->stack_lock);
        return TACTYK_TESTSTATE__FAIL;
    }
}
uint64_t tactyk_test__TEST_STACKPOSITION(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_ctx_stack->stack_position = vmctx->stack->stack_position;
    if (vmctx->stack->stack_position == ival) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "stack position deviation, expected:%jd observed:%jd", ival, vmctx->stack->stack_position);
        return TACTYK_TESTSTATE__FAIL;
    }
}

/*
struct tactyk_asmvm__vm_stack_entry {
    void *source_command_map;
    uint64_t source_return_index;
    uint32_t source_lwcallstack_floor;
    uint32_t source_mctxstack_floor;
    void *dest_command_map;
    void *dest_function_map;
    uint64_t dest_jump_index;
};
*/
uint64_t tactyk_test__TEST_STACK__STACK_ENTRY(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *index_param = spec->token->next;
    uint64_t idx = 0;
    if (!tactyk_dblock__try_parseuint(&idx, index_param)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    if (idx < 0) {
        return TACTYK_TESTSTATE__PASS;
    }

    struct tactyk_asmvm__vm_stack_entry *ctx_st_entry = &vmctx->stack->entries[idx];
    struct tactyk_asmvm__vm_stack_entry *shadow_st_entry = &shadow_ctx_stack->entries[idx];

    bool permissive = false;

    struct tactyk_asmvm__Program *dest_program = NULL;
    struct tactyk_asmvm__Program *source_program = tprg;

    struct tactyk_dblock__DBlock *testitem = spec->child;
    while (testitem != NULL) {
        struct tactyk_dblock__DBlock *token = testitem->token;

        if (tactyk_dblock__equals_c_string(token, "dest-program")) {
            token = token->next;
            dest_program = tactyk_dblock__get(programs, token);
            shadow_st_entry->dest_command_map = dest_program->command_map;
            shadow_st_entry->dest_function_map = dest_program->function_map;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-program")) {
            token = token->next;
            source_program = tactyk_dblock__get(programs, token);
            shadow_st_entry->source_command_map = source_program->command_map;
        }
        else if (tactyk_dblock__equals_c_string(token, "jumptarget")) {
            token = token->next;
            uint64_t jtarget = 0;
            if (!tactyk_dblock__try_parseuint(&jtarget, token)) {
                // should probably allow jump targets reference by name (it isn't conveniently stored)
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid jump target: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->dest_jump_index = jtarget;
        }
        else if (tactyk_dblock__equals_c_string(token, "returntarget")) {
            token = token->next;
            uint64_t jtarget = 0;
            if (!tactyk_dblock__try_parseuint(&jtarget, token)) {
                // should probably allow jump target reference by name (it isn't conveniently stored)
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid jump target: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_return_index = jtarget;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-lwcsfloor")) {
            token = token->next;
            uint64_t lwcsfloor = 0;
            if (!tactyk_dblock__try_parseuint(&lwcsfloor, token)) {
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid lw-callstack floor: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
        }
        else if (tactyk_dblock__equals_c_string(token, "src-mctxfloor")) {
            token = token->next;
            uint64_t mctxfloor = 0;
            if (!tactyk_dblock__try_parseuint(&mctxfloor, token)) {
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid lw-callstack floor: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_mctxstack_floor = mctxfloor;
        }
        testitem = testitem->next;
    }

    struct tactyk_dblock__DBlock *permissive_param = index_param->next;
    if ( (permissive_param != NULL) && tactyk_dblock__equals_c_string(permissive_param, "*")) {
        memcpy(shadow_st_entry, ctx_st_entry, sizeof( struct tactyk_asmvm__vm_stack_entry));
    }
    return TACTYK_TESTSTATE__PASS;
}


uint64_t tactyk_test__TEST_CALLBACK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_cbid = spec->token->next;
    uint64_t uival = 0;
    if (!tactyk_dblock__try_parseuint(&uival, expected_cbid)) {
        tactyk_test__report("Callback-id parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (uival > 8) {
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "Invalid callback id: %ju", uival);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (uival == callback_id) {
        callback_id = 0;        // The callback has been properly accounted for at this point.
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "Incorrect callback -- expected id: %ju, observed id:%ju", uival, callback_id);
        return TACTYK_TESTSTATE__FAIL;
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
    //struct tactyk_dblock__DBlock *value = spec->token->next;
    return false;
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
    switch(entry->offset) {
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

    target = &vmctx->memblocks[entry->offset];
    target->base_address = mbll->base_address;
    target->array_bound = abound;
    target->element_bound = ebound;

    shadow_target = &shadow_memblocks[entry->offset];
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
    //struct tactyk_asmvm__memblock_lowlevel *shadow_mbs = NULL;
    if (src_program == NULL) {
        src_program = tprg;
        //shadow_mbs = shadow_memblocks;
    }
    else {
        //shadow_mbs = tactyk_dblock__get(shadow_memblock_sets, name);
        reg_ofs = reg_ofs->next;
        name = name->next;
    }

    struct tactyk_asmvm__memblock_highlevel *mbhl = NULL;
    struct tactyk_asmvm__memblock_lowlevel *mbll = NULL;
    //struct tactyk_asmvm__memblock_lowlevel *shadow_mbll = NULL;

    uint64_t mb_index = 0;
    if (tactyk_dblock__try_parseuint(&mb_index, name)) {
        mbhl = tactyk_dblock__index_allocated(src_program->memory_layout_hl->store, mb_index);
        mbll = tactyk_dblock__index_allocated(src_program->memory_layout_ll, mb_index);
        //shadow_mbll = &shadow_mbs[mb_index];
    }
    else {
        mbhl = tactyk_dblock__get(src_program->memory_layout_hl, name);
        mbll = tactyk_dblock__index(src_program->memory_layout_ll, mbhl->memblock_id);
        //shadow_mbll = &shadow_mbs[mbhl->memblock_id];
    }

    struct tactyk_asmvm__memblock_lowlevel *target = &vmctx->active_memblocks[entry->offset-1];
    struct tactyk_asmvm__memblock_lowlevel *shadow_target = &shadow_vmctx->active_memblocks[entry->offset-1];
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
    if ( target->offset != mbll->offset ) {
        snprintf(
            test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
            "memory binding deviation -- expected memblock offset=%u observed memblock offset=%u",
            mbll->offset, target->offset
        );
        return TACTYK_TESTSTATE__FAIL;
    }

    int64_t observed_ofs = 0;

    switch(entry->offset) {
        case 1: {
            observed_ofs = (uint8_t*) vmctx->reg.rADDR1 - (uint8_t*)target->base_address;
            shadow_vmctx->reg.rADDR1 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 2: {
            observed_ofs = (uint8_t*) vmctx->reg.rADDR2 - (uint8_t*)target->base_address;
            shadow_vmctx->reg.rADDR2 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 3: {
            observed_ofs = (uint8_t*) vmctx->reg.rADDR3 - (uint8_t*)target->base_address;
            shadow_vmctx->reg.rADDR3 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
        case 4: {
            observed_ofs = (uint8_t*) vmctx->reg.rADDR4 - (uint8_t*)target->base_address;
            shadow_vmctx->reg.rADDR4 = (uint64_t*) &target->base_address[expected_ofs];
            break;
        }
    }

    shadow_target->base_address = target->base_address;
    shadow_target->array_bound = expected_abound;
    shadow_target->element_bound = expected_ebound;
    shadow_target->memblock_index = target->memblock_index;
    shadow_target->offset = target->offset;

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

bool tactyk_test__SET_REGISTER (struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return false;
    }
    switch(entry->offset) {
        case 1: {
            vmctx->reg.rLWCSI = (uint64_t) ival;
            shadow_vmctx->reg.rLWCSI = (uint64_t) ival;
            return true;
        }
        case 2: {
            vmctx->reg.rMCSI = (uint64_t) ival;
            shadow_vmctx->reg.rMCSI = (uint64_t) ival;
            return true;
        }
        case 10: {
            vmctx->reg.rA = (uint64_t) ival;
            shadow_vmctx->reg.rA = (uint64_t) ival;
            return true;
        }
        case 11: {
            vmctx->reg.rB = (uint64_t) ival;
            shadow_vmctx->reg.rB = (uint64_t) ival;
            return true;
        }
        case 12: {
            vmctx->reg.rC = (uint64_t) ival;
            shadow_vmctx->reg.rC = (uint64_t) ival;
            return true;
        }
        case 13: {
            vmctx->reg.rD = (uint64_t) ival;
            shadow_vmctx->reg.rD = (uint64_t) ival;
            return true;
        }
        case 14: {
            vmctx->reg.rE = (uint64_t) ival;
            shadow_vmctx->reg.rE = (uint64_t) ival;
            return true;
        }
        case 15: {
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

uint64_t tactyk_test__TEST_REGISTER(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    int64_t stval = 0;
    switch(valtest_spec->offset) {
        case 1: {
            stval = vmctx->reg.rLWCSI;
            shadow_vmctx->reg.rLWCSI = vmctx->reg.rLWCSI;
            break;
        }
        case 2: {
            stval = vmctx->reg.rMCSI;
            shadow_vmctx->reg.rMCSI = vmctx->reg.rMCSI;
            break;
        }
        case 10: {
            stval = vmctx->reg.rA;
            shadow_vmctx->reg.rA = vmctx->reg.rA;
            break;
        }
        case 11: {
            stval = vmctx->reg.rB;
            shadow_vmctx->reg.rB = vmctx->reg.rB;
            break;
        }
        case 12: {
            stval = vmctx->reg.rC;
            shadow_vmctx->reg.rC = vmctx->reg.rC;
            break;
        }
        case 13: {
            stval = vmctx->reg.rD;
            shadow_vmctx->reg.rD = vmctx->reg.rD;
            break;
        }
        case 14: {
            stval = vmctx->reg.rE;
            shadow_vmctx->reg.rE = vmctx->reg.rE;
            break;
        }
        case 15: {
            stval = vmctx->reg.rF;
            shadow_vmctx->reg.rF = vmctx->reg.rF;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }

    if (stval == ival) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "deviation on register %s, expected:%jd observed:%jd", valtest_spec->name, ival, stval);
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
    switch(entry->offset) {
        case 0: {
            vmctx->reg.xA.f64[0] = fval;
            shadow_vmctx->reg.xA.f64[0] = fval;
            break;
        }
        case 1: {
            vmctx->reg.xB.f64[0] = fval;
            shadow_vmctx->reg.xB.f64[0] = fval;
            break;
        }
        case 2: {
            vmctx->reg.xC.f64[0] = fval;
            shadow_vmctx->reg.xC.f64[0] = fval;
            break;
        }
        case 3: {
            vmctx->reg.xD.f64[0] = fval;
            shadow_vmctx->reg.xD.f64[0] = fval;
            break;
        }
        case 4: {
            vmctx->reg.xE.f64[0] = fval;
            shadow_vmctx->reg.xE.f64[0] = fval;
            break;
        }
        case 5: {
            vmctx->reg.xF.f64[0] = fval;
            shadow_vmctx->reg.xF.f64[0] = fval;
            break;
        }
        case 6: {
            vmctx->reg.xG.f64[0] = fval;
            shadow_vmctx->reg.xG.f64[0] = fval;
            break;
        }
        case 7: {
            vmctx->reg.xH.f64[0] = fval;
            shadow_vmctx->reg.xH.f64[0] = fval;
            break;
        }
        case 8: {
            vmctx->reg.xI.f64[0] = fval;
            shadow_vmctx->reg.xI.f64[0] = fval;
            break;
        }
        case 9: {
            vmctx->reg.xJ.f64[0] = fval;
            shadow_vmctx->reg.xJ.f64[0] = fval;
            break;
        }
        case 10: {
            vmctx->reg.xK.f64[0] = fval;
            shadow_vmctx->reg.xK.f64[0] = fval;
            break;
        }
        case 11: {
            vmctx->reg.xL.f64[0] = fval;
            shadow_vmctx->reg.xL.f64[0] = fval;
            break;
        }
        case 12: {
            vmctx->reg.xM.f64[0] = fval;
            shadow_vmctx->reg.xM.f64[0] = fval;
            break;
        }
        case 13: {
            vmctx->reg.xN.f64[0] = fval;
            shadow_vmctx->reg.xN.f64[0] = fval;
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
    switch(valtest_spec->offset) {
        case 0: {
            stval = vmctx->reg.xA.f64[0];
            shadow_vmctx->reg.xA.f64[0] = vmctx->reg.xA.f64[0];
            break;
        }
        case 1: {
            stval = vmctx->reg.xB.f64[0];
            shadow_vmctx->reg.xB.f64[0] = vmctx->reg.xB.f64[0];
            break;
        }
        case 2: {
            stval = vmctx->reg.xC.f64[0];
            shadow_vmctx->reg.xC.f64[0] = vmctx->reg.xC.f64[0];
            break;
        }
        case 3: {
            stval = vmctx->reg.xD.f64[0];
            shadow_vmctx->reg.xD.f64[0] = vmctx->reg.xD.f64[0];
            break;
        }
        case 4: {
            stval = vmctx->reg.xE.f64[0];
            shadow_vmctx->reg.xE.f64[0] = vmctx->reg.xE.f64[0];
            break;
        }
        case 5: {
            stval = vmctx->reg.xF.f64[0];
            shadow_vmctx->reg.xF.f64[0] = vmctx->reg.xF.f64[0];
            break;
        }
        case 6: {
            stval = vmctx->reg.xG.f64[0];
            shadow_vmctx->reg.xG.f64[0] = vmctx->reg.xG.f64[0];
            break;
        }
        case 7: {
            stval = vmctx->reg.xH.f64[0];
            shadow_vmctx->reg.xH.f64[0] = vmctx->reg.xH.f64[0];
            break;
        }
        case 8: {
            stval = vmctx->reg.xI.f64[0];
            shadow_vmctx->reg.xI.f64[0] = vmctx->reg.xI.f64[0];
            break;
        }
        case 9: {
            stval = vmctx->reg.xJ.f64[0];
            shadow_vmctx->reg.xJ.f64[0] = vmctx->reg.xJ.f64[0];
            break;
        }
        case 10: {
            stval = vmctx->reg.xK.f64[0];
            shadow_vmctx->reg.xK.f64[0] = vmctx->reg.xK.f64[0];
            break;
        }
        case 11: {
            stval = vmctx->reg.xL.f64[0];
            shadow_vmctx->reg.xL.f64[0] = vmctx->reg.xL.f64[0];
            break;
        }
        case 12: {
            stval = vmctx->reg.xM.f64[0];
            shadow_vmctx->reg.xM.f64[0] = vmctx->reg.xM.f64[0];
            break;
        }
        case 13: {
            stval = vmctx->reg.xN.f64[0];
            shadow_vmctx->reg.xN.f64[0] = vmctx->reg.xN.f64[0];
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

uint64_t tactyk_test__TEST_LWCALL_STACK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    uint64_t num_tokens = tactyk_dblock__count_tokens(spec);
    if (num_tokens < 2) {
        tactyk_test__report("[lwcstack] Not enough arguments");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    struct tactyk_dblock__DBlock *ofs_token = spec->token->next;
    struct tactyk_dblock__DBlock *val_token = ofs_token->next;
    struct tactyk_dblock__DBlock *rpt_token = val_token->next;

    uint64_t ofs = 0;
    uint64_t val = 0;
    uint64_t rpt = 1;

    if (!tactyk_dblock__try_parseuint(&ofs, ofs_token)) {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, ofs_token);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "[lwcstack] Invalid offset: %s", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (!tactyk_dblock__try_parseuint(&val, val_token)) {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, val_token);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "[lwcstack] Invalid stack index: %s", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if ((rpt_token != NULL) && (!tactyk_dblock__try_parseuint(&rpt, rpt_token))) {
        char buf[64];
        tactyk_dblock__export_cstring(buf, 64, rpt_token);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "[lwcstack] Invalid repetition qualifier: %s", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    uint64_t limit = ofs + rpt;
    if (limit > TACTYK_ASMVM__LWCALL_STACK_SIZE) {
        limit = TACTYK_ASMVM__LWCALL_STACK_SIZE;
    }

    for (;ofs < limit; ofs++) {
        if (vmctx->lwcall_stack[ofs] != val) {
            snprintf(
                test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                "LW Call Stack deviation at offset %ju, expected=%ju observed=%u",
                ofs, val, vmctx->lwcall_stack[ofs]
            );
            return TACTYK_TESTSTATE__FAIL;
        }
        shadow_lwcall_stack[ofs] = val;
    }
    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__TEST_STASH(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *ofs_token = spec->token->next;
    struct tactyk_dblock__DBlock *fieldname_token = ofs_token->next;

    uint64_t ofs = 0;
    if (!tactyk_dblock__try_parseuint(&ofs, ofs_token)) {
        ofs = vmctx->reg.rMCSI;
        fieldname_token = ofs_token;
    }
    else if (ofs >= TACTYK_ASMVM__MCTX_STACK_SIZE) {
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "mctx stack offset is out of bounds: %ju", ofs);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    struct tactyk_dblock__DBlock *val_token = fieldname_token->next;

    struct tactyk_asmvm__MicrocontextStash *stash = &vmctx->microcontext_stack[ofs];
    struct tactyk_asmvm__MicrocontextStash *shstash = &shadow_mctxstack[ofs];

    char fn[64];
    tactyk_dblock__export_cstring(fn, 64, fieldname_token);

    double f64val = 0;
    int64_t ival = 0;
    //double st_f64val = 0;
    int64_t st_ival = 0;

    bool pass = false;
    bool field_matched = false;

    tactyk_dblock__try_parsedouble(&f64val, val_token);
    tactyk_dblock__try_parseint(&ival, val_token);

    #define STASH_TEST(NAME, FIELD) \
    else if (strncmp(fn, #NAME, 64) == 0) { \
        pass = (ival == stash->FIELD); \
        shstash->FIELD = ival; \
        st_ival = stash->FIELD; \
        field_matched = true; \
    }
    #define STASH_ATEST(NAME, FIELD, TYPE) \
    else if (strncmp(fn, NAME, 64) == 0) { \
        pass = ((TYPE)ival == stash->FIELD); \
        shstash->FIELD = (TYPE)ival; \
        st_ival = stash->FIELD; \
        field_matched = true; \
    }
    if ( (strncmp(fn, "addr", 4) == 0) && (strlen(fn) == 5) ) {
        uint64_t aofs = fn[4] - '1';
        struct tactyk_asmvm__memblock_lowlevel *mbll = &stash->memblocks[aofs];
        struct tactyk_asmvm__memblock_lowlevel *shmbll = &shstash->memblocks[aofs];
        if (ival == 0) {
            struct tactyk_dblock__DBlock *prgref = val_token;
            struct tactyk_dblock__DBlock *addrref = val_token->next;
            struct tactyk_asmvm__Program *prg = tactyk_dblock__get(programs, prgref);
            if (prg == NULL) {
                addrref = val_token;
                prg = tprg;
            }
            struct tactyk_dblock__DBlock *addrofs = addrref->next;
            struct tactyk_asmvm__memblock_highlevel *mbhl = tactyk_dblock__get(prg->memory_layout_hl, addrref);
            if (mbhl == NULL) {
                char buf[64];
                tactyk_dblock__export_cstring(buf, 64, prgref);
                snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "Undefined memblock:  '%s'", buf);
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            ival += (int64_t)mbhl->data;

            if (addrofs != NULL) {
                uint64_t uival = 0;
                tactyk_dblock__try_parseuint(&uival, addrofs);
                ival += uival;
            }
        }
        pass = ((uint8_t*)ival == mbll->base_address);
        shmbll->base_address = (uint8_t*)ival;
        field_matched = true;
    }
    STASH_ATEST("addr1.array_bound", memblocks[0].array_bound, uint32_t)
    STASH_ATEST("addr1.element_bound", memblocks[0].element_bound, uint32_t)
    STASH_ATEST("addr1.index", memblocks[0].memblock_index, uint32_t)
    STASH_ATEST("addr1.offset", memblocks[0].offset, uint32_t)
    STASH_ATEST("addr2.array_bound", memblocks[1].array_bound, uint32_t)
    STASH_ATEST("addr2.element_bound", memblocks[1].element_bound, uint32_t)
    STASH_ATEST("addr2.index", memblocks[1].memblock_index, uint32_t)
    STASH_ATEST("addr2.offset", memblocks[1].offset, uint32_t)
    STASH_ATEST("addr3.array_bound", memblocks[2].array_bound, uint32_t)
    STASH_ATEST("addr3.element_bound", memblocks[2].element_bound, uint32_t)
    STASH_ATEST("addr3.index", memblocks[2].memblock_index, uint32_t)
    STASH_ATEST("addr3.offset", memblocks[2].offset, uint32_t)
    STASH_ATEST("addr4.array_bound", memblocks[3].array_bound, uint32_t)
    STASH_ATEST("addr4.element_bound", memblocks[3].element_bound, uint32_t)
    STASH_ATEST("addr4.index", memblocks[3].memblock_index, uint32_t)
    STASH_ATEST("addr4.offset", memblocks[3].offset, uint32_t)

    STASH_TEST(al, a.i64[0])
    STASH_TEST(ah, a.i64[1])
    STASH_TEST(bl, b.i64[0])
    STASH_TEST(bh, b.i64[1])
    STASH_TEST(cl, c.i64[0])
    STASH_TEST(ch, c.i64[1])
    STASH_TEST(dl, d.i64[0])
    STASH_TEST(dh, d.i64[1])
    STASH_TEST(el, e.i64[0])
    STASH_TEST(eh, e.i64[1])

    STASH_TEST(fl, f.i64[0])
    STASH_TEST(fh, f.i64[1])
    STASH_TEST(gl, g.i64[0])
    STASH_TEST(gh, g.i64[1])
    STASH_TEST(hl, h.i64[0])
    STASH_TEST(hh, h.i64[1])
    STASH_TEST(il, i.i64[0])
    STASH_TEST(ih, i.i64[1])
    STASH_TEST(jl, j.i64[0])
    STASH_TEST(jh, j.i64[1])

    STASH_TEST(kl, k.i64[0])
    STASH_TEST(kh, k.i64[1])
    STASH_TEST(ll, l.i64[0])
    STASH_TEST(lh, l.i64[1])
    STASH_TEST(ml, m.i64[0])
    STASH_TEST(mh, m.i64[1])
    STASH_TEST(nl, n.i64[0])
    STASH_TEST(nh, n.i64[1])
    STASH_TEST(ol, o.i64[0])
    STASH_TEST(oh, o.i64[1])

    STASH_TEST(pl, p.i64[0])
    STASH_TEST(ph, p.i64[1])
    STASH_TEST(ql, q.i64[0])
    STASH_TEST(qh, q.i64[1])
    STASH_TEST(rl, r.i64[0])
    STASH_TEST(rh, r.i64[1])
    STASH_TEST(sl, s.i64[0])
    STASH_TEST(sh, s.i64[1])
    STASH_TEST(tl, t.i64[0])
    STASH_TEST(th, t.i64[1])

    STASH_TEST(ul, u.i64[0])
    STASH_TEST(uh, u.i64[1])
    STASH_TEST(vl, v.i64[0])
    STASH_TEST(vh, v.i64[1])
    STASH_TEST(wl, w.i64[0])
    STASH_TEST(wh, w.i64[1])
    STASH_TEST(xl, x.i64[0])
    STASH_TEST(xh, x.i64[1])
    STASH_TEST(yl, y.i64[0])
    STASH_TEST(yh, y.i64[1])

    STASH_TEST(zl, z.i64[0])
    STASH_TEST(zh, z.i64[1])
    #undef STASH_TEST
    #undef STASH_ATEST

    if (field_matched) {
        if (pass) {
            return TACTYK_TESTSTATE__PASS;
        }
        else {
            sprintf(test_state->report, "deviation on mctx-stash field '%s', expected:%jd observed:%jd", fn, ival, st_ival);
            return TACTYK_TESTSTATE__FAIL;
        }
    }
    else {
        sprintf(test_state->report, "unrecognized mctx field: '%s'", fn);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

}

uint64_t tactyk_test__TEST_CCALL_ARGUMENT(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    assert(entry->offset < 6);
    int64_t cbval = ccall_args[entry->offset];
    if (ival == cbval) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "callback argument #%ju deviation, expected:%jd observed:%jd", entry->offset, ival, cbval);
        return TACTYK_TESTSTATE__FAIL;
    }
}
