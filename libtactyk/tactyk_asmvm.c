//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include <sys/mman.h>
#include <stdint.h>

#include "tactyk.h"
#include "tactyk_asmvm.h"
#include "tactyk_dblock.h"
#include "tactyk_alloc.h"



struct tactyk_asmvm__VM* tactyk_asmvm__new_vm() {
    struct tactyk_asmvm__VM *vm = tactyk_alloc__allocate(1, sizeof(struct tactyk_asmvm__VM));
    vm->program_count = 0;
    vm->program_list = tactyk_alloc__allocate(TACTYK_ASMVM__PROGRAM_CAPACITY, sizeof(struct tactyk_asmvm__program_declaration));
    return vm;
}

struct tactyk_asmvm__Context* tactyk_asmvm__new_context(struct tactyk_asmvm__VM *vm) {
    struct tactyk_asmvm__Context *ctx = tactyk_alloc__allocate(1, sizeof(struct tactyk_asmvm__Context));
    ctx->microcontext_stack = tactyk_alloc__allocate(TACTYK_ASMVM__MCTX_STACK_SIZE*TACTYK_ASMVM__MCTX_ENTRY_SIZE, sizeof(uint64_t));
    ctx->microcontext_stack_offset = 0;
    ctx->lwcall_stack_floor = 0;
    ctx->mctx_stack_floor = 0;
    ctx->lwcall_stack = tactyk_alloc__allocate(TACTYK_ASMVM__LWCALL_STACK_SIZE, sizeof(uint32_t));

    ctx->stack = tactyk_alloc__allocate(1, sizeof(struct tactyk_asmvm__Stack));
    ctx->stack->stack_lock = 0;
    ctx->stack->stack_position = -1;
    // tactyk signature
    // This is a binary transform of the pointer to the context which must be placed within the context at predefined location.
    // TACTYK uses this to validate the context pointer.  Prior to running any program code.
    uint64_t sig = *((uint64_t*)"-TACTYK-");
    sig ^= (uint64_t)ctx;
    sig += *((uint32_t*)"-CTX");
    ctx->signature = sig;

    ctx->vm = vm;
    return ctx;
}
void tactyk_asmvm__add_program(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *program) {
;
    if (context->vm->program_count >= TACTYK_ASMVM__PROGRAM_CAPACITY) {
        error("ASMVM -- Too many loaded programs", NULL);
    }
    #ifdef TACTYK_DEBUG
    printf("ADD PROGRAM\n");
    #endif // TACTYK_DEBUG
    struct tactyk_asmvm__program_declaration *dec = &context->vm->program_list[context->vm->program_count];
    context->vm->program_count += 1;
    dec->instruction_count = program->length;
    dec->instruction_jumptable = program->command_map;
    uint64_t num_funcs = program->functions->element_count;
    dec->function_count = num_funcs;
    #ifdef USE_TACTYK_ALLOCATOR
    void* target_address = tactyk__mk_random_base_address();
    #else
    void* target_address = NULL;
    #endif // USE_TACTYK_ALLOCATOR
    uint64_t fjt_size = num_funcs * sizeof(void*);
    tactyk_asmvm__op * fjumptable = mmap(target_address, fjt_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    //tactyk_asmvm__op *fjumptable = tactyk_alloc__allocate(num_funcs, sizeof(tactyk_asmvm__op));
    for (uint64_t i = 0; i < num_funcs; i += 1) {
        struct tactyk_asmvm__identifier *id = tactyk_dblock__index(program->functions->store, i);
        #ifdef TACTYK_DEBUG
        printf("func: '%s' index=%ju ptr=%p\n", id->txt, id->value, program->command_map[id->value]);
        #endif // TACTYK_DEBUG
        fjumptable[i] = program->command_map[id->value];
    }
    mprotect(fjumptable, fjt_size, PROT_READ );
    dec->function_jumptable = fjumptable;
}

/*
uint64_t tactyk_asmvm__get(struct tactyk_asmvm__Program *prog, void* data, char* varname) {
    struct tactyk_asmvm__identifier *varid = tactyk_table__get_strkey(prog->symbols.idtbl, varname);
    uint64_t index = varid->value;
    return ((uint64_t*)data)[index];
}
void tactyk_asmvm__set(struct tactyk_asmvm__Program *prog, void* data, char* varname, uint64_t value) {
    struct tactyk_asmvm__identifier *varid = tactyk_table__get_strkey(prog->symbols.idtbl, varname);
    uint64_t index = varid->value;
    ((uint64_t*)data)[index] = value;
}
*/
bool tactyk_asmvm__prepare_invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname) {
    struct tactyk_asmvm__identifier *identifier = tactyk_dblock__get(prog->functions, funcname);
    if (identifier == NULL) {
        return false;
    }
    int64_t iptr = identifier->value;

    context->hl_program_ref = prog;
    context->memblocks = (struct tactyk_asmvm__memblock_lowlevel*) prog->memory_layout_ll->data;
    context->memblock_count = TACTYK_ASMVM__MEMBLOCK_CAPACITY;
    context->max_instruction_pointer = prog->length-1;
    context->program_map = prog->command_map;
    context->instruction_index = iptr;

    return (iptr < prog->length);
}

bool tactyk_asmvm__call(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname) {
    if (tactyk_asmvm__prepare_invoke(context, prog, funcname)) {
        uint64_t result = prog->run(context);
        if (result < 100) {
            result = context->STATUS;
        }

        // indicate if the machine is in a valid state
        return (result == 2) || (result == 3) || (result == 4);
    }
    else {
        return false;
    }
}
void tactyk_asmvm__invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname) {
    if (tactyk_asmvm__prepare_invoke(context, prog, funcname)) {
        uint64_t result = prog->run(context);
        if (result < 100) {
            result = context->STATUS;
        }
        if (result >= 100) {
            char msg[256];
            sprintf(msg, "TACTYK -- error #%ju", result);
            error(msg, NULL);
        }
    }
}

/*
void tactyk_asmvm__dispose_VM(struct tactyk_asmvm__VM *vm) {
    tfree(vm->contexts);
    tfree(vm->programs);
    tfree(vm);
}
*/

void tactyk_asmvm__get_mblock(struct tactyk_asmvm__Context *asmvm_context, void* name, struct tactyk_asmvm__memblock_highlevel **m_hl, struct tactyk_asmvm__memblock_lowlevel **m_ll) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = NULL;
    struct tactyk_asmvm__memblock_highlevel *mem_hl = tactyk_dblock__get(asmvm_context->hl_program_ref->memory_layout_hl, name);
    if (mem_hl == NULL) {
        error("ASMVM-GET_MBLOCK -- memory block not specified", name);
    }
    else {
        mem_ll = mem_hl->memblock;
    }

    *m_hl = mem_hl;
    *m_ll = mem_ll;
}
void tactyk_asmvm__update_dynamic_memblock(struct tactyk_asmvm__Context *asmvm_context, struct tactyk_asmvm__memblock_lowlevel *m_ll, int64_t active_index) {
    struct tactyk_asmvm__memblock_lowlevel *mem_ll = tactyk_dblock__index(asmvm_context->hl_program_ref->memory_layout_ll, m_ll->memblock_index);
    struct tactyk_asmvm__memblock_highlevel *mem_hl = tactyk_dblock__index(asmvm_context->hl_program_ref->memory_layout_hl, m_ll->memblock_index);
    mem_hl->data = m_ll->base_address;
    *mem_ll = *m_ll;
    switch(active_index) {
        case 0: {
            asmvm_context->reg.rADDR1 = (uint64_t*) m_ll->base_address;
            break;
        }
        case 1: {
            asmvm_context->reg.rADDR2 = (uint64_t*) m_ll->base_address;
            break;
        }
        case 2: {
            asmvm_context->reg.rADDR3 = (uint64_t*) m_ll->base_address;
            break;
        }
        case 3: {
            asmvm_context->reg.rADDR4 = (uint64_t*) m_ll->base_address;
            break;
        }
        //if active_index is anything else (presumably -1), dont attempt to update an address register
    }
}
