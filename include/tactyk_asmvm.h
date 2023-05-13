//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_ASMVM__INCLUDE_GUARD
#define TACTYK_ASMVM__INCLUDE_GUARD

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tactyk.h"

#include "tactyk_dblock.h"

#define TACTYK_ASMVM__MEMBLOCK_CAPACITY 256
#define TACTYK_ASMVM__VM_STACK_SIZE 1024
#define TACTYK_ASMVM__PROGRAM_CAPACITY 256
#define TACTYK_ASMVM__MCTX_STACK_SIZE 65536
#define TACTYK_ASMVM__MCTX_ENTRY_SIZE 64
#define TACTYK_ASMVM__LWCALL_STACK_SIZE 65536
#define TACTYK_ASMVM__PROGRAM_CAPACITY 256


// memblock type hints
// These are only the automatically assigned "type" specifications, which TACTYK-PL uses to indicate how a memblockw as specified
// These are not to be construed as absolute rules
// If you need a custom type declaration for a memblock, pick a unique integer, assign it to the memblock type fields, and use it
//  as you see fit.
#define TACTYK_ASMVM__MEMBLOCK_TYPE__UNKNOWN 0
#define TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC 1
#define TACTYK_ASMVM__MEMBLOCK_TYPE__ALLOC 2
#define TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL 3

struct tactyk_asmvm__Context;
typedef uint64_t (*tactyk_asmvm__runnable)(struct tactyk_asmvm__Context *ctx);
typedef void (*tactyk_asmvm__show_indicator)(uint64_t iptr, char *msg);

// Represents branch targtes within compiled tactyk programs.
//  These are not directly callable (tactyk uses its own calling conventions internally), but act like functions.
//      actual calls into tactyk are accomplished by passing an index (which is used to access these from a jump table)
// Should probably consider just switching to void*
//
typedef void (*tactyk_asmvm__op)();


extern const int32_t STATIC_MEMORY_HEADER_LENGTH;
extern const int32_t STATIC_MEMORY_LENGTH;

// 128-bit data type to represent an xmm register.
// Future considerations:  it might be worthwhile to consider respecting the avx extentions, if there is a sensible way to do it without
//                         breaking compatibility with older devices.
union tactyk_asmvm__reg128 {
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    int64_t i64[2];
    int32_t i32[4];
    int16_t i16[8];
    double f64[2];
    float f32[4];
};

struct tactyk_asmvm__register_bank {
    uint64_t rTEMPS;
    uint64_t rLWCSI;
    uint64_t rMCSI;
    uint64_t rTEMPA;
    uint64_t rTEMPC;
    uint64_t rTEMPD;
    uint64_t *rADDR1;
    uint64_t *rADDR2;
    uint64_t *rADDR3;
    uint64_t *rADDR4;
    uint64_t rA;
    uint64_t rB;
    uint64_t rC;
    uint64_t rD;
    uint64_t rE;
    uint64_t rF;

    union tactyk_asmvm__reg128 xA;
    union tactyk_asmvm__reg128 xB;
    union tactyk_asmvm__reg128 xC;
    union tactyk_asmvm__reg128 xD;

    union tactyk_asmvm__reg128 xE;
    union tactyk_asmvm__reg128 xF;
    union tactyk_asmvm__reg128 xG;
    union tactyk_asmvm__reg128 xH;

    union tactyk_asmvm__reg128 xI;
    union tactyk_asmvm__reg128 xJ;
    union tactyk_asmvm__reg128 xK;
    union tactyk_asmvm__reg128 xL;

    union tactyk_asmvm__reg128 xM;
    union tactyk_asmvm__reg128 xN;
    union tactyk_asmvm__reg128 xTEMPA;
    union tactyk_asmvm__reg128 xTEMPB;

    // maybe should consider also includ a set of "long double" entries to represent the x87 fpu
    //      But for now, there is no aspect of tactyk which itneracts with it, and sse2 was selected for floating point math
    //      (supposedly the x87 fpu is better for high-precision math)
};

// low-level function signature
// This is used to build function calls (and prevent tactyk programs from passing or receiving undeclared parameters)
struct tactyk_asmvm__c_function_spec {
    void *fptr;
    uint64_t intarg_count;
    uint64_t floatarg_count;
    uint64_t intret_count;
    uint64_t floatret_count;
};


// memory layout specification used within the virtual machine
//      (basically just a pointer to the allocated memory, boundaries, and a pair of extra properties for implementing queues)
//          (details about properties are provided to the emit itnerface as a set of named constants)
struct tactyk_asmvm__memblock_lowlevel {
    uint8_t* base_address;
    uint32_t element_bound;
    uint32_t array_bound;
    uint32_t memblock_index;
    uint32_t offset;
};
// memory layout specification used outside of the virtual machine
//      (a pointer to the allocated memory plus information about each property)
struct tactyk_asmvm__memblock_highlevel {
    struct tactyk_asmvm__struct *definition;
    struct tactyk_asmvm__memblock_lowlevel *memblock;
    uint64_t memblock_id;       // possibly not needed.
    uint64_t num_entries;       // number of [high-level] elements in the memblock.  (Element size is given by definition->byte_stride)
    uint32_t type;
    uint8_t *data;              // binary data.
};

struct tactyk_asmvm__vm_stack_entry {
    void *source_command_map;
    struct tactyk_asmvm__memblock_lowlevel *source_memblocks;
    uint64_t source_memblock_count;
    uint32_t source_return_index;
    uint32_t source_max_iptr;
    uint32_t source_lwcallstack_floor;
    uint32_t source_mctxstack_floor;
    uint32_t source_lwcallstack_position;
    uint32_t source_mctxstack_position;
    void *dest_command_map;
    struct tactyk_asmvm__memblock_lowlevel *dest_memblocks;
    uint64_t dest_memblock_count;
    void *dest_function_map;
    uint32_t dest_jump_index;
    uint32_t dest_max_iptr;
};

struct tactyk_asmvm__Stack {
    int64_t stack_position;
    uint64_t stack_lock;
    struct tactyk_asmvm__vm_stack_entry entries[TACTYK_ASMVM__VM_STACK_SIZE];
};

struct tactyk_asmvm__program_declaration {
    uint64_t instruction_count;
    tactyk_asmvm__op *instruction_jumptable;
    uint64_t function_count;
    tactyk_asmvm__op *function_jumptable;
    struct tactyk_asmvm__memblock_lowlevel *memblocks;
    uint64_t memblock_count;
};

struct tactyk_asmvm__VM {
    uint64_t program_count;
    struct tactyk_asmvm__program_declaration *program_list;
};

struct tactyk_asmvm__Program;

struct tactyk_asmvm__MicrocontextStash {
    union tactyk_asmvm__reg128 a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, s26, s27, s28, s29, s30, s31;
};

// would prefer an explicit struct memory layout here, since this represents a low-level data structure
struct tactyk_asmvm__Context {

    uint64_t instruction_count;
    struct tactyk_asmvm__Context *subcontext;

    // program memory
    //  (or whatever memory the host/runtime assigns to programs)
    struct tactyk_asmvm__memblock_lowlevel *memblocks;
    uint64_t memblock_count;

    // dword #4
    struct tactyk_asmvm__memblock_lowlevel active_memblocks[4];

    // dword #16
    uint32_t *lwcall_stack;
    struct tactyk_asmvm__MicrocontextStash *microcontext_stack;
    uint64_t microcontext_stack_offset;
    uint32_t lwcall_stack_floor;
    uint32_t mctx_stack_floor;

    // dword #20
    struct tactyk_asmvm__VM *vm;
    struct tactyk_asmvm__Stack *stack;
    tactyk_asmvm__op *program_map;
    struct tactyk_asmvm__Program *hl_program_ref;      // a pointer to help high-level code access representative data structures.

    uint64_t instruction_index;     //tactyk function to call into.

    // execution state or error code
    uint64_t STATUS;

    uint64_t signature;
    uint64_t extra;         // register spills

    // dword #28
    struct tactyk_asmvm__register_bank reg;                     // tactyk context register content
    struct tactyk_asmvm__register_bank runtime_registers;       // native context register content

    uint64_t diagnostic_data[1024];
};
void tactyk_asmvm__print_context(struct tactyk_asmvm__Context *context);
void tactyk_asmvm__print_diagnostic_data(struct tactyk_asmvm__Context *context, int64_t amount);

struct tactyk_asmvm__property {
    char name[TACTYK__MAX_IDENTIFIER_LENGTH];
    int8_t dtype;
    uint64_t byte_offset;
    uint64_t byte_width;
};

struct tactyk_asmvm__struct {
    char name[TACTYK__MAX_IDENTIFIER_LENGTH];
    uint64_t byte_stride;
    uint64_t num_properties;
    struct tactyk_asmvm__property *properties;
};

struct tactyk_asmvm__Program {
    char* name;                                             // program name (currently unused)
    int32_t length;                                         // program instruction length
    //struct tactyk_asmvm__symbols symbols;                   // symbol tables (functions, variables, statically allocated memory, data structures)
    struct tactyk_dblock__DBlock *memory_layout_ll;             //
    struct tactyk_dblock__DBlock *memory_layout_hl;             //
    struct tactyk_dblock__DBlock *functions;
    //void* vmbin;
    //void* immediates;
    tactyk_asmvm__op *command_map;
    tactyk_asmvm__op *function_map;
    void* executable;
    tactyk_asmvm__runnable run;
    tactyk_asmvm__show_indicator debug_func;
};



// just a name to bind to an instruction pointer.
struct tactyk_asmvm__identifier {
    char txt[TACTYK__MAX_IDENTIFIER_LENGTH+1];
    int64_t value;
    char *class;
};


struct tactyk_asmvm__VM* tactyk_asmvm__new_vm();
//void tactyk_asmvm__dispose_VM(struct tactyk_asmvm__VM *vm);

struct tactyk_asmvm__Context* tactyk_asmvm__new_context(struct tactyk_asmvm__VM *vm);
void tactyk_asmvm__add_program(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *program);

uint64_t tactyk_asmvm__get(struct tactyk_asmvm__Program *tactyk_pl__prog, void* data, char* varname);
void tactyk_asmvm__set(struct tactyk_asmvm__Program *tactyk_pl__prog, void* data, char* varname, uint64_t value);

bool tactyk_asmvm__prepare_invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname);
void tactyk_asmvm__invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *tactyk_pl__prog, char* funcname);
bool tactyk_asmvm__call(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *tactyk_pl__prog, char* funcname);
bool tactyk_asmvm__resume(struct tactyk_asmvm__Context *context);

typedef void (*tactyk_asmvm__debug_callback)(struct tactyk_asmvm__Context *ctx);
void tactyk_asmvm__invoke_debug(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *tactyk_pl__prog, char* funcname, tactyk_asmvm__debug_callback dbg_callback);

void tactyk_asmvm__get_mblock(struct tactyk_asmvm__Context *asmvm_context, void* name, struct tactyk_asmvm__memblock_highlevel **m_hl, struct tactyk_asmvm__memblock_lowlevel **m_ll);
void tactyk_asmvm__update_declared_memblock(struct tactyk_asmvm__Context *asmvm_context, struct tactyk_asmvm__memblock_lowlevel *m_ll, int64_t active_index);
void tactyk_asmvm__bind(struct tactyk_asmvm__Context *asmvm_context, uint64_t index, void* object, uint64_t object_size, uint64_t object_count);
void tactyk_asmvm__unbind(struct tactyk_asmvm__Context *asmvm_context, void* object);
//extern void tactyk_asmvm__run(struct tactyk_asmvm__Context *context);

#endif /* TACTYK_ASMVM__INCLUDE_GUARD */
