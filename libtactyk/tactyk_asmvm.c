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
    return vm;
}


struct tactyk_asmvm__Context* tactyk_asmvm__new_context(struct tactyk_asmvm__VM *vm) {
    struct tactyk_asmvm__Context *ctx = calloc(1, sizeof(struct tactyk_asmvm__Context));
    ctx->microcontext_stack = calloc(32*65536, sizeof(uint64_t));
    ctx->microcontext_stack_offset = 0;
    ctx->microcontext_stack_size = 32*65536*sizeof(uint64_t);
    ctx->lwcall_stack = calloc(65536, sizeof(uint32_t));
    //ctx->mctxstack[0] = calloc(MICROCONTEXT_SIZE*MICROCONTEXT_SCALE, 8);
    //ctx->mctxstack[1] = calloc(MICROCONTEXT_SIZE*MICROCONTEXT_SCALE, 8);
    //ctx->mctxstack[2] = calloc(MICROCONTEXT_SIZE*MICROCONTEXT_SCALE, 8);
    //ctx->mctxstack[3] = calloc(MICROCONTEXT_SIZE*MICROCONTEXT_SCALE, 8);
    //ctx->active_mctxstack = ctx->mctxstack[0];
    //uint64_t mctxstack[MICROCONTEXT_SIZE*MICROCONTEXT_SCALE];
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
        context->regbank_A.rPROG = prog->command_map;
        //context->bank_A.rIMM = prog->immediates;
        context->instruction_index = iptr;
        //context->program = prog->program;
        //context->stash = NULL;
        //context->bank_A.rMAXIP = prog->length-1;
        prog->run(context);
        //tactyk_asmvm__run(context);
    }
}
extern void tactyk_asmvm__invoke_debug(struct tactyk_asmvm__Context *context, struct tactyk_asmvm__Program *prog, char* funcname, tactyk_asmvm__debug_callback dbg_callback) {
    //struct tactyk_asmvm__identifier *identifier = tactyk_table__get_strkey(prog->symbols.labeltbl, funcname);
    struct tactyk_asmvm__identifier *identifier = tactyk_dblock__get(prog->functions, funcname);
    int64_t iptr = identifier->value;
    context->hl_program_ref = prog;
    if (iptr < prog->length) {
        context->memblocks = (struct tactyk_asmvm__memblock_lowlevel*) prog->memory_layout_ll->data;
        //context->memblocks = prog->memory_layout_ll;
        context->memblock_count = TACTYK_ASMVM__MEMBLOCK_CAPACITY;
        context->max_instruction_pointer = prog->length-1;
        context->regbank_A.rPROG = prog->executable;
        //context->bank_A.rPROG = prog->program_data;
        //context->bank_A.rIMM = prog->immediates;
        context->instruction_index = iptr;
        //context->bank_A.rMAXIP = prog->length-1;
        prog->run(context);
        //tactyk_asmvm__run(context);
        while (context->STATUS == TACTYK_ASMVM_STATUS_BREAK) {
            dbg_callback(context);
            printf( "Type somethign with 's' to toggle stepping.  Press ENTER proceed to next break point ...\n");
            if (context->stepper == 0) {
                printf("stepper:  OFF\n");
            }
            else {
                printf("stepper:  ON\n");
            }
            char ch;
            //prog->debug_func(context->bank_A.rIPTR, prog->debug_info);
            do {
                ch = getchar( );
                if (ch == 's') {
                    if (context->stepper == 0) {
                        context->stepper = 1;
                    }
                    else {
                        context->stepper = 0;
                    }
                }
                //printf("%d\n", ch);
            }
            while (ch != 10);
            prog->run(context);
            //tactyk_asmvm__run(context);
        }
    }

}
/*
void tactyk_asmvm__dispose_VM(struct tactyk_asmvm__VM *vm) {
    free(vm->contexts);
    free(vm->programs);
    free(vm);
}
*/


void tactyk_asmvm__print_context(struct tactyk_asmvm__Context *context) {
    printf("Status:     %lu\n", context->STATUS);
    printf("I-index:    %lu\n", context->instruction_index);
    printf("StashPTR:   %lu\n", (uint64_t)context->regbank_A.rSTASH);
    printf("ProgPTR:    %lu\n", (uint64_t)context->regbank_A.rPROG);
    printf("LWCSI:      %lu\n", context->regbank_A.rLWCSI);
    printf("MCSI:       %ld\n", context->regbank_A.rMCSI);
    printf("TEMP:       %ld\n", context->regbank_A.rTEMP);
    printf("Addr1PTR:   %lu\n", (uint64_t)context->regbank_A.rADDR1);
    printf("a1bounds:   %lu, %lu\n", (uint64_t)context->active_memblocks[0].element_bound, (uint64_t)context->active_memblocks[0].array_bound);
    printf("Addr2PTR:   %lu\n", (uint64_t)context->regbank_A.rADDR2);
    printf("a2bounds:   %lu, %lu\n", (uint64_t)context->active_memblocks[1].element_bound, (uint64_t)context->active_memblocks[1].array_bound);
    printf("Addr3PTR:   %lu\n", (uint64_t)context->regbank_A.rADDR3);
    printf("a3bounds:   %lu, %lu\n", (uint64_t)context->active_memblocks[2].element_bound, (uint64_t)context->active_memblocks[2].array_bound);
    printf("Addr4PTR:   %lu\n", (uint64_t)context->regbank_A.rADDR4);
    printf("a4bounds:   %lu, %lu\n", (uint64_t)context->active_memblocks[3].element_bound, (uint64_t)context->active_memblocks[3].array_bound);
    printf("rA:         %ld\n", context->regbank_A.rA);
    printf("rB:         %ld\n", context->regbank_A.rB);
    printf("rC:         %ld\n", context->regbank_A.rC);
    printf("rD:         %ld\n", context->regbank_A.rD);
    printf("rE:         %ld\n", context->regbank_A.rE);
    printf("rF:         %ld\n", context->regbank_A.rF);
}

void tactyk_asmvm__print_diagnostic_data(struct tactyk_asmvm__Context *context, int64_t amount) {
    printf("diagnistic data: \n");
    for (int64_t i = 0; i < amount; i++) {
        printf("%ld:\t%ld\n", i, context->diagnostic_data[i]);
    }
}

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
