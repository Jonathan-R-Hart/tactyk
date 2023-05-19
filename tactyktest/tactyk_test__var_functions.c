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
    shadow_vmctx->memblocks = (struct tactyk_asmvm__memblock_lowlevel*) prog->memory_layout_ll->data;
    shadow_vmctx->memblock_count = prog->memory_layout_ll->element_count;
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
            shadow_st_entry->dest_memblocks = (struct tactyk_asmvm__memblock_lowlevel*) dest_program->memory_layout_ll->data;
            shadow_st_entry->dest_memblock_count = dest_program->memory_layout_ll->element_count;
        }
        else if (tactyk_dblock__equals_c_string(token, "src-program")) {
            token = token->next;
            source_program = tactyk_dblock__get(programs, token);
            shadow_st_entry->source_command_map = source_program->command_map;
            shadow_st_entry->source_max_iptr = source_program->length;
            shadow_st_entry->source_memblocks = (struct tactyk_asmvm__memblock_lowlevel*) source_program->memory_layout_ll->data;
            shadow_st_entry->source_memblock_count = source_program->memory_layout_ll->element_count;
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
        struct tactyk_dblock__DBlock *item_type = NULL;
        struct tactyk_dblock__DBlock *expected_value = NULL;
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
            item_type = iindex->next;
            expected_value = item_type->next;
            if (expected_value == NULL) {
                expected_value = item_type;
                item_type = NULL;
            }
            int64_t ival = 0;
            double fval = 0;
            bool int_success = false;
            double dbl_success = false;

            if (tactyk_dblock__try_parsedouble(&fval, expected_value)) {
                dbl_success = true;
            }
            if (tactyk_dblock__try_parseint(&ival, expected_value)) {
                int_success = true;
            }

            if ( tactyk_dblock__equals_c_string(item_type, "float") || tactyk_dblock__equals_c_string(item_type, "float64")) {
                if (!dbl_success) { goto ERR_FLOAT; }
                double observed_val = *((double*)&mbll->base_address[idx]);
                if (tactyk_test__approximately_eq(fval, observed_val, precision_f64)) {
                    *((double*)&shadow_mbll->base_address[idx]) = observed_val;
                    return TACTYK_TESTSTATE__PASS;
                }
                else {
                    *((double*)&shadow_mbll->base_address[idx]) = fval;
                    sprintf(test_state->report, "deviation [f64]: memblock #%ju at offset %ju, expected:%f observed:%f", mbhl->memblock_id, idx, fval, observed_val);
                    return TACTYK_TESTSTATE__FAIL;
                }
            }
            if ( tactyk_dblock__equals_c_string(item_type, "float32") ) {
                if (!dbl_success) { goto ERR_FLOAT; }
                float observed_val = *((float*)&mbll->base_address[idx]);
                double f64v = (double)observed_val;
                if (tactyk_test__approximately_eq(fval, f64v, precision_f32)) {
                    *((float*)&shadow_mbll->base_address[idx]) = observed_val;
                    return TACTYK_TESTSTATE__PASS;
                }
                else {
                    *((float*)&shadow_mbll->base_address[idx]) = fval;
                    sprintf(test_state->report, "deviation [f32]: memblock #%ju at offset %ju, expected:%f observed:%f", mbhl->memblock_id, idx, fval, f64v);
                    return TACTYK_TESTSTATE__FAIL;
                }
            }
            if ( tactyk_dblock__equals_c_string(item_type, "byte") ) {
                if (!int_success) { goto ERR_INTEGER; }
                shadow_mbll->base_address[idx] = (uint8_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "word") ) {
                if (!int_success) { goto ERR_INTEGER; }
               *((uint16_t*) &shadow_mbll->base_address[idx]) = (uint16_t)ival;
            }

            else if ( tactyk_dblock__equals_c_string(item_type, "dword") ) {
                if (!int_success) { goto ERR_INTEGER; }
               *((uint32_t*) &shadow_mbll->base_address[idx]) = (uint32_t)ival;
            }
            else if ( tactyk_dblock__equals_c_string(item_type, "qword") ) {
                if (!int_success) { goto ERR_INTEGER; }
               *((uint64_t*) &shadow_mbll->base_address[idx]) = (uint64_t)ival;
            }
            else {
                goto ERR_OTHER;
            }
        }
        return TACTYK_TESTSTATE__PASS;

        ERR_FLOAT: {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, expected_value);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not a floating point number", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        ERR_INTEGER: {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, expected_value);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "'%s' is not an integer", buf);
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        ERR_OTHER: {
            char buf[64];
            tactyk_dblock__export_cstring(buf, 64, item_type);
            snprintf(test_state->report, TACTYK_TEST__REPORT_BUFSIZE, "Unrecognized data type: %s", buf );
            return TACTYK_TESTSTATE__TEST_ERROR;
        }
    }
    return TACTYK_TESTSTATE__PASS;


}

uint64_t tactyk_test__TEST_ADDR(struct tactyk_test_entry *entry, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *expected_value = spec->token->next;
    if (tactyk_dblock__equals_c_string(expected_value, "NULL")) {
        struct tactyk_asmvm__memblock_lowlevel *shadow_target = &shadow_vmctx->active_memblocks[entry->offset-1];
        shadow_target->base_address = NULL;
        shadow_target->element_bound = 0;
        shadow_target->array_bound = 0;
        shadow_target->memblock_index = 0;
        shadow_target->offset = 0;
        switch(entry->offset) {
            case 1: {
                shadow_vmctx->reg.rADDR1 = NULL;
                break;
            }
            case 2: {
                shadow_vmctx->reg.rADDR2 = NULL;
                break;
            }
            case 3: {
                shadow_vmctx->reg.rADDR3 = NULL;
                break;
            }
            case 4: {
                shadow_vmctx->reg.rADDR4 = NULL;
                break;
            }
        }
        return TACTYK_TESTSTATE__PASS;
    }
    
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
uint64_t tactyk_test__TEST_XMM_REGISTER (struct tactyk_test_entry *valtest_spec, struct tactyk_dblock__DBlock *spec) {
    struct tactyk_dblock__DBlock *ftype = spec->token->next;
    struct tactyk_dblock__DBlock *token = ftype;
    
    bool bits32_mode = false;
    bool fpmode = true;
    
    double precision = precision_f64;
    if ( tactyk_dblock__equals_c_string(ftype, "f32") || tactyk_dblock__equals_c_string(ftype, "float32")) {
        bits32_mode = true;
        fpmode = true;
        token = token->next;
        precision = precision_f32;
    }
    else if ( tactyk_dblock__equals_c_string(ftype, "f64") || tactyk_dblock__equals_c_string(ftype, "float64")) {
        bits32_mode = false;
        fpmode = true;
        token = token->next;
        precision = precision_f64;
    }
    else if ( tactyk_dblock__equals_c_string(ftype, "i32") || tactyk_dblock__equals_c_string(ftype, "int32")) {
        bits32_mode = true;
        fpmode = false;
        token = token->next;
    }
    else if ( tactyk_dblock__equals_c_string(ftype, "i64") || tactyk_dblock__equals_c_string(ftype, "int64")) {
        bits32_mode = false;
        fpmode = false;
        token = token->next;
    }
    
    // float32+ is for 64-bit floats which have been promoted from the 32-bit format.  A reduced precision is needed for it
    else  if ( tactyk_dblock__equals_c_string(ftype, "f32+") || tactyk_dblock__equals_c_string(ftype, "float32+")) {
        bits32_mode = false;
        fpmode = true;
        token = token->next;
        precision = precision_f32;
    }
    
    struct tactyk_dblock__DBlock *expected_value[4];
    memset(expected_value, 0, sizeof(expected_value));
    for (uint64_t i = 0; i < 4; i++) {
        expected_value[i] = token;
        token = token->next;
        if (token == NULL) {
            break;
        }
    }
    
    if (bits32_mode && fpmode) {
        double fval = 0;
        double fval2 = 0;
        double fval3 = 0;
        double fval4 = 0;
        double stval = 0;
        double stval2 = 0;
        double stval3 = 0;
        double stval4 = 0;
        if (expected_value[0] == NULL) {
            error("Test-XMM -- First value specifier is NULL.", spec);
        }
        if (!tactyk_dblock__try_parsedouble(&fval, expected_value[0])) {
            error("Test-XMM -- Not a floating point number",expected_value[0]);
        }
        if (expected_value[1] != NULL) {
            tactyk_dblock__try_parsedouble(&fval2, expected_value[1]);
        }
        if (expected_value[2] != NULL) {
            printf("ev %p\n", expected_value[2]);
            tactyk_dblock__try_parsedouble(&fval3, expected_value[2]);
        }
        if (expected_value[3] != NULL) {
            tactyk_dblock__try_parsedouble(&fval4, expected_value[3]);
        }
        
        # define CHK_FIELDS_F32(INDEX, REGNAME) \
        case INDEX: { \
            stval = vmctx->reg.REGNAME.f32[0]; \
            shadow_vmctx->reg.REGNAME.f32[0] = vmctx->reg.REGNAME.f32[0];\
            stval2 = vmctx->reg.REGNAME.f32[1]; \
            shadow_vmctx->reg.REGNAME.f32[1] = vmctx->reg.REGNAME.f32[1];\
            stval3 = vmctx->reg.REGNAME.f32[2]; \
            shadow_vmctx->reg.REGNAME.f32[2] = vmctx->reg.REGNAME.f32[2];\
            stval4 = vmctx->reg.REGNAME.f32[3]; \
            shadow_vmctx->reg.REGNAME.f32[3] = vmctx->reg.REGNAME.f32[3];\
            break; \
        }
    
        switch(valtest_spec->offset) {
            CHK_FIELDS_F32(0, xA)
            CHK_FIELDS_F32(1, xB)
            CHK_FIELDS_F32(2, xC)
            CHK_FIELDS_F32(3, xD)
            CHK_FIELDS_F32(4, xE)
            CHK_FIELDS_F32(5, xF)
            CHK_FIELDS_F32(6, xG)
            CHK_FIELDS_F32(7, xH)
            CHK_FIELDS_F32(8, xI)
            CHK_FIELDS_F32(9, xJ)
            CHK_FIELDS_F32(10, xK)
            CHK_FIELDS_F32(11, xL)
            CHK_FIELDS_F32(12, xM)
            CHK_FIELDS_F32(13, xN)
            CHK_FIELDS_F32(14, xTEMPA)
            CHK_FIELDS_F32(15, xTEMPB)
            
            default: {
                tactyk_test__report("Test element-offset is invalid");
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
        }
        
        #undef CHK_FIELDS_F32
        
        if (
            tactyk_test__approximately_eq(stval, fval, precision) && 
            tactyk_test__approximately_eq(stval2, fval2, precision) && 
            tactyk_test__approximately_eq(stval3, fval3, precision) && 
            tactyk_test__approximately_eq(stval4, fval4, precision)
        ) {
            return TACTYK_TESTSTATE__PASS;
        }
        else {
            sprintf(test_state->report, "deviation: Register %s, expected:%f,%f,%f,%f observed:%f,%f,%f,%f", valtest_spec->name, fval, fval2, fval3, fval4, stval,stval2,stval3,stval4);
            return TACTYK_TESTSTATE__FAIL;
        }
    }
    else if (!bits32_mode && fpmode) {
        double fval = 0;
        double fval2 = 0;
        double stval = 0;
        double stval2 = 0;
        
        if (expected_value[0] == NULL) {
            error("Test-XMM -- First value specifier is NULL.", spec);
        }
        if (!tactyk_dblock__try_parsedouble(&fval, expected_value[0])) {
            error("Test-XMM -- Not a floating point number",expected_value[0]);
        }
        if (expected_value[1] != NULL) {
            tactyk_dblock__try_parsedouble(&fval2, expected_value[1]);
        }
        
        # define CHK_FIELDS_F64(INDEX, REGNAME) \
        case INDEX: { \
            stval = vmctx->reg.REGNAME.f64[0]; \
            shadow_vmctx->reg.REGNAME.f64[0] = vmctx->reg.REGNAME.f64[0];\
            stval2 = vmctx->reg.REGNAME.f64[1]; \
            shadow_vmctx->reg.REGNAME.f64[1] = vmctx->reg.REGNAME.f64[1];\
            break; \
        }
    
        switch(valtest_spec->offset) {
            CHK_FIELDS_F64(0, xA)
            CHK_FIELDS_F64(1, xB)
            CHK_FIELDS_F64(2, xC)
            CHK_FIELDS_F64(3, xD)
            CHK_FIELDS_F64(4, xE)
            CHK_FIELDS_F64(5, xF)
            CHK_FIELDS_F64(6, xG)
            CHK_FIELDS_F64(7, xH)
            CHK_FIELDS_F64(8, xI)
            CHK_FIELDS_F64(9, xJ)
            CHK_FIELDS_F64(10, xK)
            CHK_FIELDS_F64(11, xL)
            CHK_FIELDS_F64(12, xM)
            CHK_FIELDS_F64(13, xN)
            CHK_FIELDS_F64(14, xTEMPA)
            CHK_FIELDS_F64(15, xTEMPB)
            
            default: {
                tactyk_test__report("Test element-offset is invalid");
                return TACTYK_TESTSTATE__TEST_ERROR;
            }
        }
        
        #undef CHK_FIELDS_F64
        
        if (tactyk_test__approximately_eq(stval, fval, precision) && tactyk_test__approximately_eq(stval2, fval2, precision)) {
            return TACTYK_TESTSTATE__PASS;
        }
        else {
            sprintf(test_state->report, "deviation: Register %s, expected:%f,%f observed:%f,%f", valtest_spec->name, fval, fval2, stval,stval2);
            return TACTYK_TESTSTATE__FAIL;
        }
    }
    else if (bits32_mode && !fpmode) {
        int64_t ival = 0;
        uint64_t uival1 = 0;
        uint64_t uival2 = 0;
        uint64_t uival3 = 0;
        uint64_t uival4 = 0;

        if (expected_value[0] == NULL) {
            tactyk_test__report("XMM-register-test: No value specified.");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        if (tactyk_dblock__try_parseint(&ival, expected_value[0])) {
            uival1 = (uint64_t)ival;
            if (tactyk_dblock__try_parseint(&ival, expected_value[1])) {
                uival2 = (uint64_t)ival;
            }
            if (tactyk_dblock__try_parseint(&ival, expected_value[2])) {
                uival3 = (uint64_t)ival;
            }
            if (tactyk_dblock__try_parseint(&ival, expected_value[3])) {
                uival4 = (uint64_t)ival;
            }
        }
        else if ( (expected_value[0]->length > 0) && (tactyk_dblock__getchar(expected_value[0], 0) == '\'') ) {
            // setup string test by copying raw bytes onto integer variables (which are then written to shadow context register content)
            // This assumes tactyk_dblock__export_cstring zero-fills the buffer
            char buf[256];
            tactyk_dblock__export_cstring(buf, 256, expected_value[0]);
            memcpy(&uival1, &buf[1], 4);
            memcpy(&uival2, &buf[5], 4);
            memcpy(&uival3, &buf[9], 4);
            memcpy(&uival4, &buf[13], 4);
        }

        # define CHK_FIELDS_I32(INDEX, REGNAME) \
        case INDEX: { \
            shadow_vmctx->reg.REGNAME.i32[0] = uival1;\
            shadow_vmctx->reg.REGNAME.i32[1] = uival2;\
            shadow_vmctx->reg.REGNAME.i32[2] = uival3;\
            shadow_vmctx->reg.REGNAME.i32[3] = uival4;\
            break; \
        }
        
        switch(valtest_spec->offset) {
            CHK_FIELDS_I32(0, xA)
            CHK_FIELDS_I32(1, xB)
            CHK_FIELDS_I32(2, xC)
            CHK_FIELDS_I32(3, xD)
            CHK_FIELDS_I32(4, xE)
            CHK_FIELDS_I32(5, xF)
            CHK_FIELDS_I32(6, xG)
            CHK_FIELDS_I32(7, xH)
            CHK_FIELDS_I32(8, xI)
            CHK_FIELDS_I32(9, xJ)
            CHK_FIELDS_I32(10, xK)
            CHK_FIELDS_I32(11, xL)
            CHK_FIELDS_I32(12, xM)
            CHK_FIELDS_I32(13, xN)
            CHK_FIELDS_I32(14, xTEMPA)
            CHK_FIELDS_I32(15, xTEMPB)
        }
        
        #undef CHK_FIELDS_I32

        return TACTYK_TESTSTATE__PASS;
    }
    else if (!bits32_mode && !fpmode) {
        int64_t ival = 0;
        uint64_t uival1 = 0;
        uint64_t uival2 = 0;

        if (expected_value[0] == NULL) {
            tactyk_test__report("XMM-register-test: No value specified.");
            return TACTYK_TESTSTATE__TEST_ERROR;
        }

        if (tactyk_dblock__try_parseint(&ival, expected_value[0])) {
            uival1 = (uint64_t)ival;
            if (tactyk_dblock__try_parseint(&ival, expected_value[1])) {
                uival2 = (uint64_t)ival;
            }
        }
        else if ( (expected_value[0]->length > 0) && (tactyk_dblock__getchar(expected_value[0], 0) == '\'') ) {
            // setup string test by copying raw bytes onto integer variables (which are then written to shadow context register content)
            // This assumes tactyk_dblock__export_cstring zero-fills the buffer
            char buf[256];
            tactyk_dblock__export_cstring(buf, 256, expected_value[0]);
            memcpy(&uival1, &buf[1], 8);
            memcpy(&uival2, &buf[9], 8);
        }

        # define CHK_FIELDS_I64(INDEX, REGNAME) \
        case INDEX: { \
            shadow_vmctx->reg.REGNAME.i64[0] = uival1;\
            shadow_vmctx->reg.REGNAME.i64[1] = uival2;\
            break; \
        }
        
        switch(valtest_spec->offset) {
            CHK_FIELDS_I64(0, xA)
            CHK_FIELDS_I64(1, xB)
            CHK_FIELDS_I64(2, xC)
            CHK_FIELDS_I64(3, xD)
            CHK_FIELDS_I64(4, xE)
            CHK_FIELDS_I64(5, xF)
            CHK_FIELDS_I64(6, xG)
            CHK_FIELDS_I64(7, xH)
            CHK_FIELDS_I64(8, xI)
            CHK_FIELDS_I64(9, xJ)
            CHK_FIELDS_I64(10, xK)
            CHK_FIELDS_I64(11, xL)
            CHK_FIELDS_I64(12, xM)
            CHK_FIELDS_I64(13, xN)
            CHK_FIELDS_I64(14, xTEMPA)
            CHK_FIELDS_I64(15, xTEMPB)
        }
        
        #undef CHK_FIELDS_I64

        return TACTYK_TESTSTATE__PASS;
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

    int64_t ival = 0;
    int64_t ival2 = 0;

    bool field_matched = false;

    if (tactyk_dblock__getchar(val_token, 0) == '\'') {
        char buf[256];
        tactyk_dblock__export_cstring(buf, 256, val_token);
        memcpy(&ival, &buf[1], 8);
        memcpy(&ival2, &buf[9], 8);
    }
    else {
        tactyk_dblock__try_parseint(&ival, val_token);
    }

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

    #define STASH_BTEST(NAME) \
    else if (strncmp(fn, #NAME, 64) == 0) { \
        shstash->NAME.i64[0] = ival; \
        shstash->NAME.i64[1] = ival2; \
        field_matched = true; \
    }
    if (false) {}
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
    
    STASH_TEST(s26l, s26.i64[0])
    STASH_TEST(s26h, s26.i64[1])
    STASH_TEST(s27l, s27.i64[0])
    STASH_TEST(s27h, s27.i64[1])
    STASH_TEST(s28l, s28.i64[0])
    STASH_TEST(s28h, s28.i64[1])
    STASH_TEST(s29l, s29.i64[0])
    STASH_TEST(s29h, s29.i64[1])
    STASH_TEST(s30l, s30.i64[0])
    STASH_TEST(s30h, s30.i64[1])
    STASH_TEST(s31l, s31.i64[0])
    STASH_TEST(s31h, s31.i64[1])

    STASH_BTEST(a)
    STASH_BTEST(b)
    STASH_BTEST(c)
    STASH_BTEST(d)
    STASH_BTEST(e)
    STASH_BTEST(f)
    STASH_BTEST(g)
    STASH_BTEST(h)
    STASH_BTEST(i)
    STASH_BTEST(j)
    STASH_BTEST(k)
    STASH_BTEST(l)
    STASH_BTEST(m)
    STASH_BTEST(n)
    STASH_BTEST(o)
    STASH_BTEST(p)
    STASH_BTEST(q)
    STASH_BTEST(r)
    STASH_BTEST(s)
    STASH_BTEST(t)
    STASH_BTEST(u)
    STASH_BTEST(v)
    STASH_BTEST(w)
    STASH_BTEST(x)
    STASH_BTEST(y)
    STASH_BTEST(z)
    STASH_BTEST(s26)
    STASH_BTEST(s27)
    STASH_BTEST(s28)
    STASH_BTEST(s29)
    STASH_BTEST(s30)
    STASH_BTEST(s31)

    #undef STASH_TEST
    #undef STASH_ATEST
    #undef STASH_BTEST

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
