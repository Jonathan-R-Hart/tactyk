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
//  for debug invocation

#include <stdlib.h>
//  calloc, free

#include "tactyk_dblock.h"

struct tactyk_asmvm__Context;
typedef uint64_t (*tactyk_asmvm__runnable)(struct tactyk_asmvm__Context *ctx);
typedef void (*tactyk_asmvm__show_indicator)(uint64_t iptr, char *msg);

// Represents branch targtes within compiled tactyk programs.
//  These are not directly callable (tactyk uses its own calling conventions internally), but act like functions.
//      actual calls into tactyk are accomplished by passing an index (which is used to access these from a jump table)
// Should probably consider just switching to void*
//
typedef void (*tactyk_asmvm__op)();

#define TACTYK_ASMVM_STATUS_BREAK 16

#define MAX_APIFUNCS 1024
#define MAX_LABELS 65536
#define MAX_STRUCTS 65536
#define MAX_IDENTIFIER_LENGTH 255
#define TACTYK_ASMVM__MEMBLOCK_CAPACITY 256

extern const int32_t STATIC_MEMORY_HEADER_LENGTH;
extern const int32_t STATIC_MEMORY_LENGTH;


struct tactyk_asmvm__register_bank {
    uint64_t rFLAGS;                            // register bank position 0 technically is for the VM context pointer, but is never read by the VM, so the slot is
    //union tactyk_asmvm__immediate_properties *rIMM;
    uint64_t* rSTASH;
    tactyk_asmvm__op *rPROG;
    uint64_t rLWCSI;             // no longer for holding instruction pointers (everything but the initial call into tactyk is now a direct jump).
                                // To be renamed after deciding what to do with fastcalls.
    uint64_t rTEMP;
    uint64_t *rADDR1;
    uint64_t *rADDR2;
    uint64_t *rADDR3;
    uint64_t *rADDR4;
    uint64_t rMCSI;
    uint64_t rA;
    uint64_t rB;
    uint64_t rC;
    uint64_t rD;
    uint64_t rE;
    uint64_t rF;
};

// memblock type hints
// These are only the automatically assigned "type" specifications, which TACTYK-PL uses to indicate how a memblockw as specified
// These are not to be construed as absolute rules
// If you need a custom type declaration for a memblock, pick a unique integer, assign it to the memblock type fields, and use it
//  as you see fit.
#define TACTYK_ASMVM__MEMBLOCK_TYPE__UNKNOWN 0
#define TACTYK_ASMVM__MEMBLOCK_TYPE__STATIC 1
#define TACTYK_ASMVM__MEMBLOCK_TYPE__ALLOC 2
#define TACTYK_ASMVM__MEMBLOCK_TYPE__EXTERNAL 3

// memory layout specification used within the virtual machine
//      (basically just a pointer to the allocated memory, boundaries, and a pair of extra properties for implementing queues)
//          (details about properties are provided to the emit itnerface as a set of named constants)
struct tactyk_asmvm__memblock_lowlevel {
    uint8_t* base_address;
    uint32_t element_bound;
    uint32_t array_bound;
    uint32_t memblock_index;
    uint32_t type;
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

#define MICROCONTEXT_SIZE 32
#define MICROCONTEXT_SCALE 1024
#define STASH_SCALE 32

struct tactyk_asmvm__Program;
// would prefer an explicit struct memory layout here, since this represents a low-level data structure
struct tactyk_asmvm__Context {

    uint64_t max_instruction_pointer;
    struct tactyk_asmvm__Context *subcontext;


    // program memory
    //  (or whatever memory the host/runtime assigns to programs)
    struct tactyk_asmvm__memblock_lowlevel *memblocks;
    uint64_t memblock_count;

    struct tactyk_asmvm__memblock_lowlevel active_memblocks[4];

    void *lwcall_stack;
    void *microcontext_stack;
    uint64_t microcontext_stack_offset;
    uint64_t microcontext_stack_size;

    uint64_t diagnostic_data[1024];

    struct tactyk_asmvm__VM *vm;
    tactyk_asmvm__op *program;
    struct tactyk_asmvm__Program *hl_program_ref;      // a pointer to help high-level code access representative data structures.

    uint64_t instruction_index;     //tactyk function to call into.

    // erorr codes go here when tactyk-vm sees what it dont like.
    uint64_t STATUS;

    uint64_t stepper;

    struct tactyk_asmvm__register_bank regbank_A;     // main register storage bank - for runtime <-> calls into VM
    //struct tactyk_asmvm__register_bank regbank_B;     // secondary register storage bank - for VM <-> calls into other things
};
void tactyk_asmvm__print_context(struct tactyk_asmvm__Context *context);
void tactyk_asmvm__print_diagnostic_data(struct tactyk_asmvm__Context *context, int64_t amount);

struct tactyk_asmvm__property {
    char name[MAX_IDENTIFIER_LENGTH];
    int8_t dtype;
    uint64_t byte_offset;
    uint64_t byte_width;
};

struct tactyk_asmvm__struct {
    char name[MAX_IDENTIFIER_LENGTH];
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
    void* executable;
    tactyk_asmvm__runnable run;
    tactyk_asmvm__show_indicator debug_func;
};

// Internal Virtual Machine data structure.
// This is used within tactyk for an internal stack and to offload the native/runtime registers
struct tactyk_asmvm__VM {
    struct tactyk_asmvm__register_bank runtime_registers;
    uint64_t vm_stack_position;
    uint64_t vm_stack[1024];
    struct tactyk_asmvm__Context *static_contexts[1024];
    uint64_t dcontext_position;
    struct tactyk_asmvm__Context *dynamic_contexts[1024];
};



// just a name to bind to an instruction pointer.
struct tactyk_asmvm__identifier {
    char txt[MAX_IDENTIFIER_LENGTH+1];
    int64_t value;
    char *class;
};


struct tactyk_asmvm__VM* tactyk_asmvm__new_vm();
//void tactyk_asmvm__dispose_VM(struct tactyk_asmvm__VM *vm);

struct tactyk_asmvm__Context* tactyk_asmvm__new_context(struct tactyk_asmvm__VM *vm);

uint64_t tactyk_asmvm__get(struct tactyk_asmvm__Program *tactyk_pl__prog, void* data, char* varname);
void tactyk_asmvm__set(struct tactyk_asmvm__Program *tactyk_pl__prog, void* data, char* varname, uint64_t value);
void tactyk_asmvm__invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *tactyk_pl__prog, char* funcname);

typedef void (*tactyk_asmvm__debug_callback)(struct tactyk_asmvm__Context *ctx);
void tactyk_asmvm__invoke_debug(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *tactyk_pl__prog, char* funcname, tactyk_asmvm__debug_callback dbg_callback);

void tactyk_asmvm__get_mblock(struct tactyk_asmvm__Context *asmvm_context, void* name, struct tactyk_asmvm__memblock_highlevel **m_hl, struct tactyk_asmvm__memblock_lowlevel **m_ll);

//extern void tactyk_asmvm__run(struct tactyk_asmvm__Context *context);

#endif /* TACTYK_ASMVM__INCLUDE_GUARD */
