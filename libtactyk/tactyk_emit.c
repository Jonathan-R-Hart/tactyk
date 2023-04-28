//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/mman.h>

#include "tactyk_emit.h"
#include "tactyk_util.h"
#include "tactyk_asmvm.h"
#include "tactyk.h"
#include "tactyk_alloc.h"

#include "tactyk_assembler.h"
#include "tactyk_dblock.h"

#include <setjmp.h>
#define FETCH_VALUE_CAPACITY 255
#define TACTYK_EMIT_LABELBUFFER_CAPACITY 1024

// tags prepended to internal labels to guarantee uniqueness
#define TACTYK_EMIT__BRANCHTARGET_PREFIX "lbl#"
#define TACTYK_EMIT__COMMAND_PREFIX "cmd#"

//
#define TACTYK_EMIT__BRANCHTARGET_PREFIX_SIZE (sizeof(TACTYK_EMIT__BRANCHTARGET_PREFIX)-1)
#define TACTYK_EMIT__COMMAND_PREFIX_SIZE (sizeof(TACTYK_EMIT__COMMAND_PREFIX)-1)

struct tactyk_emit__Context emit_settings;

#define TACTYK_EMIT__ERROR_MSG_MAXLENGTH 1024
static char error_message[TACTYK_EMIT__ERROR_MSG_MAXLENGTH];
static jmp_buf err_jbuf;

// Error handling
// Internal errors result in process termination.  External errors are deferred to the host application.
//
// The host application may handle errors by attaching an error handler to the context data structure (non-returning function which accepts a c-string).
//
// Detailed explanation:
//      printf() followed by exit(1) for problems that can be caused by misconfiguration (terminates the process)
//      assert() for problems that are supposed to be unpossible (terminates the process)
//      tactyk_emit_error() for problems presumed to be script defects (process continues running, but compilation is halted and the host application is notified (if an error handler is given) )
static tactyk_emit__error_handler error_handler;
void tactyk_emit__default_error_handler(char *msg) {
    printf("TACTYK-EMIT ERROR: '%s'\n", msg);
    exit(1);
}
void tactyk_emit__error(struct tactyk_emit__Context *ctx, void *msg_ptr) {
    char *msg;
    if (tactyk_dblock__is_dblock(msg_ptr)) {
        msg = calloc(1024, 1);
        tactyk_dblock__export_cstring(msg, 1024, (struct tactyk_dblock__DBlock*) msg_ptr);
    }
    else {
        msg = msg_ptr;
    }
    if (msg != error_message) {
        if (ctx->error_handler != NULL) {
            error_handler = ctx->error_handler;
        }
        snprintf(error_message, TACTYK_EMIT__ERROR_MSG_MAXLENGTH, "%s", msg);
    }
    longjmp(err_jbuf, 1);
}

struct tactyk_emit__Context* tactyk_emit__init() {
    struct tactyk_emit__Context *ctx = tactyk_alloc__allocate(1, sizeof(struct tactyk_emit__Context));
    ctx->random_const_fs = tactyk__rand_uint64() & 0x3fffffff;
    ctx->random_const_gs = tactyk__rand_uint64() & 0x3fffffff;

    ctx->api_table = tactyk_dblock__new_table(64);
    ctx->c_api_table = tactyk_dblock__new_table(64);
    ctx->visa_token_constants = tactyk_dblock__new_table(512);
    ctx->visa_token_invmap = NULL;
    ctx->token_handle_count = 0;
    ctx->has_visa_constants = false;
    ctx->operator_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->typespec_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->instruction_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->subroutine_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));

    ctx->global_vars = tactyk_dblock__new_table(256);

    tactyk_dblock__set_persistence_code(ctx->global_vars, 100);
    tactyk_dblock__set_persistence_code(ctx->api_table, 100);
    tactyk_dblock__set_persistence_code(ctx->c_api_table, 100);
    tactyk_dblock__set_persistence_code(ctx->visa_token_constants, 100);
    tactyk_dblock__set_persistence_code(ctx->operator_table, 100);
    tactyk_dblock__set_persistence_code(ctx->typespec_table, 100);
    tactyk_dblock__set_persistence_code(ctx->instruction_table, 100);
    tactyk_dblock__set_persistence_code(ctx->subroutine_table, 100);

    tactyk_dblock__put(ctx->operator_table, "symbol", tactyk_emit__Symbol);

    tactyk_dblock__put(ctx->operator_table, "select-operand", tactyk_emit__SelectOp);
    tactyk_dblock__put(ctx->operator_table, "select-template", tactyk_emit__SelectTemplate);
    tactyk_dblock__put(ctx->operator_table, "select-kw", tactyk_emit__SelectKeyword);
    tactyk_dblock__put(ctx->operator_table, "select-kw2", tactyk_emit__SelectKeyword2);
    tactyk_dblock__put(ctx->operator_table, "flags", tactyk_emit__Flags);

    tactyk_dblock__put(ctx->operator_table, "case", tactyk_emit__Case);
    tactyk_dblock__put(ctx->operator_table, "contains", tactyk_emit__Contains);

    tactyk_dblock__put(ctx->operator_table, "pick", tactyk_emit__Pick);
    tactyk_dblock__put(ctx->operator_table, "operand", tactyk_emit__Operand);
    tactyk_dblock__put(ctx->operator_table, "opt-operand", tactyk_emit__OptionalOperand);
    tactyk_dblock__put(ctx->operator_table, "composite", tactyk_emit__Composite);
    tactyk_dblock__put(ctx->operator_table, "type", tactyk_emit__Type);
    tactyk_dblock__put(ctx->operator_table, "value", tactyk_emit__Value);

    tactyk_dblock__put(ctx->operator_table, "scramble", tactyk_emit__Scramble);
    tactyk_dblock__put(ctx->operator_table, "code", tactyk_emit__Code);

    tactyk_dblock__put(ctx->operator_table, "int-operand", tactyk_emit__IntOperand);
    tactyk_dblock__put(ctx->operator_table, "float-operand", tactyk_emit__FloatOperand);
    tactyk_dblock__put(ctx->operator_table, "string-operand", tactyk_emit__StringOperand);

    tactyk_dblock__put(ctx->operator_table, "sub", tactyk_emit__DoSub);
    tactyk_dblock__put(ctx->operator_table, "nullarg", tactyk_emit__NullArg);

    ctx->active_labels = NULL;
    ctx->active_labels_last = NULL;

    error_handler = tactyk_emit__default_error_handler;

    if (setjmp(err_jbuf)) {
        error_handler(error_message);
        exit(1);
    }

    return ctx;
}

void tactyk_emit__reset(struct tactyk_emit__Context *emitctx) {
    emitctx->local_vars = tactyk_dblock__new_table(256);
    emitctx->script_commands = tactyk_dblock__new_container(8, sizeof(struct tactyk_emit__script_command));
    emitctx->code_template = tactyk_dblock__new(16);
    emitctx->has_visa_constants = false;
}

// release all memory belonging to an emit context.
void tactyk_emit__dispose(struct tactyk_emit__Context *ctx) {
    tactyk_dblock__dispose(ctx->global_vars);
    tactyk_dblock__dispose(ctx->local_vars);
    tactyk_dblock__dispose(ctx->operator_table);
    tactyk_dblock__dispose(ctx->typespec_table);
    tactyk_dblock__dispose(ctx->instruction_table);
    tactyk_dblock__dispose(ctx->subroutine_table);
    tactyk_dblock__dispose(ctx->active_labels);

    tactyk_dblock__dispose(ctx->code_template);

    tactyk_alloc__free(ctx);
}

bool tactyk_emit__comprehend_int_value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data, char *kwname);

struct tactyk_dblock__DBlock* tactyk_emit__fetch_var(struct tactyk_emit__Context *ctx, void *destination, struct tactyk_dblock__DBlock *raw) {
    struct tactyk_dblock__DBlock *varvalue = tactyk_dblock__interpolate(raw, '$', ctx->namechars, ctx->local_vars, ctx->global_vars);
    if (destination != NULL) {
        destination = tactyk_dblock__from_string_or_dblock(destination);
        tactyk_dblock__put(ctx->local_vars, destination, tactyk_dblock__shallow_copy(varvalue));
    }
    return varvalue;
}

// generic subroutine executor.
//  This steps through each child, fetches the operator associated with each child's command, loads any tokens into the local namespace, and invokes it.
//  If no related operator is found, this alternatively acts as a variable assignment operator.
//  If anything returns false, exit.
//  Non-trivial operators should call into this after performing anything specific.
// (trivial operators don't have child operators to run and so have no need for this)
bool tactyk_emit__ExecSubroutine(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *sub_data = data->child;
    while (sub_data != NULL) {
        struct tactyk_dblock__DBlock *name = sub_data->token;

        tactyk_emit__sub_func sfunc = tactyk_dblock__get(ctx->operator_table, name);
        if (sfunc != NULL) {
            if (!sfunc(ctx, sub_data)) {
                return false;
            }
        }
        else {
            struct tactyk_dblock__DBlock *varname = name;
            if (tactyk_dblock__getchar(varname, 0) == '$') {

                struct tactyk_dblock__DBlock *varvalue_raw = varname->next;
                if (varvalue_raw != NULL) {
                    tactyk_emit__fetch_var(ctx, varname, varvalue_raw);
                    goto do_next_sub;
                };
            }
            warn("EMIT -- Unrecognized virtual-ISA command", sub_data);
        }
        do_next_sub:
        sub_data = sub_data->next;
    }
    return true;
}
//  Indirect call into a subroutine.
//  This is for handling reusable components in the general case.
//      (and the general case is expected to only be simple commands which differ from each other only by a single ASM instruction)
bool tactyk_emit__DoSub(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_emit__subroutine_spec *spec = tactyk_dblock__get(ctx->subroutine_table, data->token->next);
    if (spec == NULL) {
        error("EMIT -- Undefined subroutine: ", data);
    }
    spec->func(ctx, spec->vopcfg);
    return true;
}

bool tactyk_emit__ExecInstruction(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_emit__script_command *cmd = ctx->active_command;
    tactyk_dblock__reset_table(ctx->local_vars, true);
    tactyk_dblock__clear(ctx->code_template);
    ctx->valid_parse_result = false;

    ctx->pl_operand_raw = cmd->tokens;
    ctx->pl_operand_resolved = NULL;

    struct tactyk_dblock__DBlock *cmd_idx = tactyk_dblock__from_uint(ctx->iptr);
    struct tactyk_dblock__DBlock *cmd_idx_next = tactyk_dblock__from_uint(ctx->iptr + 1);
    tactyk_dblock__put(ctx->local_vars, "$COMMAND_INDEX", cmd_idx);
    tactyk_dblock__put(ctx->local_vars, "$COMMAND_INDEX_NEXT", cmd_idx_next);
    tactyk_dblock__append(cmd->asm_code, TACTYK_EMIT__COMMAND_PREFIX);
    tactyk_dblock__append(cmd->asm_code, cmd_idx);
    tactyk_dblock__append(cmd->asm_code, ":\n");
    uint64_t code_len = cmd->asm_code->length;

    struct tactyk_dblock__DBlock *next_instruction_label = tactyk_dblock__from_c_string(TACTYK_EMIT__COMMAND_PREFIX);
    tactyk_dblock__append(next_instruction_label, cmd_idx_next);
    tactyk_dblock__put(ctx->local_vars, "$NEXT_INSTRUCTION", next_instruction_label);

    bool result = tactyk_emit__ExecSubroutine(ctx, data);
    // clean up any temp dblocks (mostly dynamically text chunks)
    if (code_len == cmd->asm_code->length) {
        error("EMIT -- Code generation failed", cmd->pl_code);
    }
    ctx->pl_operand_raw = NULL;
    ctx->pl_operand_resolved = NULL;

    return result;
}

void tactyk_emit__add_tactyk_apifunc(struct tactyk_emit__Context *ctx, char *name, tactyk_emit__tactyk_api_function func) {
    struct tactyk_dblock__DBlock *fctn = tactyk_dblock__from_uint((uint64_t)func);
    tactyk_dblock__put(ctx->api_table, name, fctn);
}

void tactyk_emit__add_c_apifunc(struct tactyk_emit__Context *ctx, char *name, void* func) {
    struct tactyk_dblock__DBlock *fctn = tactyk_dblock__from_uint((uint64_t)func);
    tactyk_dblock__put(ctx->c_api_table, name, fctn);
}

void tactyk_emit__init_program(struct tactyk_emit__Context *ctx) {
    ctx->symbol_tables = tactyk_dblock__new_table(128);
    ctx->label_table = tactyk_dblock__new_table(256);
    ctx->const_table = tactyk_dblock__new_table(64);
    ctx->fconst_table = tactyk_dblock__new_table(64);
    ctx->memblock_table = tactyk_dblock__new_table(64);

    ctx->program = tactyk_alloc__allocate(1, sizeof(struct tactyk_asmvm__Program));
    ctx->program->functions = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_asmvm__identifier));
    tactyk_dblock__set_persistence_code(ctx->program->functions, 10);

    tactyk_dblock__put(ctx->symbol_tables, "label", ctx->label_table);
    tactyk_dblock__put(ctx->symbol_tables, "const", ctx->const_table);
    tactyk_dblock__put(ctx->symbol_tables, "fconst", ctx->fconst_table);
    tactyk_dblock__put(ctx->symbol_tables, "mem", ctx->memblock_table);
    tactyk_dblock__put(ctx->symbol_tables, "capi", ctx->c_api_table);
    tactyk_dblock__put(ctx->symbol_tables, "tapi", ctx->api_table);

    ctx->program->memory_layout_ll = tactyk_dblock__new_container(TACTYK_ASMVM__MEMBLOCK_CAPACITY, sizeof(struct tactyk_asmvm__memblock_lowlevel));
    tactyk_dblock__fix(ctx->program->memory_layout_ll);
    tactyk_dblock__make_pseudosafe(ctx->program->memory_layout_ll);
    ctx->program->memory_layout_hl = tactyk_dblock__new_managedobject_table(TACTYK_ASMVM__MEMBLOCK_CAPACITY, sizeof(struct tactyk_asmvm__memblock_highlevel));

    tactyk_dblock__set_persistence_code(ctx->program->memory_layout_ll, 10);
    tactyk_dblock__set_persistence_code(ctx->program->memory_layout_hl, 10);
}

bool tactyk_emit__Type(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *token = data->token->next;
    while (token != NULL) {
        struct tactyk_emit__subroutine_spec *type_applicator = tactyk_dblock__get(ctx->typespec_table, token);
        if (type_applicator == NULL) {
            error("EMIT -- invalid type specifier", token);
        }
        if (type_applicator->func(ctx, type_applicator->vopcfg)) {
            return true;
        }
        token = token->next;
    }
    return false;
}

bool tactyk_emit__IntOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    //struct tactyk_dblock__DBlock *opval = ctx->pl_operand_resolved;
    int64_t ival;
    struct tactyk_dblock__DBlock *kw;

    if (tactyk_dblock__try_parseint(&ival, ctx->pl_operand_resolved)) {
        if (ival < 0) {
            kw = tactyk_dblock__from_c_string("qword");
        }
        else if (ival <= 0xff) {
            kw = tactyk_dblock__from_c_string("byte");
        }
        else if (ival <= 0xffff) {
            kw = tactyk_dblock__from_c_string("word");
        }
        else if (ival <= 0xffffffff) {
            kw = tactyk_dblock__from_c_string("dword");
        }
        else {
            kw = tactyk_dblock__from_c_string("qword");
        }
        tactyk_dblock__put(ctx->local_vars, "$KW",  kw);
        tactyk_dblock__put(ctx->local_vars, "$VALUE",  ctx->pl_operand_resolved);
        return true;
    }
    else {
        return false;
    }
}

// parse a float.
//  If success, use a 64-bit integer as an intermediate representation so it can be treated like any other 64-bit field (required for immediate scrambling).
bool tactyk_emit__FloatOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    union tactyk_util__float_int val;

    if (tactyk_dblock__try_parsedouble(&val.fval, ctx->pl_operand_resolved)) {
        ctx->pl_operand_resolved = tactyk_dblock__from_int(val.ival);
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("qword"));
        tactyk_dblock__put(ctx->local_vars, "$VALUE",  ctx->pl_operand_resolved);
        return true;
    }
    else {
        return false;
    }
}

bool tactyk_emit__StringOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    char buf[16];
    memset(buf, 0, 16);
    struct tactyk_dblock__DBlock *sb = ctx->pl_operand_raw;
    int64_t len = sb->length;
    if (len > 0) {
        uint8_t *data = (uint8_t*) sb->data;
        switch(data[0]) {
            case '\'': {
                break;
            }
            case '`': {
                sb = ctx->pl_operand_resolved;
                len = sb->length;
                data = (uint8_t*) sb->data;
                break;
            }
            case '"': {
                if (data[len-1] == '"') {
                    len -= 1;
                }
                break;
            }
            default: {
                // in theory, I'd like to return whatever random string is in the dblock, but that interferes with multi-purpose operators
                //      (and can hide errors)
                return false;
            }
        }
        len -= 1;   // starting from position 1
        if (len > 0) {
            if (len > 16) {
                len = 16;
            }
            memcpy(buf, &data[1], len);
        }
    }
    uint64_t v_lower = *((uint64_t*)&buf[0]);
    uint64_t v_upper = *((uint64_t*)&buf[8]);
    struct tactyk_dblock__DBlock *si = tactyk_dblock__from_uint(v_lower);
    tactyk_dblock__put(ctx->local_vars, "$VALUE", si);
    tactyk_dblock__put(ctx->local_vars, "$VALUE_UPPER", tactyk_dblock__from_uint(v_upper));
    if (len > 8) {
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("qword"));
    }
    else {
        tactyk_emit__comprehend_int_value(ctx, si, "$KW");
    }

    return true;
}

int64_t tactyk_dblock__table_find(struct tactyk_dblock__DBlock *table, struct tactyk_dblock__DBlock *key);

bool tactyk_emit__Symbol(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *tbl_name = data->token->next;
    struct tactyk_dblock__DBlock *tbl = tactyk_dblock__get(ctx->symbol_tables, tbl_name);
    if (tbl == NULL) {
        error("EMIT -- undefined symbol table", data);
    }
    else {
        struct tactyk_dblock__DBlock *entry = tactyk_dblock__get(tbl, ctx->pl_operand_resolved);
        if (entry == NULL) {
            return false;
        }
        tactyk_dblock__put(ctx->local_vars, "$VALUE", entry);
        tactyk_emit__comprehend_int_value(ctx, entry, "$KW");
    }
    return true;
}


bool tactyk_emit__ExecSelector(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *sub_data = data->child;
    while (sub_data != NULL) {
        tactyk_emit__sub_func matcher_func = tactyk_dblock__get(ctx->operator_table, sub_data->token);
        if (matcher_func == NULL) {
            error("EMIT select-operand -- Invalid matcher", sub_data);
        }
        else {
            if(matcher_func(ctx, sub_data)) {
                return true;
            }
        }
        sub_data = sub_data->next;
    }
    return false;
}

// The "flags" opreator is basically a select statement which runs all child operators, disregards all child outputs, and returns true always.
bool tactyk_emit__Flags(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = ctx->pl_operand_resolved;
    struct tactyk_dblock__DBlock *sub_data = data->child;
    bool any_passed = false;

    while (sub_data != NULL) {
        tactyk_emit__sub_func matcher_func = tactyk_dblock__get(ctx->operator_table, sub_data->token);

        if (matcher_func == NULL) {
            error("EMIT select-operand -- Invalid matcher", sub_data);
        }
        else {
            any_passed |= matcher_func(ctx, sub_data);
        }
        sub_data = sub_data->next;
    }

    if (!any_passed) {
        warn("EMIT -- no conditional statements accepted", data);
    }
    return true;
}
bool tactyk_emit__SelectOp(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = ctx->pl_operand_raw;
    if (ctx->select_token == NULL) {
        ctx->select_token = tactyk_dblock__from_c_string("[[ NULL ]]");
    }
    return tactyk_emit__ExecSelector(ctx, data);
}
bool tactyk_emit__SelectTemplate(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = ctx->code_template;
    #ifdef TACTYK_DEBUG
    printf("SELECT-TEMPLATE:  ");
    tactyk_dblock__println(ctx->select_token);
    #endif // TACTYK_DEBUG
    return tactyk_emit__ExecSelector(ctx, data);
}

bool tactyk_emit__SelectKeyword(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = tactyk_dblock__get(ctx->local_vars, "$KW");
    return tactyk_emit__ExecSelector(ctx, data);
}

// the scrambler's word-size template selection competes with other word-size template selectors,
//  so to resolve it, the scrambler and the subroutine it configures use the cleverly named alternative: "$KW2"
//      (at this point the selectors should probably just use a parameter to specify which the selection argument should be obtained from)
bool tactyk_emit__SelectKeyword2(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = tactyk_dblock__get(ctx->local_vars, "$KW2");
    return tactyk_emit__ExecSelector(ctx, data);
}
bool tactyk_emit__Contains(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *case_token = data->token->next;
    while (case_token != NULL) {
        if (tactyk_dblock__contains(ctx->select_token, case_token)) {
            tactyk_emit__ExecSubroutine(ctx, data);
            return true;
        }
        case_token = case_token->next;
    }
    return false;
}
bool tactyk_emit__Case(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *case_token = data->token->next;
    while (case_token != NULL) {
        if (tactyk_dblock__equals(case_token, ctx->select_token)) {
            tactyk_emit__ExecSubroutine(ctx, data);
            return true;
        }
        case_token = case_token->next;
    }
    return false;
}

bool tactyk_emit__Pick(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    tactyk_dblock__append(ctx->code_template, data->token->next);
    return true;
}

bool tactyk_emit__Operand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *alt = data->token->next;
    if (alt != NULL) {
        error("EMIT -- unimplemented feature (explicit data source specification for TACTYK-VISA operands)", data);
    }
    if (ctx->pl_operand_raw == NULL) {
        error("EMIT -- Not enough arguments", ctx->active_command->pl_code);
    }
    ctx->pl_operand_raw = ctx->pl_operand_raw->next;

    if (ctx->pl_operand_raw != NULL) {
        ctx->pl_operand_resolved = tactyk_emit__fetch_var(ctx, NULL, ctx->pl_operand_raw);
    }
    else {
        ctx->pl_operand_resolved = tactyk_dblock__from_c_string("[[ NULL ]]");
    }

    // Prevent temp vars from leaking.  In most cases, it doesn't matter, as a successfully applied typespec overwrite what is generally expected.
    // But, the overwrite is not guaranteed, and this is a somewhat more centralized place to handle it.
    // Except in special cases, usage of these variables outside of an operand subroutine is improper
    //  (the only such special case at present is using the scrambler to encode instruction indices)
    tactyk_dblock__delete(ctx->local_vars, "$KW");
    tactyk_dblock__delete(ctx->local_vars, "$VALUE");
    tactyk_dblock__delete(ctx->local_vars, "$VALUE_UPPER");

    if (!tactyk_emit__ExecSubroutine(ctx, data)) {
        return false;
    }

    //if (!tactyk_emit__ExecSubroutine(ctx, data)) {
    //    tactyk_dblock__print_structure_simple(data);
    //    error("EMIT -- failed to handle operand", data);
    //}
    return true;
}

bool tactyk_emit__OptionalOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    struct tactyk_dblock__DBlock *raw_operand = ctx->pl_operand_raw;

    // if the standard operand applicator fails, put the raw operand back so a non-optional operand can handle it.
    if (!tactyk_emit__Operand(ctx, vopcfg)) {
        ctx->pl_operand_raw = raw_operand;
    }
    return true;

}

// command interpreter which performs actions until either all tokens are consumed or until a preset limit is reached
//  and optionally can randomly permute generated code.
//
// The permutation option might seem oddly placed -- it is for assisting with binary executable randomization
//      (the products of the composite operation would otherwise yield predictable sequences in executable memory)
bool tactyk_emit__Composite(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    bool permute = false;
    bool remove_duplicates = false;
    uint64_t max_ops = 16;

    struct tactyk_dblock__DBlock *param = vopcfg->token;
    while (param != NULL) {
        if (!tactyk_dblock__try_parseuint(&max_ops, param)) {
            if (tactyk_dblock__equals_c_string(param, "permute-code")) {
                permute = true;
            }
            else if (tactyk_dblock__equals_c_string(param, "no-duplicates")) {
                remove_duplicates = true;
            }
        }
        param = param->next;
    }

    struct tactyk_dblock__DBlock *main_cb = ctx->active_command->asm_code;
    struct tactyk_dblock__DBlock **code_fragments = calloc(max_ops, sizeof(void*));
    uint64_t opcount = 0;
    struct tactyk_dblock__DBlock *cfrag = tactyk_dblock__new(4096);
    ctx->active_command->asm_code = cfrag;

    while ( (ctx->pl_operand_raw != NULL) || (opcount < max_ops) ) {

        if (!tactyk_emit__ExecSubroutine(ctx, vopcfg)) {
            if (ctx->pl_operand_raw == NULL) {
                break;
            }
            else {
                error("EMIT -- nonspecific error during composite command evalation", ctx->active_command->pl_code);
                return false;
            }
        }

        if (cfrag->length > 0) {
            code_fragments[opcount] = cfrag;
            cfrag = tactyk_dblock__new(4096);
            ctx->active_command->asm_code = cfrag;
            opcount += 1;
            tactyk_dblock__clear(ctx->code_template);
        }

        for (uint64_t i = 0; i < opcount; i++) {
            struct tactyk_dblock__DBlock *cfrag_a = code_fragments[i];
            for (uint64_t j = i+1; j < opcount; j++) {
                struct tactyk_dblock__DBlock *cfrag_b = code_fragments[j];
                if (tactyk_dblock__equals(cfrag_a, cfrag_b)) {
                    tactyk_dblock__clear(cfrag_a);
                    break;
                }
            }
        }
    }
    if (permute) {
        for (uint64_t i = 0; i < opcount; i++) {
            uint64_t randpos = tactyk_util__rand_range(opcount-i);
            cfrag = code_fragments[randpos];
            assert(cfrag != NULL);
            code_fragments[randpos] = code_fragments[opcount-i-1];
            if (cfrag->length > 0) {
                tactyk_dblock__append(main_cb, cfrag);
            }
        }
        ctx->active_command->asm_code = main_cb;
    }
    else {
        for (uint64_t i = 0; i < opcount; i++) {
            cfrag = code_fragments[i];
            tactyk_dblock__append(main_cb, cfrag);
        }
        ctx->active_command->asm_code = main_cb;
    }

    return true;
}

// Called by functions which write to $VALUE to ensure $KW is updated with an appropriate keyword (for integer handling that differ based on word size).
bool tactyk_emit__comprehend_int_value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data, char *kwname) {
    int64_t ival = 0;
    uint64_t uival = 0;

    if (data == NULL) {
        tactyk_dblock__delete(ctx->local_vars, kwname);
        return false;
    }

    if (tactyk_dblock__try_parseint(&ival, data)) {
        uival = (uint64_t)ival;
    }
    else if (!tactyk_dblock__try_parseuint(&uival, data)) {
        tactyk_dblock__delete(ctx->local_vars, kwname);
        return false;
    }

    if (uival <= 0xff) {
        tactyk_dblock__put(ctx->local_vars, kwname, tactyk_dblock__from_c_string("byte"));
    }
    else if (uival <= 0xffff) {
        tactyk_dblock__put(ctx->local_vars, kwname, tactyk_dblock__from_c_string("word"));
    }
    else if (uival <= 0xffffffff) {
        tactyk_dblock__put(ctx->local_vars, kwname, tactyk_dblock__from_c_string("dword"));
    }
    else {
        tactyk_dblock__put(ctx->local_vars, kwname, tactyk_dblock__from_c_string("qword"));
    }

    return true;
}

bool tactyk_emit__NullArg(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    char buf[16];
    tactyk_dblock__export_cstring(buf, 16, ctx->pl_operand_resolved);
    if (strncmp(buf, "[[ NULL ]]", 16) == 0) {
        tactyk_dblock__put(ctx->local_vars, "$VALUE",  ctx->pl_operand_resolved);
        return true;
    }
    else {
        return false;
    }
}

bool tactyk_emit__Value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    tactyk_emit__fetch_var(ctx, "$VALUE", data->token->next);
    tactyk_emit__comprehend_int_value(ctx, data->token->next, "$KW");
    return true;
}

bool tactyk_emit__Scramble(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *token = data->token->next;
    struct tactyk_dblock__DBlock *sc_dest_register = tactyk_emit__fetch_var(ctx, NULL, token);
    token = token->next;
    struct tactyk_dblock__DBlock *sc_code_name = token;
    token = token->next;
    struct tactyk_dblock__DBlock *sc_input;

    if (token == NULL) {
        sc_input = tactyk_dblock__from_c_string("$VALUE");
        struct tactyk_dblock__DBlock *sc_input_resolved = tactyk_emit__fetch_var(ctx, NULL, sc_input);
        tactyk_dblock__dispose(sc_input);
        sc_input = sc_input_resolved;
        tactyk_emit__comprehend_int_value(ctx, sc_input_resolved, "$KW2");
    }
    else {
        sc_input = tactyk_emit__fetch_var(ctx, NULL, token);
        struct tactyk_dblock__DBlock *sc_input_resolved = tactyk_emit__fetch_var(ctx, NULL, sc_input);
        sc_input = sc_input_resolved;
    //printf("SCRAMBLE:  ");
    //tactyk_dblock__println(sc_input_resolved);
        tactyk_emit__comprehend_int_value(ctx, sc_input_resolved, "$KW2");
    }
    int64_t raw_val;


    // if no integer input, cancel and output a dummy value as "de-scrambling" code.
    //      This looseness is a hack to allow operands to resolve to non-integer tokens without having to make the type specification subroutines
    //      aware either of other type specifiers or of the target variables that their outputs are to be stored in.
    //      If the selected assembly template does not reference the de-scrambling code, this should not break anything.
    //      If assembly template erroneously references it (or if the wrong values are used for makign the selection), then the dummy value
    //      becomes an obvious and obnoxious assembly-language comment.
    if (!tactyk_dblock__try_parseint(&raw_val, sc_input)) {
        struct tactyk_dblock__DBlock *error_indicator = tactyk_dblock__from_c_string("; ---- OMITTED DE-SCRAMBLE OP (no value) ---- ;");
        tactyk_dblock__put(ctx->local_vars, sc_code_name, error_indicator);
        return true;
    }
    tactyk_dblock__dispose(sc_input);

    uint64_t rand_val = tactyk__rand_uint64();
    uint64_t diff_val = (uint64_t)raw_val ^ rand_val;
    //bool is_qword = false;

    if (raw_val < 0) {
        //is_qword = true;
    }
    else if (raw_val <= 0xff) {
        rand_val &= 0xff;
        diff_val &= 0xff;
    }
    else if (raw_val <= 0xffff) {
        rand_val &= 0xffff;
        diff_val &= 0xffff;
    }
    else if (raw_val <= 0xffffffff) {
        rand_val &= 0xffffffff;
        diff_val &= 0xffffffff;
    }
    else {
        //is_qword = true;
    }

    struct tactyk_dblock__DBlock *sc_rand = tactyk_dblock__from_uint(rand_val);
    struct tactyk_dblock__DBlock *sc_diff = tactyk_dblock__from_uint(diff_val);
    tactyk_dblock__put(ctx->local_vars, "$SC_RAND", sc_rand);
    tactyk_dblock__put(ctx->local_vars, "$SC_DIFF", sc_diff);
    tactyk_dblock__put(ctx->local_vars, "$SC_DEST", sc_dest_register);

    tactyk_dblock__delete(ctx->local_vars, "$SC");
    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__get(ctx->subroutine_table, "scramble");
    if (sub->func(ctx, sub->vopcfg)) {
        struct tactyk_dblock__DBlock *code = tactyk_dblock__get(ctx->local_vars, "$SC");
        tactyk_dblock__put(ctx->local_vars, sc_code_name, code);
    }
    return true;
}

bool tactyk_emit__Code(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *target_name = data->token->next;
    struct tactyk_dblock__DBlock *code;

    if (target_name == NULL) {
        code = ctx->active_command->asm_code;
    }
    else {
        code = tactyk_dblock__get(ctx->local_vars, target_name);
        if (code == NULL) {
            code = tactyk_dblock__new(16);
            tactyk_dblock__put(ctx->local_vars, target_name, code);
        }
    }

    struct tactyk_dblock__DBlock *code_template = data->child;
    while (code_template != NULL) {
        struct tactyk_dblock__DBlock *code_rewritten = tactyk_emit__fetch_var(ctx, NULL, code_template);
        code_template = code_template->next;
        tactyk_dblock__append(code, code_rewritten);
        tactyk_dblock__append_char(code, '\n');
    }
    return true;
}

void tactyk_emit__sanitize_identifier(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *out, struct tactyk_dblock__DBlock* id) {
    char txt[8];
    uint8_t *id_chars = (uint8_t*)id->data;
    for (uint64_t i = 0; i < id->length; i += 1) {
        uint8_t c = id_chars[i];

        if (ctx->namechars[c] == true) {
            txt[0] = c;
            txt[1] = 0;
        }
        else {
            sprintf(txt, "~%u", c);
        }
        tactyk_dblock__append(out, txt);
    }
}

void tactyk_emit__add_script_label(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *raw_label) {
    struct tactyk_dblock__DBlock *sanitized_label = tactyk_dblock__from_c_string("lbl#");
    tactyk_emit__sanitize_identifier(ctx, sanitized_label, raw_label);

    tactyk_dblock__make_pseudosafe(sanitized_label);
    tactyk_dblock__put(ctx->label_table, raw_label, sanitized_label);

    struct tactyk_asmvm__identifier *id = tactyk_dblock__new_managedobject(ctx->program->functions, raw_label);
    id->value = ctx->script_commands->element_count;
    tactyk_dblock__export_cstring(id->txt, TACTYK__MAX_IDENTIFIER_LENGTH, raw_label);
    //strncpy(id->txt, tactyk_dblock__export_cstring(raw_label), MAX_IDENTIFIER_LENGTH);

    if (ctx->active_labels == NULL) {
        ctx->active_labels = sanitized_label;
        ctx->active_labels_last = sanitized_label;
    }
    else {
        ctx->active_labels_last->next = sanitized_label;
        ctx->active_labels_last = sanitized_label;
    }
}

void tactyk_emit__add_script_command(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *token, struct tactyk_dblock__DBlock *line) {
    struct tactyk_dblock__DBlock *name = token;
    struct tactyk_emit__script_command *cmd = tactyk_dblock__new_object(ctx->script_commands);
    cmd->name = name;
    cmd->tokens = token;
    cmd->pl_code = line;
    cmd->asm_code = tactyk_dblock__new(4096);
    cmd->labels = ctx->active_labels;

    ctx->active_labels = NULL;
    ctx->active_labels_last = NULL;
}

void tactyk_emit__compile(struct tactyk_emit__Context *ctx) {
    for (uint64_t i = 0; i < ctx->script_commands->element_count; i += 1) {
        struct tactyk_emit__script_command *cmd = tactyk_dblock__index(ctx->script_commands, i);
        #ifdef TACTYK_DEBUG
            printf("---------\n");
            printf("CMD #%ju: ", i);
            tactyk_dblock__println(cmd->tokens);
            printf("\n");
        #endif // TACTYK_DEBUG
        ctx->iptr = i;
        ctx->active_command = cmd;
        struct tactyk_dblock__DBlock *label = cmd->labels;
        while (label != NULL) {
            tactyk_dblock__append(cmd->asm_code, label);
            tactyk_dblock__append(cmd->asm_code, ":\n");
            label = label->next;
        }
        struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__get(ctx->instruction_table, cmd->name);

        sub->func(ctx, sub->vopcfg);

        #ifdef TACTYK_DEBUG
        {
            printf("COMMAND: ");
            tactyk_dblock__println(cmd->name);
            tactyk_dblock__println(cmd->asm_code);
            printf("\n");
        }
        #endif // TACTYK_DEBUG
    }

    uint64_t program_size = ctx->script_commands->element_count;
    uint64_t *program_map = tactyk_alloc__allocate(program_size, sizeof(uint64_t));
    for (uint64_t i = 0; i < program_size; i++) {
        program_map[i] = i;
    }
    uint64_t j;

    for (uint64_t i = 0; i < (program_size-1); i++) {
        uint64_t j = i + tactyk_util__rand_range(program_size-i);
        int32_t iv = program_map[i];
        int32_t jv = program_map[j];

        program_map[i] = jv;
        program_map[j] = iv;
    }

    char fname_assembly_code[64];
    char fname_object[64];
    char fname_symbols[64];

    sprintf(fname_assembly_code, "/tmp/tactyk_program__XXXXXX.asm");
    sprintf(fname_object, "/tmp/tactyk_object__XXXXXX.obj");
    sprintf(fname_symbols, "/tmp/tactyk_symbols__XXXXXX.map");

    int result_asm = mkstemps(fname_assembly_code, 4);
    int result_obj = mkstemps(fname_object, 4);
    int result_sym = mkstemps(fname_symbols, 4);

    //mkstemp

    //FILE *asm_file = fopen

    FILE *asm_file =  fopen( fname_assembly_code, "w" );
    fprintf(asm_file, "BITS 64\n");
    fprintf(asm_file, "[map symbols %s]\n", fname_symbols);

    fprintf(asm_file, "%%define random_const_FS %ju\n", ctx->random_const_fs);
    fprintf(asm_file, "%%define random_const_GS %ju\n", ctx->random_const_gs);

    fprintf(asm_file, "%s\n", ctx->asm_header);

    for (uint64_t i = 0; i < program_size; i++) {
        uint64_t j = program_map[i];
        struct tactyk_emit__script_command *cmd = tactyk_dblock__index(ctx->script_commands, j);
        //struct tactyk_emit__script_command *cmd = &ctx->script_commands[j];
        //printf("%s\n\n", cmd->asm_code);
        char *buf = calloc(cmd->asm_code->length+1, 1);
        tactyk_dblock__export_cstring(buf, cmd->asm_code->length+1, cmd->asm_code);
        fprintf(asm_file, "%s\n\n", buf);
        free(buf);
    }
    fclose(asm_file);

    struct tactyk_assembly *assembly = tactyk_assemble(fname_assembly_code, fname_object, fname_symbols);

    assembly->name = "Tactyk (should) Affix Captions To Your Kitten: Mix - o - Bits, The Very Technical";
    //printf("assembled %ju bytes, name=%s\n", assembly->length, assembly->name);

    //struct tactyk_dblock__DBlock jt_table = tactyk_dblock__new_table(1024);
    //ctx->program->jump_target_table = jt_table;

    uint64_t ex_size = tactyk_util__next_pow2(assembly->length);
    #ifdef USE_TACTYK_ALLOCATOR
    void *exec_target_address = tactyk__mk_random_base_address();
    void *exec_mem = mmap(exec_target_address, ex_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    #else
    void *exec_mem = mmap(NULL, ex_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    #endif // USE_TACTYK_ALLOCATOR

    memcpy(exec_mem, assembly->bin, assembly->length);
    mprotect(exec_mem, ex_size, PROT_READ | PROT_EXEC);

    ctx->program->executable = exec_mem;
    ctx->program->length = program_size;

    #ifdef USE_TACTYK_ALLOCATOR
    void* exec_map_target_address = tactyk__mk_random_base_address();
    #else
    void* exec_map_target_address = NULL;
    #endif // USE_TACTYK_ALLOCATOR

    uint64_t command_map_size = program_size * sizeof(void*);
    tactyk_asmvm__op * command_map = mmap(exec_map_target_address, command_map_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    ctx->program->command_map = command_map;

    // copy offsets from the symbol table to the assembly precursor abstraction (which are directly referenced by the precursor program abstraction)
    {
        uint64_t command_index = 0;

        struct tactyk_dblock__DBlock *labellist = assembly->labels->store;
        for (uint64_t i = 0; i < labellist->element_count; i++) {
            struct tactyk_asmvm__identifier *bin_symbol = tactyk_dblock__index(labellist, i);

            // if the symbol from the binary has a command prefix, and the affix is a valid integer, then add an entry to the command map
            //      (the program's global jump table)
            if (strncmp(bin_symbol->txt, TACTYK_EMIT__COMMAND_PREFIX, sizeof(TACTYK_EMIT__COMMAND_PREFIX)-1) == 0) {
                if (tactyk_util__try_parseuint(&command_index, &bin_symbol->txt[sizeof(TACTYK_EMIT__COMMAND_PREFIX)-1], false)) {
                    tactyk_asmvm__op cmd_ptr = (tactyk_asmvm__op) exec_mem + bin_symbol->value;
                    command_map[command_index] = cmd_ptr;
                }
            }

            // main entry point
            else if (strcmp(bin_symbol->txt, "run") == 0) {
                ctx->program->run = (tactyk_asmvm__runnable) exec_mem + bin_symbol->value;
            }
            else {
                //printf("OTHER SYMBOL: '%s'\n", bin_symbol->txt);
            }
        }
    }
    mprotect(command_map, command_map_size, PROT_READ );
}

