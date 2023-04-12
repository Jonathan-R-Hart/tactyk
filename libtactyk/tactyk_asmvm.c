//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "tactyk.h"
#include "tactyk_asmvm.h"
#include "tactyk_dblock.h"

struct tactyk_asmvm__VM* tactyk_asmvm__new_vm() {
    struct tactyk_asmvm__VM *vm = calloc(1, sizeof(struct tactyk_asmvm__VM));
    vm->program_count = 0;
    vm->program_list = calloc(TACTYK_ASMVM__PROGRAM_CAPACITY, sizeof(struct tactyk_asmvm__program_declaration));
    return vm;
}

struct tactyk_asmvm__Context* tactyk_asmvm__new_context(struct tactyk_asmvm__VM *vm) {
    struct tactyk_asmvm__Context *ctx = calloc(1, sizeof(struct tactyk_asmvm__Context));
    ctx->microcontext_stack = calloc(TACTYK_ASMVM__MCTX_STACK_SIZE*TACTYK_ASMVM__MCTX_ENTRY_SIZE, sizeof(uint64_t));
    ctx->microcontext_stack_offset = 0;
    ctx->microcontext_stack_size = TACTYK_ASMVM__MCTX_STACK_SIZE*TACTYK_ASMVM__MCTX_ENTRY_SIZE*sizeof(uint64_t);
    ctx->lwcall_stack = calloc(TACTYK_ASMVM__LWCALL_STACK_SIZE, sizeof(uint32_t));

    ctx->ctx_stack = calloc(1, sizeof(struct tactyk_asmvm__Stack));
    ctx->ctx_stack->stack_lock = 0;
    ctx->ctx_stack->stack_position = -1;
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
void tactyk_asmvm__invoke(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname) {

    //ctx->bank_A.rPROG = prog->program;
    //ctx->max_instruction_pointer = prog->length-1;
    //printf("run @ %lu %lu\n", prog->run, (char)*((char*)prog->run));
    //uint64_t result = prog->run(ctx);

    //printf("result= %lu\n", result);

    //printf("...  pfuncs:  %p\n", prog);
    struct tactyk_asmvm__identifier *identifier = tactyk_dblock__get(prog->functions, funcname);
    //struct tactyk_asmvm__identifier *identifier = tactyk_table__get_strkey(prog->symbols.labeltbl, funcname);
    int64_t iptr = identifier->value;
    //printf("ip=%jd ln=%jd\n mbptr = %p\n", iptr, prog->length, prog->memory_layout_ll);
    context->hl_program_ref = prog;
    //iptr = 0;
    if (iptr < prog->length) {
        context->memblocks = (struct tactyk_asmvm__memblock_lowlevel*) prog->memory_layout_ll->data;
        context->memblock_count = TACTYK_ASMVM__MEMBLOCK_CAPACITY;
        context->max_instruction_pointer = prog->length-1;
        //context->bank_A.rPROG = prog->executable;
        context->reg.rPROG = prog->command_map;
        //context->bank_A.rIMM = prog->immediates;
        context->instruction_index = iptr;
        //context->program = prog->program;
        //context->stash = NULL;
        //context->bank_A.rMAXIP = prog->length-1;
        uint64_t result = prog->run(context);
        if (result < 100) {
            result = context->STATUS;
        }
        if (result >= 100) {
            char msg[256];
            sprintf(msg, "TACTYK -- error #%ju", result);
            error(msg, NULL);
        }
        //tactyk_asmvm__run(context);
    }
}

/*
void tactyk_asmvm__dispose_VM(struct tactyk_asmvm__VM *vm) {
    free(vm->contexts);
    free(vm->programs);
    free(vm);
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
