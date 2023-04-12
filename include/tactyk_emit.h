//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_EMIT__INCLUDE_GUARD
#define TACTYK_EMIT__INCLUDE_GUARD

//  This is a compiler for tactyk.  Once it can run the tests, it will replace for tactyk_emit (and assume its name).
//  until then, it will bear the odd and somewhat misleading name, "tactyk_emit_new".
//

#include <stdbool.h>
#include <stdint.h>

#include "tactyk_asmvm.h"
#include "tactyk_dblock.h"

#define TACTYK_EMIT_SYMBOL_METATABLE_CAPACITY 8
#define TACTYK_EMIT_GLOBALVARS_CAPACITY 16
#define TACTYK_EMIT_LOCALVARS_CAPACITY 256
#define TACTYK_EMIT_SPECIALS_CAPACITY 512
#define TACTYK_EMIT_OPERATOR_CAPACITY 512
#define TACTYK_EMIT_TYPESPEC_CAPACITY 512
#define TACTYK_EMIT_INSTRUCTION_CAPACITY 1024
#define TACTYK_EMIT_SUB_CAPACITY 2048
#define TACTYK_EMIT__SUBROUTINE_CAPACITY (TACTYK_EMIT_INSTRUCTION_CAPACITY + TACTYK_EMIT_TYPESPEC_CAPACITY + TACTYK_EMIT_SUB_CAPACITY)

#define TACTYK_EMIT_MAINTEXTBUF_CAPACITY 1<<20
#define TACTYK_EMIT__CODEBUF_INITCAPACITY 4096
#define TACTYK_EMIT__CODEBUF_BUFLIST_CAPACITY 32

#define TACTYK_EMIT_CONSTANTS_CAPACITY 4096
#define TACTYK_EMIT_LABELS_CAPACITY 4096
#define TACTYK_EMIT_APIFUNCS_CAPACITY 4096
#define TACTYK_EMIT_STRUCTS_CAPACITY 4096
#define TACTYK_EMIT_MEMBLOCKS_CAPACITY 4096

#define TACTYK_EMIT_SCRIPT_COMMAND_CAPACITY 65536
#define TACTYK_EMIT_SCRIPT_COMMAND_OPERAND_CAPACITY 16
#define TACTYK_EMIT_SCRIPT_COMMAND_TOKEN_MAXSIZE 63
#define TACTYK_EMIT_SCRIPT_COMMAND_LINE_SIZE (TACTYK_EMIT_SCRIPT_COMMAND_OPERAND_CAPACITY * (TACTYK_EMIT_SCRIPT_COMMAND_OPERAND_CAPACITY + 1))
#define TACTYK_EMIT_SCRIPT_BRANCHTARGET_CAPACITY 1024

#define TACTYK_EMIT_ASMCHUNK_MAXSIZE 4096

//struct tactyk_structured_text;
struct tactyk_emit__Context;

struct tactyk_emit__script_command {
    struct tactyk_dblock__DBlock *name;
    struct tactyk_dblock__DBlock *tokens;
    struct tactyk_dblock__DBlock *pl_code;

    struct tactyk_dblock__DBlock *labels;

    struct tactyk_dblock__DBlock *asm_code;

    int32_t linenumber;
};

void tactyk_emit__add_script_label(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock* label);
void tactyk_emit__add_script_command(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *token, struct tactyk_dblock__DBlock *line);
void tactyk_emit__compile(struct tactyk_emit__Context *ctx);
//#define TACTYK_EMIT_COMMAND_FRAGMENT_CAPACITY 16

typedef char* (*tactyk_emit__tolabel)(char *out, char *val, int32_t max_length);
typedef bool (*tactyk_emit__sub_func)(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *data);
typedef void (*tactyk_emit__error_handler)(char *message);


typedef void (tactyk_emit__tactyk_api_function)(struct tactyk_asmvm__Context *context);

// (probably will hard-code arguments handling)
//typedef bool (*tactyk_emit__sub_func)(struct tactyk_emit_sub *sub_ctx, char *args_class, char **args, int32_t args_count);

// structured text with an invokable function bolted on.
struct tactyk_emit__subroutine_spec {
    struct tactyk_dblock__DBlock *vopcfg;
    tactyk_emit__sub_func func;
};

struct tactyk_emit__Context {

    char *visa_file_prefix;

    char *asm_header;

    tactyk_emit__tolabel sanitize_label;

    struct tactyk_dblock__DBlock *operator_table;

    struct tactyk_dblock__DBlock *instruction_table;
    struct tactyk_dblock__DBlock *subroutine_table;
    struct tactyk_dblock__DBlock *typespec_table;

    struct tactyk_asmvm__Program *program;

    struct tactyk_dblock__DBlock *local_vars;
    struct tactyk_dblock__DBlock *global_vars;

    struct tactyk_dblock__DBlock *symbol_tables;
    struct tactyk_dblock__DBlock *const_table;
    struct tactyk_dblock__DBlock *fconst_table;
    struct tactyk_dblock__DBlock *memblock_table;
    struct tactyk_dblock__DBlock *label_table;
    struct tactyk_dblock__DBlock *api_table;
    struct tactyk_dblock__DBlock *c_api_table;

    struct tactyk_dblock__DBlock *visa_token_constants;
    struct tactyk_dblock__DBlock **visa_token_invmap;

    struct tactyk_dblock__DBlock *code_template;

    struct tactyk_dblock__DBlock *script_commands;

    struct tactyk_emit__script_command *active_command;
    uint64_t active_command_arg_index;

    struct tactyk_dblock__DBlock *active_labels;
    struct tactyk_dblock__DBlock *active_labels_last;

    bool valid_parse_result;
    struct tactyk_dblock__DBlock *pl_operand_raw;
    struct tactyk_dblock__DBlock *pl_operand_resolved;

    tactyk_emit__error_handler error_handler;

    struct tactyk_dblock__DBlock *select_token;
    char *scramble_dest;
    char *scramble_src;

    bool namechars[256];
    uint64_t iptr;

    uint64_t token_handle_count;
    bool has_visa_constants;
};

struct tactyk_emit__Context* tactyk_emit__init();
void tactyk_emit__reset(struct tactyk_emit__Context *emitctx);
void tactyk_emit__dispose(struct tactyk_emit__Context *ctx);
void tactyk_emit__error(struct tactyk_emit__Context *ctx, void *msg);

void tactyk_emit__add_tactyk_apifunc(struct tactyk_emit__Context *ctx, char *name, tactyk_emit__tactyk_api_function func);

//  Make a c-function directly invokable from tactyk
//
//  This is potentially dangerous and should be restricted to only functions which accept primitive arguments (integers/char/bool -- maybe floating points,b ut support for
//  it has not been implemented yet).
//
//  If a function accepts pointers, the pointers that get passed are whatever a tactyk program wants (which almost certainly leads to type confusion)
//
//  It is preferable that all interfacing code (anything which is invokable by tactyk) should be able to extract arguments either directly from a tactyk_asmvm__Context
//  or from the memory blocks attached to a tactyk_asmvm__Context.
//
//  This could be made to be appropriate if type constraints where available (C doesn't seem to do that, and I am unaware of any optional warnings that could be enabled)
void tactyk_emit__add_c_apifunc(struct tactyk_emit__Context *ctx, char *name, void *func);

void tactyk_emit__init_program(struct tactyk_emit__Context *ctx);
//void tactyk_emit_new__init(struct tactyk_emit_new__Context *ctx);

bool tactyk_emit__ExecSubroutine(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__ExecInstruction(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);


// TACTYK VISA basic functionality
//  (obviously I still need to move them either to tactyk_visa.c or create tactyk_visa_supplemental.c and put them there)
//
// Initailly when starting the redesign of emit, these functions were explicitly coded in config file, which would have
// greatly reduced the amount of C code, but would only have the effect of moving complexity into a configuration.
//
// These functions are bound to names and invoked dynamically to transform tactyk_pl code to Assembly Language.
// Generally each function performs a bunch various checks and manipualtions, sets a preset variable, then invokes
// any subordinate function.  Combined with a structured text object, these represent subroutines.
// The top-level subroutine (called an "instruction") also manages the acquisisition of tokens from tactyk_pl code at the beginning,
//  and exports the generated code at the end.
bool tactyk_emit__Apply(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__Int(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__Int_low32(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__Int_high32(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Symbol(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__SelectOp(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__SelectTemplate(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__SelectKeyword(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Flags(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Case(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Contains(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__IntRange(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Match(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__True(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__False(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Pick(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__Assign(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Operand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__OptionalOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Type(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Code(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Args(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Value(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__IntOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__FloatOperand(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__NullArg(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);

bool tactyk_emit__Scramble(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__DoSub(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__CheckedLabel(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
bool tactyk_emit__Exit(struct tactyk_emit__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);
//bool tactyk_emit__Subroutine(struct tactyk_emit_new__Context *ctx, struct tactyk_dblock__DBlock *vopcfg);


#endif //TACTYK_EMIT__INCLUDE_GUARD
