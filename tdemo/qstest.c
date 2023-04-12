//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "tactyk_debug.h"

#include "qstest.h"

char* tactyk_qsort_program_ctl = R"""(
    # quicksort.tactykpl
    # Sort input words using the quicksort algorithm

    mem stack 102400

    extmem data

    struct qs_args
        8 last

    mem args qs_args 1

    var stored_bounds
        get
            unstash e1f1
        set
            stash e1f1
    REDIR2:
        lwcall DO_QSORT
        lwreturn
    REDIR1:
        lwcall REDIR2
        lwreturn
    MAIN:
        assign a 12
        assign b 23
        assign c 34
        assign d 45
        assign e 56
        assign f 67
        stash a1b1c1d1e1f1
        assign a 0
        assign b 0
        assign c 0
        assign d 0
        assign e 0
        assign f 0
        mctxpop
        lwcall REDIR1
        mctxpush
        unstash a1b1c1d1e1f1
        exit
)""";

char* tactyk_qsort_program_qs = R"""(
    DO_QSORT:
        bind addr1 args
        load qword f addr1 qs_args.last
        # pivot, low, and high pointers
        bind addr1 data
        bind addr2 data
        bind addr3 data
        shift-left e 3
        shift-left f 3
        # parameter stack
        bind addr4 stack
        goto QSORT_TEST
    QSORT:
        # get next partition bounds from the stack
        pop f addr4
        pop e addr4
    QSORT_TEST:
        if e < 0 QSORT_SKIP
        if f < 0 QSORT_SKIP
        # have the partition bounds met or crossed?
        if e >= f QSORT_SKIP

        # copy the bounds into context local storage so they can be recovered after partition

        stash e1f1
        # set stored_bounds

        goto QSORT_PARTITION
    QSORT_SKIP:
        # is there anything left on the stack?
        assign a addr4.offset
        # offset-get a addr4
        if a != 0 QSORT
        goto DONE
    QSORT_EPILOGUE:
        # move the new partition point out of the way and recover the original bounds
        assign d f

        unstash e1f1
        # get stored_bounds

        # defer partition e->d.
        push addr4 e
        push addr4 d

        assign e d
        add e 8

        #  sort partition (d+1)->f next

        goto QSORT_TEST

    QSORT_PARTITION:
        # set pivot to center of partition
        assign a e
        add a f

        # divide the aword-aligned index by two with bit shifting.  In principle, a special isntruction that uses scaled-index addressing should be used here to avoid
        #  the extra bitwise operation, but that's extra work.
        shift-right a 4
        shift-left a 3

        # dereference pivot
        load qword a addr3 a

        sub e 8
        add f 8

    PARTITION_LOOP_LEFT:
        add e 8
        load qword b addr1 e

        if b < a PARTITION_LOOP_LEFT

    PARTITION_LOOP_RIGHT:

        sub f 8
        load qword c addr2 f

        if c > a PARTITION_LOOP_RIGHT
        if e >= f QSORT_EPILOGUE

        store qword addr1 e c
        store qword addr2 f b

        goto PARTITION_LOOP_LEFT

    DONE:
        lwreturn
)""";

char* tactyk_qsort_program_diag = R"""(
    DIAG:
        cpuclocks
        exit
)""";


struct tactyk_asmvm__Program *qsprogram;

struct tactyk_asmvm__Program* run_qsort_tests(struct tactyk_emit__Context *emitctx, int64_t len, int64_t seed, struct tactyk_asmvm__Context *ctx) {

    printf("\n");
    printf("QUICKSORT TEST\n");

    int64_t c1, c2, init_hash, final_hash;

    struct tactyk_pl__Context *plctx = tactyk_pl__new(emitctx);
    tactyk_pl__load(plctx, tactyk_qsort_program_diag);
    tactyk_pl__load(plctx, tactyk_qsort_program_qs);
    tactyk_pl__load(plctx, tactyk_qsort_program_ctl);
    struct tactyk_asmvm__Program *program = tactyk_pl__build(plctx);

    qsprogram = program;

    struct tactyk_asmvm__memblock_highlevel *mblk = tactyk_dblock__get(program->memory_layout_hl, "args");
    uint64_t *dat = (uint64_t*)mblk->data;
    dat[0] = len-1;

    // Define the data memory
    //      (boilerplate code for specifying an externally-managed array of 8-byte words).
    struct tactyk_asmvm__memblock_highlevel *dblk = tactyk_dblock__get(program->memory_layout_hl, "data");
    dblk->data = calloc(len, 8);

    dblk->num_entries = len;
        struct tactyk_asmvm__struct *ddef = calloc(1, sizeof(struct tactyk_asmvm__struct));
        ddef->byte_stride = 8;
        ddef->num_properties = 1;
            struct tactyk_asmvm__property *dprop = calloc(1, sizeof(struct tactyk_asmvm__property));
            dprop->byte_offset = 0;
            dprop->byte_width = 8;
        ddef->properties = dprop;
    dblk->definition = ddef;
    struct tactyk_asmvm__memblock_lowlevel *dspec = dblk->memblock;
    dspec->base_address = dblk->data;
    dspec->element_bound = len*8;
    dspec->array_bound = 1;

    int64_t *data = calloc(len, 8);
    randfill(data, len, seed);
    print_a_few(data, 8);
    init_hash = compute_hash(data, len);
    printf("init hash: %lu\n", init_hash);
    printf("\n");


    tactyk_asmvm__invoke(ctx, program, "DIAG");
    c1 = ctx->diagnostic_data[0];

    cqsort(data, 0, len-1);

    tactyk_asmvm__invoke(ctx, program, "DIAG");
    c2 = ctx->diagnostic_data[0];

    print_a_few(data, 8);
    final_hash = compute_hash(data, len);
    printf("final hash: %lu\n", final_hash);
    checkit(data, len);
    printf("cycle count: %lu\n\n\n", c2-c1);

    randfill((int64_t*)dblk->data, len, seed);
    init_hash = compute_hash((int64_t*)dblk->data, len);
    print_a_few((int64_t*)dblk->data, 8);
    printf("init hash: %lu\n", init_hash);
    printf("\n");

    tactyk_asmvm__invoke(ctx, program, "DIAG");
    c1 = ctx->diagnostic_data[0];


    tactyk_asmvm__invoke(ctx, program, "MAIN");

    tactyk_asmvm__invoke(ctx, program, "DIAG");
    c2 = ctx->diagnostic_data[0];

    print_a_few((int64_t*)dblk->data, 8);
    final_hash = compute_hash((int64_t*)dblk->data, len);
    printf("final hash: %lu\n", final_hash);
    checkit((int64_t*)dblk->data, len);
    printf("cycle count: %lu\n\n\n", c2-c1);

    tactyk_debug__print_context(ctx);


    return program;
}

void print_a_few(int64_t *data, int64_t amount) {
    for (int64_t i = 0; i < amount; i++) {
        printf("%ld:\t%ld\n", i, data[i]);
    }
}

int64_t partition(int64_t *data, int64_t low, int64_t high) {
    int64_t pivot = data[(low+high)>>1];
    low -= 1;
    high += 1;
    while (true) {
        do {
            low += 1;
        } while (data[low] < pivot);
        do {
            high -= 1;
        } while (data[high] > pivot);

        if (low >= high) {
            return high;
        }
        int64_t tmp = data[low];
        data[low] = data[high];
        data[high] = tmp;
    }
}

void cqsort(int64_t *data, int64_t low, int64_t high) {
    if ( (low >= 0) && (high >= 0) && (low < high) ) {
        int64_t p = partition(data, low, high);
        cqsort(data, low, p);
        cqsort(data, p+1, high);
    }
}

// compute a hash from a block of memory
//  hash function is a slightly modified xorshift64 PRNG
int64_t compute_hash(int64_t *data, int64_t len) {
    int64_t state = 0;
    for (int64_t i = 0; i < len; i++) {
        state += data[i];
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
    }
    return state;
}

// allocate and fill a block of memory with seeded random data.
//  PRNG is xorshift64
void randfill(int64_t *data, int64_t len, int64_t seed) {
    printf(">>> %p\n", data);
    int64_t state = seed;
    for (int64_t i = 0; i < len; i++) {
        state += i;
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        data[i] = state & 0x7fffffffffffffff;
    }
}

void checkit(int64_t *data, int64_t len) {
    int64_t a = data[0];
    for (int64_t i = 0; i < len; i++) {
        int64_t b = data[i];
        if (a > b) {
            printf("FAIL\n");
            return;
        }
        a = b;
    }
    printf("PASS\n");
}

