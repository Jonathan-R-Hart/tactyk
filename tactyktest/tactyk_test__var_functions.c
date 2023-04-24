#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "ttest.h"
#include "tactyk_dblock.h"

uint64_t tactyk_test__TEST_CONTEXT_STATUS(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_vmctx->STATUS = ival;
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__TEST_CONTEXT_MCTX_FLOOR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_vmctx->mctx_stack_floor = ival;
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__TEST_CONTEXT_LWCS_FLOOR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_vmctx->lwcall_stack_floor = ival;
    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__TEST_CONTEXT_PROGRAM(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    if (expected_value == NULL) {
        tactyk_test__report("TEST_CONTEXT_PROGRAM - No program specified");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    struct tactyk_asmvm__Program *prog = tactyk_dblock__get(programs, expected_value);

    if (prog == NULL) {
        char buf[256];
        tactyk_dblock__export_cstring(buf, 256, expected_value);
        snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "TEST_CONTEXT_PROGRAM - Failed to resolve program: %s", buf);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    shadow_vmctx->program_map = prog->command_map;
    shadow_vmctx->instruction_count = prog->length;
    return TACTYK_TESTSTATE__PASS;
}

uint64_t tactyk_test__TEST_STACKLOCK(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    uint64_t uival = 0;
    if (!tactyk_dblock__try_parseuint(&uival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_ctx_stack->stack_lock = uival;
    return TACTYK_TESTSTATE__PASS;
}
uint64_t tactyk_test__TEST_STACKPOSITION(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    shadow_ctx_stack->stack_position = ival;
    return TACTYK_TESTSTATE__PASS;
}

int64_t getFunctionID(struct tactyk_asmvm__Program *prog, struct tactyk_dblock__DBlock *label) {
    struct tactyk_asmvm__identifier *id = tactyk_dblock__get(prog->functions, label);
    if (id == NULL) {
        return -1;
    }
    else {
        for (uint64_t i = 0; i < prog->functions->element_count; i++) {
            struct tactyk_asmvm__identifier *iid = tactyk_dblock__index(prog->functions->store, i);
            if (id == iid) {
                return i;
            }
        }
        return -1;
    }
}

int64_t getLabelPosition(struct tactyk_asmvm__Program *prog, struct tactyk_dblock__DBlock *label) {
    struct tactyk_asmvm__identifier *id = tactyk_dblock__get(prog->functions, label);
    if (id != NULL) {
        return id->value;
    }
    else {
        return -1;
    }
}

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

    struct tactyk_asmvm__Program *dest_program = NULL;
    struct tactyk_asmvm__Program *source_program = NULL;

    struct tactyk_dblock__DBlock *testitem = spec->child;
    while (testitem != NULL) {
        struct tactyk_dblock__DBlock *token = testitem->token;
        if (tactyk_dblock__equals_c_string(token, "dest-program")) {
            token = token->next;
            dest_program = tactyk_dblock__get(programs, token);
            shadow_st_entry->dest_command_map = dest_program->command_map;
            shadow_st_entry->dest_function_map = dest_program->function_map;
            shadow_st_entry->dest_max_iptr = dest_program->length;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-program")) {
            token = token->next;
            source_program = tactyk_dblock__get(programs, token);
            shadow_st_entry->source_command_map = source_program->command_map;
            shadow_st_entry->source_max_iptr = source_program->length;
        }
        else if (tactyk_dblock__equals_c_string(token, "jumptarget")) {
            token = token->next;
            if (token == NULL) {
                tactyk_test__report("stackentry-Jumptarget: target not specified.");
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            if (dest_program == NULL) {
                tactyk_test__report("stackenrty-Jumptarget: destination program not declared.");
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            {
                int64_t jtarget = getFunctionID(dest_program, token);
                if (jtarget != -1) {
                    shadow_st_entry->dest_jump_index = (uint32_t)jtarget;
                    goto handle_next_item;
                }
            }
            {
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
        }
        else if (tactyk_dblock__equals_c_string(token, "returntarget")) {
            token = token->next;
            if (token == NULL) {
                tactyk_test__report("stackentry-returntarget: target not specified.");
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            uint64_t jtarget = 0;
            if (source_program != NULL) {
                int64_t lbl_pos = getLabelPosition(source_program, token);
                if (lbl_pos != -1) {
                    jtarget = lbl_pos;
                    token = token->next;
                }
            }
            uint64_t joffset = 0;

            if ( (token != NULL) && (!tactyk_dblock__try_parseuint(&joffset, token))) {
                // should probably allow jump target reference by name (it isn't conveniently stored)
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid return target: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_return_index = jtarget+joffset;
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
            shadow_st_entry->source_lwcallstack_floor = lwcsfloor;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-mctxfloor")) {
            token = token->next;
            uint64_t mctxfloor = 0;
            if (!tactyk_dblock__try_parseuint(&mctxfloor, token)) {
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid mctxstack floor: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_mctxstack_floor = mctxfloor;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-lwcspos")) {
            token = token->next;
            uint64_t lwcsposition = 0;
            if (!tactyk_dblock__try_parseuint(&lwcsposition, token)) {
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid lw-callstack position: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_lwcallstack_position = lwcsposition;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-mctxpos")) {
            token = token->next;
            uint64_t mctxposition = 0;
            if (!tactyk_dblock__try_parseuint(&mctxposition, token)) {
                char buf[256];
                tactyk_dblock__export_cstring(buf, 256, token);
                snprintf(
                    test_state->report, TACTYK_TEST__REPORT_BUFSIZE,
                    "invalid mctxstack position: %s\n",
                    buf
                );
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
            shadow_st_entry->source_mctxstack_position = mctxposition;
        }
        handle_next_item:
        testitem = testitem->next;
    }
    struct tactyk_dblock__DBlock *rpt_param = index_param->next;
    if (rpt_param != NULL) {
        uint64_t limit = 0;
        if (tactyk_dblock__try_parseuint(&limit, rpt_param)) {
            limit += idx;
            if (limit > TACTYK_ASMVM__VM_STACK_SIZE) {
                limit = TACTYK_ASMVM__VM_STACK_SIZE;
            }
            struct tactyk_asmvm__vm_stack_entry *src_entry = &shadow_ctx_stack->entries[idx];
            for (uint64_t i = 1; i < limit; i++) {
                struct tactyk_asmvm__vm_stack_entry *dest_entry = &shadow_ctx_stack->entries[i];
                memcpy(dest_entry, src_entry, sizeof(struct tactyk_asmvm__vm_stack_entry));
            }

        }
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
        return TACTYK_TESTSTATE__PASS;
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
                shadow_mbll->base_address[idx] = (uint8_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "word") ) {
               *((uint16_t*) &shadow_mbll->base_address[idx]) = (uint16_t)ival;
            }

            else if ( tactyk_dblock__equals_c_string(item_type, "dword") ) {
               *((uint32_t*) &shadow_mbll->base_address[idx]) = (uint32_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "qword") ) {
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
    return TACTYK_TESTSTATE__PASS;
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
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    if (num_tokens >= 4) {
        struct tactyk_dblock__DBlock *array_bound = reg_ofs->next;
        struct tactyk_dblock__DBlock *element_bound = reg_ofs->next->next;
        if (!tactyk_dblock__try_parseuint(&expected_abound, array_bound)) {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, array_bound);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "array-bound '%s' is not an integer.", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
        if (!tactyk_dblock__try_parseuint(&expected_ebound, element_bound)) {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, element_bound);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "element-bound '%s' is not an integer.", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    else {
        expected_abound = mbll->array_bound;
        expected_ebound = mbll->element_bound;
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

    shadow_target->base_address = mbll->base_address;
    shadow_target->array_bound = mbll->array_bound;
    shadow_target->element_bound = mbll->element_bound;
    shadow_target->memblock_index = mbll->memblock_index;
    shadow_target->offset = mbll->offset;

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

uint64_t tactyk_test__TEST_REGISTER(struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;

    int64_t ival = 0;
    if (!tactyk_dblock__try_parseint(&ival, expected_value)) {
        tactyk_test__report("Test value parameter is not an integer");
        return TACTYK_TESTSTATE__TEST_ERROR;
    }
    uint64_t uival = (uint64_t)ival;
    switch(valtest_spec->offset) {
        case 1: {
            shadow_vmctx->reg.rLWCSI = uival;
            break;
        }
        case 2: {
            shadow_vmctx->reg.rMCSI = uival;
            break;
        }
        case 10: {
            shadow_vmctx->reg.rA = uival;
            break;
        }
        case 11: {
            shadow_vmctx->reg.rB = uival;
            break;
        }
        case 12: {
            shadow_vmctx->reg.rC = uival;
            break;
        }
        case 13: {
            shadow_vmctx->reg.rD = uival;
            break;
        }
        case 14: {
            shadow_vmctx->reg.rE = uival;
            break;
        }
        case 15: {
            shadow_vmctx->reg.rF = uival;
            break;
        }
        default: {
            tactyk_test__report("Test element-offset is invalid");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    return TACTYK_TESTSTATE__PASS;
}

// floating-point test
// This deviates substantially from the established pattern:
//      Due to floating point rounding errors, this directly copies actual state into the shadow state,
//      then performs its own state transition check using a floating point comparison with error tolerance.
//      If the comparison test fails, this will pre-empt the general-purpose state transition tracker by returning a failure.
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
    if (tactyk_test__approximately_eq(stval, fval)) {
        return TACTYK_TESTSTATE__PASS;
    }
    else {
        sprintf(test_state->report, "deviation: Register %s, expected:%f observed:%f", valtest_spec->name, fval, stval);
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

    struct tactyk_asmvm__MicrocontextStash *shstash = &shadow_mctxstack[ofs];

    char fn[64];
    tactyk_dblock__export_cstring(fn, 64, fieldname_token);

    double f64val = 0;
    int64_t ival = 0;

    bool field_matched = false;

    tactyk_dblock__try_parsedouble(&f64val, val_token);
    tactyk_dblock__try_parseint(&ival, val_token);

    #define STASH_TEST(NAME, FIELD) \
    else if (strncmp(fn, #NAME, 64) == 0) { \
        shstash->FIELD = ival; \
        field_matched = true; \
    }
    #define STASH_ATEST(NAME, FIELD, TYPE) \
    else if (strncmp(fn, NAME, 64) == 0) { \
        shstash->FIELD = (TYPE)ival; \
        field_matched = true; \
    }
    if ( (strncmp(fn, "addr", 4) == 0) && (strlen(fn) == 5) ) {
        uint64_t aofs = fn[4] - '1';
        //struct tactyk_asmvm__memblock_lowlevel *mbll = &stash->memblocks[aofs];
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

    if (!field_matched) {
        sprintf(test_state->report, "unrecognized mctx field: '%s'", fn);
        return TACTYK_TESTSTATE__TEST_ERROR;
    }

    return TACTYK_TESTSTATE__PASS;
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
