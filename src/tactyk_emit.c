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

#include <sys/mman.h>

#include "tactyk_emit.h"
#include "tactyk_util.h"
#include "tactyk_asmvm.h"
#include "tactyk.h"

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

// buffer for strings returned by the fetch_value function.
// The extra char at the end is for a guaranteed null terminator.
// The buffer is not expected to be filled.
char emit_txt_buf[FETCH_VALUE_CAPACITY+1];

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
    struct tactyk_emit__Context *ctx = calloc(1, sizeof(struct tactyk_emit__Context));

    ctx->visa_file_prefix = "";

    ctx->api_table = tactyk_dblock__new_table(64);
    ctx->c_api_table = tactyk_dblock__new_table(64);
    ctx->global_vars = tactyk_dblock__new_table(256);
    ctx->local_vars = tactyk_dblock__new_table(256);
    ctx->operator_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->typespec_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->instruction_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    ctx->subroutine_table = tactyk_dblock__new_managedobject_table(256, sizeof(struct tactyk_emit__subroutine_spec));
    //ctx->special_table = tactyk_dblock__new_managedobject_table(64);
    //ctx->definition_table = tactyk_dblock__new_table(64);

    ctx->script_commands = tactyk_dblock__new_container(8, sizeof(struct tactyk_emit__script_command));

    emit_txt_buf[FETCH_VALUE_CAPACITY] = 0;      // ensure it has a null terminator

    //tactyk_textbuf__init(&ctx->tbuf, TACTYK_EMIT_MAINTEXTBUF_CAPACITY, false);

    ctx->codebuf_index = 0;
    //tactyk_table__init(&ctx->codebuf_table, TACTYK_EMIT__CODEBUF_BUFLIST_CAPACITY);
    //tactyk_textbuf__init(&ctx->main_codebuf, TACTYK_EMIT__CODEBUF_INITCAPACITY, true);

    tactyk_dblock__put(ctx->operator_table, "symbol", tactyk_emit__Symbol);

    tactyk_dblock__put(ctx->operator_table, "select-operand", tactyk_emit__SelectOp);
    tactyk_dblock__put(ctx->operator_table, "select-template", tactyk_emit__SelectTemplate);
    tactyk_dblock__put(ctx->operator_table, "select-kw", tactyk_emit__SelectKeyword);
    tactyk_dblock__put(ctx->operator_table, "flags", tactyk_emit__Flags);

    tactyk_dblock__put(ctx->operator_table, "case", tactyk_emit__Case);
    tactyk_dblock__put(ctx->operator_table, "contains", tactyk_emit__Contains);

    tactyk_dblock__put(ctx->operator_table, "pick", tactyk_emit__Pick);
    tactyk_dblock__put(ctx->operator_table, "operand", tactyk_emit__Operand);
    tactyk_dblock__put(ctx->operator_table, "type", tactyk_emit__Type);
    tactyk_dblock__put(ctx->operator_table, "value", tactyk_emit__Value);

    tactyk_dblock__put(ctx->operator_table, "scramble", tactyk_emit__Scramble);
    tactyk_dblock__put(ctx->operator_table, "code", tactyk_emit__Code);

    tactyk_dblock__put(ctx->operator_table, "int-operand", tactyk_emit__IntOperand);

    tactyk_dblock__put(ctx->operator_table, "sub", tactyk_emit__DoSub);
    tactyk_dblock__put(ctx->operator_table, "label", tactyk_emit__CheckedLabel);
    tactyk_dblock__put(ctx->operator_table, "nullarg", tactyk_emit__NullArg);

    ctx->active_labels = NULL;
    ctx->active_labels_last = NULL;

    ctx->code_template = tactyk_dblock__new(16);
    ctx->temp = tactyk_dblock__new(1);
    ctx->temp_last = ctx->temp;

    error_handler = tactyk_emit__default_error_handler;

    if (setjmp(err_jbuf)) {
        error_handler(error_message);
        exit(1);
    }

    return ctx;
}

// release all memory belonging to an emit context.
void tactyk_emit__dispose(struct tactyk_emit__Context *ctx) {
    tactyk_dblock__dispose(ctx->global_vars);
    tactyk_dblock__dispose(ctx->local_vars);
    tactyk_dblock__dispose(ctx->operator_table);
    tactyk_dblock__dispose(ctx->typespec_table);
    tactyk_dblock__dispose(ctx->instruction_table);
    tactyk_dblock__dispose(ctx->subroutine_table);
    tactyk_dblock__dispose(ctx->codebuf_table);
    tactyk_dblock__dispose(ctx->active_labels);

    //tactyk_dblock__dispose(ctx->special_table);

    //tactyk_textbuf__dispose(&ctx->tbuf);
    //tactyk_textbuf__dispose(&ctx->instruction_prototype_label);
    //tactyk_textbuf__dispose(&ctx->command_label);
    tactyk_dblock__dispose(ctx->code_template);
    tactyk_dblock__dispose(ctx->main_codebuf);

    for (int32_t i = 0; i < ctx->codebuf_index; i++) {
        tactyk_dblock__dispose(ctx->codebuf_list[i]);
    }

    free(ctx);
}

bool tactyk_emit__comprehend_int_value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data);

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

    struct tactyk_dblock__DBlock *next_instruction_label = tactyk_dblock__from_c_string(TACTYK_EMIT__COMMAND_PREFIX);
    tactyk_dblock__append(next_instruction_label, cmd_idx_next);
    tactyk_dblock__put(ctx->local_vars, "$NEXT_INSTRUCTION", next_instruction_label);

    bool result = tactyk_emit__ExecSubroutine(ctx, data);
    // clean up any temp dblocks (mostly dynamically text chunks)
    tactyk_dblock__dispose(ctx->temp->next);
    ctx->temp->next = NULL;
    ctx->temp_last = ctx->temp;

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
    ctx->memblock_table = tactyk_dblock__new_table(64);

    ctx->program->functions = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_asmvm__identifier));

    tactyk_dblock__put(ctx->symbol_tables, "label", ctx->label_table);
    tactyk_dblock__put(ctx->symbol_tables, "const", ctx->const_table);
    tactyk_dblock__put(ctx->symbol_tables, "mem", ctx->memblock_table);
    tactyk_dblock__put(ctx->symbol_tables, "capi", ctx->c_api_table);
    tactyk_dblock__put(ctx->symbol_tables, "tapi", ctx->api_table);
}

bool tactyk_emit__Type(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {

    struct tactyk_dblock__DBlock *token = data->token->next;
    while (token != NULL) {
        struct tactyk_emit__subroutine_spec *type_applicator = tactyk_dblock__get(ctx->typespec_table, token);
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
        tactyk_emit__comprehend_int_value(ctx, entry);
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
    //printf("seelect-operand: ");
   // tactyk_dblock__println(ctx->pl_operand_raw);
    return tactyk_emit__ExecSelector(ctx, data);
}
bool tactyk_emit__SelectTemplate(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = ctx->code_template;
    return tactyk_emit__ExecSelector(ctx, data);
}

bool tactyk_emit__SelectKeyword(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    ctx->select_token = tactyk_dblock__get(ctx->local_vars, "$KW");
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
    /*
    if (ctx->pl_operand_raw == NULL) {
        error("EMIT -- Not enough arguments", ctx->active_command->pl_code);
    }

    ctx->pl_operand_resolved = tactyk_emit__fetch_var(ctx, NULL, ctx->pl_operand_raw);
    */

    if (ctx->pl_operand_raw != NULL) {
        ctx->pl_operand_resolved = tactyk_emit__fetch_var(ctx, NULL, ctx->pl_operand_raw);
    }
    else {
        ctx->pl_operand_resolved = tactyk_dblock__from_c_string("[[ NULL ]]");
    }

    if (!tactyk_emit__ExecSubroutine(ctx, data)) {
        tactyk_dblock__print_structure_simple(data);
        error("EMIT -- failed to handle operand", data);
    }
    return true;
}

// Called by functions which write to $VALUE to ensure $KW is updated with an appropriate keyword (for integer handling that differ based on word size).
bool tactyk_emit__comprehend_int_value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    int64_t ival = 0;
    uint64_t uival = 0;

    if (data == NULL) {
        tactyk_dblock__delete(ctx->local_vars, "$KW");
        return false;
    }

    if (tactyk_dblock__try_parseint(&ival, data)) {
        uival = (uint64_t)ival;
    }
    else if (!tactyk_dblock__try_parseuint(&uival, data)) {
        tactyk_dblock__delete(ctx->local_vars, "$KW");
        return false;
    }

    if (uival <= 0xff) {
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("byte"));
    }
    else if (uival <= 0xffff) {
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("word"));
    }
    else if (uival <= 0xffffffff) {
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("dword"));
    }
    else {
        tactyk_dblock__put(ctx->local_vars, "$KW", tactyk_dblock__from_c_string("qword"));
    }

    return true;
}

bool tactyk_emit__NullArg(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg) {
    char buf[16];
    tactyk_dblock__export_cstring(buf, 16, ctx->pl_operand_resolved);
    if (strncmp(buf, "[[ NULL ]]", 16) == 0) {
        return true;
    }
    else {
        return false;
    }
}

bool tactyk_emit__Value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    //struct tactyk_dblock__DBlock *varvalue = tactyk_emit__fetch_var(ctx, "$VALUE", data->token->next);
    tactyk_emit__comprehend_int_value(ctx, data->token->next);
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
    }
    else {
        sc_input = tactyk_emit__fetch_var(ctx, NULL, token);
    }
    int64_t raw_val;

    // if no integer input, cancel and output a dummy value as "de-scrambling" code.
    //      This looseness is a hack to allow operands to resolve to non-integer tokens without having to make the type specification subroutines
    //      aware either of other type specifiers or of the target variables that their outputs are to be stored in.
    //      If the selected assembly template does not reference the de-scrambling code, this should not break anything.
    //      If assembly template erroneously references it (or if the wrong values are used for makign the selection), then the dummy value
    //      becomes an obvious and obnoxious assembly-language comment.
    if (!tactyk_dblock__try_parseint(&raw_val, sc_input)) {
        struct tactyk_dblock__DBlock *error_indicator = tactyk_dblock__from_c_string(";--------  ERROR:  Invalid scramble parameter! ");
        tactyk_dblock__put(ctx->local_vars, sc_code_name, error_indicator);
        return true;
    }
    tactyk_dblock__dispose(sc_input);

    uint64_t rand_val;
    ctx->rand(&rand_val, 8);
    uint64_t diff_val = (uint64_t)raw_val ^ rand_val;

    struct tactyk_dblock__DBlock *kw = tactyk_dblock__get(ctx->local_vars, "$KW");


    bool is_qword = false;

    if (raw_val < 0) {
        is_qword = true;
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
        is_qword = true;
    }

    if (is_qword) {
        uint64_t rand_high = rand_val >> 32;
        uint64_t rand_low = rand_val & 0xffffffff;
        uint64_t diff_high = diff_val >> 32;
        uint64_t diff_low = diff_val & 0xffffffff;

        struct tactyk_dblock__DBlock *sc_rand_high = tactyk_dblock__from_uint(rand_high);
        struct tactyk_dblock__DBlock *sc_diff_high = tactyk_dblock__from_uint(diff_high);
        struct tactyk_dblock__DBlock *sc_rand_low = tactyk_dblock__from_uint(rand_low);
        struct tactyk_dblock__DBlock *sc_diff_low = tactyk_dblock__from_uint(diff_low);
        tactyk_dblock__put(ctx->local_vars, "$SC_RAND_HIGH", sc_rand_high);
        tactyk_dblock__put(ctx->local_vars, "$SC_DIFF_HIGH", sc_diff_high);
        tactyk_dblock__put(ctx->local_vars, "$SC_RAND_LOW", sc_rand_low);
        tactyk_dblock__put(ctx->local_vars, "$SC_DIFF_LOW", sc_diff_low);
        tactyk_dblock__put(ctx->local_vars, "$SC_DEST", sc_dest_register);
    }
    else {
        struct tactyk_dblock__DBlock *sc_rand = tactyk_dblock__from_uint(rand_val);
        struct tactyk_dblock__DBlock *sc_diff = tactyk_dblock__from_uint(diff_val);
        tactyk_dblock__put(ctx->local_vars, "$SC_RAND", sc_rand);
        tactyk_dblock__put(ctx->local_vars, "$SC_DIFF", sc_diff);
        tactyk_dblock__put(ctx->local_vars, "$SC_DEST", sc_dest_register);
    }

    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__get(ctx->subroutine_table, "scramble");
    bool result = sub->func(ctx, sub->vopcfg);

    struct tactyk_dblock__DBlock *code = tactyk_dblock__get(ctx->local_vars, "$SC");
    tactyk_dblock__put(ctx->local_vars, sc_code_name, code);
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


bool tactyk_emit__CheckedLabel(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data) {
    struct tactyk_dblock__DBlock *lbl = tactyk_dblock__get(ctx->label_table, ctx->pl_operand_resolved);
    tactyk_dblock__put(ctx->local_vars, "$VALUE", lbl);
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
    tactyk_dblock__export_cstring(id->txt, MAX_IDENTIFIER_LENGTH, raw_label);
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
    struct tactyk_emit__subroutine_spec *sub = tactyk_dblock__get(ctx->subroutine_table, token);
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
        //printf("CMD #%ju: ", i);
        //tactyk_dblock__println(cmd->name);
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

        /*{
            printf("COMMAND: ");
            tactyk_dblock__println(cmd->name);
            tactyk_dblock__println(cmd->asm_code);
            printf("---\n");
        }*/

    }

    uint64_t program_size = ctx->script_commands->element_count;
    uint64_t *program_map = calloc(program_size, sizeof(uint64_t));
    for (uint64_t i = 0; i < program_size; i++) {
        program_map[i] = i;
    }
    uint64_t j;

    for (uint64_t i = 0; i < (program_size-1); i++) {

        //Pick a random integer between the value of i and the length of the program
        //This must be uniformly random and taken from a secure pseudo-random number generator.
        // the secure PRNG is /dev/urandom
        // the uniformity is accomplished by rounding the maximum uint64 down to an integer divisible by len(program)-i,
        // then discarding any excessively high random values
        //      (if accepted, these high values would slightly bias the result of binary executable randomization randomization)
        // NOTE:  This particular method of selecting secure random numbers should not be considered secure, as it also introduces potentially
        //        observable effects on system timing (I think that this is a non-issue, but that assessment is best deferred to actual experts)
        //        Additionally, division *slow* and the shuffling loop is not vectorizable due to random read-write memory access (the division
        //        operation is independent of all non-deterministic aspects of the loop, so it maybe will be optimized a bit).
        uint64_t sz = program_size - i;
        uint64_t max = 0xffffffffffffffff / sz;
        max *= sz;
        do {
            //getrandom(&j, 8,0);
            ctx->rand(&j, 8);
        } while (j > max);
        j %= sz;
        j = j + i;

        int32_t iv = program_map[i];
        int32_t jv = program_map[j];

        program_map[i] = jv;
        program_map[j] = iv;
    }

    #define ASM_PROGRAM_FILENAME "/tmp/tactyk_input.asm"
    #define ASM_OBJECT_FILENAME "/tmp/tactyk_output.o"
    #define ASM_SYMBOLS_FILENAME "/tmp/tactyk_symbols.map"


    //FILE *asm_file = fopen

    FILE *asm_file =  fopen( ASM_PROGRAM_FILENAME, "w" );
    fprintf(asm_file, "BITS 64\n");
    fprintf(asm_file, "[map symbols %s]\n", ASM_SYMBOLS_FILENAME);
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

    struct tactyk_assembly *assembly = tactyk_assemble(ASM_PROGRAM_FILENAME, ASM_OBJECT_FILENAME, ASM_SYMBOLS_FILENAME);

    assembly->name = "Tactyk (should) Affix Captions To Your Kitten: Mix - o - Bits, The Very Technical";
    //printf("assembled %ju bytes, name=%s\n", assembly->length, assembly->name);

    //struct tactyk_dblock__DBlock jt_table = tactyk_dblock__new_table(1024);
    //ctx->program->jump_target_table = jt_table;

    uint64_t ex_size = tactyk_util__next_pow2(assembly->length);

    void *exec_mem = mmap(NULL, ex_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    memcpy(exec_mem, assembly->bin, assembly->length);
    mprotect(exec_mem, ex_size, PROT_READ | PROT_EXEC);

    ctx->program->executable = exec_mem;
    ctx->program->length = program_size;

    // I do not think it is worthwhile to adapt a dblock container to handle this specific case.
    //      (but a dblock table should work nicely for allowing the host application to use a script's branch targets)
    tactyk_asmvm__op *command_map = calloc(program_size, sizeof(void*));
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
}

