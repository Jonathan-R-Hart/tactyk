//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "tactyk.h"
#include "tactyk_dblock.h"
#include "tactyk_util.h"
#include "tactyk_alloc.h"

#define TACTYK_DBLOCK__PRINT_MAX_INDENT 32
#define DBLOCKS_CAPACITY (1<<20)

/*
    DBLOCK is tactyk's memory management utility.


    Known hazards:
    If a reference to a managed object is obtained, then another managed object is instantiated and it triggers a table expansion, then the original referenced
    becomes a dangling pointer.  At present, there is no plan to interact with managed objects that will trigger that.

    Workarounds:  allocate enough memory to each table to not trigger table expansions.  Copy the returned struct when fetching items from a table, and if there is a
    need to update the table entry, obtain a fresh pointer and copy the modified data back into the table entry.  Modify the system to return structs instead of pointers.
    Instead of resizing the backing memory block, it could simply use linked lists to manage multiple such blocks 9preferred approach)


*/

struct tactyk_dblock__DBlock dblocks[DBLOCKS_CAPACITY];
struct tactyk_dblock__DBlock* dblocks_stack[DBLOCKS_CAPACITY];
int64_t dblocks_stack_idx;

struct tactyk_dblock__DBlock* tactyk_dblock__from_string_or_dblock(void *ptr);

void tactyk_dblock__init() {
    //dblocks_list = talloc
    for (int64_t i = 0; i < DBLOCKS_CAPACITY; i += 1) {
        dblocks_stack[i] = &dblocks[i];
    }
    dblocks_stack_idx = DBLOCKS_CAPACITY-1;
}

struct tactyk_dblock__DBlock* tactyk_dblock__alloc() {
    if ( (-1) == dblocks_stack_idx) {
        error("DBLOCK -- internal storage capacity exceeded", NULL);
    }
    struct tactyk_dblock__DBlock *db = dblocks_stack[dblocks_stack_idx];
    db->type = tactyk_dblock__UNSPECIFIED;
    dblocks_stack[dblocks_stack_idx] = NULL;
    if (db == NULL) {
        error("NULL ENTRY ON DBLOCK STACK", NULL);
    }
    dblocks_stack_idx -= 1;
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__new(uint64_t capacity) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    db->type = tactyk_dblock__STRING;
    db->length = 0;
    db->capacity = capacity;
    db->data = tactyk_alloc__allocate(capacity, 1);
    db->self_managed = true;
    db->child = NULL;
    db->next = NULL;
    db->stride = 1;
    db->fixed = false;
    db->persistence_code = 0;
    tactyk_dblock__update_hash(db);
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__data_copy(struct tactyk_dblock__DBlock *source) {
    struct tactyk_dblock__DBlock *copy = tactyk_dblock__alloc();
    tactyk_dblock__set_content(copy, source);
    return copy;
}

struct tactyk_dblock__DBlock* tactyk_dblock__shallow_copy(struct tactyk_dblock__DBlock *src) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    memcpy(db, src, sizeof(struct tactyk_dblock__DBlock));
    db->self_managed = true;
    db->data = tactyk_alloc__allocate(db->capacity, 1);
    memcpy(db->data, src->data, db->length);
    db->persistence_code = 0;
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__deep_copy(struct tactyk_dblock__DBlock *src) {
    if (src == NULL) {
        return NULL;
    }
    struct tactyk_dblock__DBlock *db = tactyk_dblock__shallow_copy(src);
    db->child = tactyk_dblock__deep_copy(src->child);
    db->next = tactyk_dblock__deep_copy(src->next);
    db->store = tactyk_dblock__deep_copy(src->store);
    db->token = tactyk_dblock__deep_copy(src->token);

    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__from_safe_c_string(char *data) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    db->type = tactyk_dblock__EXTERNAL;
    db->length = strlen(data);
    db->capacity = db->length;
    db->child = NULL;
    db->next = NULL;
    db->stride = 1;
    db->fixed = false;
    db->self_managed = false;
    db->data = (uint8_t*) data;
    tactyk_dblock__update_hash(db);
    db->persistence_code = 0;
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__from_c_string(char *data) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    db->type = tactyk_dblock__STRING;
    db->length = strlen(data);
    db->capacity = db->length;
    db->child = NULL;
    db->next = NULL;
    db->stride = 1;
    db->fixed = false;
    db->self_managed = true;
    db->data = tactyk_alloc__allocate(db->length, 1);
    memcpy(db->data, data, db->length);
    tactyk_dblock__update_hash(db);
    db->persistence_code = 0;
    return db;
}
struct tactyk_dblock__DBlock* tactyk_dblock__from_int(int64_t value) {
    char txt[32];
    sprintf(txt, "%jd", value);
    //printf("%s\n", txt);
    struct tactyk_dblock__DBlock *dblock = tactyk_dblock__from_c_string(txt);
    return dblock;
}
struct tactyk_dblock__DBlock* tactyk_dblock__from_uint(uint64_t value) {
    char txt[32];
    sprintf(txt, "%ju", value);
    //printf("%s\n", txt);
    struct tactyk_dblock__DBlock *dblock = tactyk_dblock__from_c_string(txt);
    return dblock;
}
struct tactyk_dblock__DBlock* tactyk_dblock__from_float(double value) {
    char txt[256];
    sprintf(txt, "%f", value);
    struct tactyk_dblock__DBlock *dblock = tactyk_dblock__from_c_string(txt);
    return dblock;
}


struct tactyk_dblock__DBlock* tactyk_dblock__from_ptr(void *ptr) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();

    db->length = sizeof(void*);
    db->capacity = sizeof(void*);
    db->child = NULL;
    db->next = NULL;
    db->stride = sizeof(void*);
    db->fixed = true;
    db->type = tactyk_dblock__EXTERNAL;
    db->self_managed = false;
    db->data = ptr;
    db->persistence_code = 0;
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__from_bytes(struct tactyk_dblock__DBlock *out, uint8_t *data, uint64_t start_index, uint64_t length, bool is_safe_data) {
    //printf("dblock from bytes out=%p data=%p si=%ju len=%ju\n", out, data, start_index, length);

    if (out == NULL) {
        out = tactyk_dblock__alloc();
    }
    out->length = length;
    out->capacity = length;
    out->child = NULL;
    out->next = NULL;
    out->stride = 1;
    out->fixed = false;
    if (is_safe_data) {
        out->type = tactyk_dblock__EXTERNAL;
        out->self_managed = false;
        out->data = &data[start_index];
    }
    else {
        out->type = tactyk_dblock__STRING;
        out->self_managed = true;
        out->data = tactyk_alloc__allocate(length, 1);
        memcpy(out->data, &data[start_index], length);
    }
    tactyk_dblock__update_hash(out);
    out->persistence_code = 0;
    return out;
}

// Test a pointer to determinine if it refers to a managed dblock.
//      (It is assumed to be a managed dblcok if it addresses anything within the stattically allocated sblock array)
bool tactyk_dblock__is_dblock(void *ptr) {
    int64_t ofs = (int64_t)ptr - (int64_t)dblocks;
    return (ofs >= 0) && (ofs < (int64_t)sizeof(dblocks));
}

void tactyk_dblock__set_content(struct tactyk_dblock__DBlock *dest, struct tactyk_dblock__DBlock *source) {
    if (dest->self_managed == true) {
        tactyk_alloc__free(dest->data);
    }
    uint8_t *data = tactyk_alloc__allocate(1, source->length);
    memcpy(data, source->data, source->length);
    dest->data = data;
    dest->length = source->length;
    dest->capacity = source->length;
    dest->element_capacity = source->element_count;
    dest->element_count = source->element_count;
    dest->stride = source->stride;
    dest->hashcode = source->hashcode;
    dest->self_managed = true;
}

// reset the dblock so it can be reused.
void tactyk_dblock__dispose(struct tactyk_dblock__DBlock *dblock) {
    if (dblock == NULL) {
        return;
    }
    if (dblock->type == tactyk_dblock__NONE) {
        return;
    }
    if (dblock->child != NULL) {
        tactyk_dblock__dispose(dblock->child);
    }
    if (dblock->next != NULL) {
        tactyk_dblock__dispose(dblock->next);
    }
    if (dblock->token != NULL) {
        tactyk_dblock__dispose(dblock->token);
    }
    if (dblock->store != NULL) {
        tactyk_dblock__dispose(dblock->store);
    }
    if (dblock->self_managed) {
        if (dblock->data != NULL) {
            tactyk_alloc__free(dblock->data);
            dblock->data = NULL;
        }
    }
    memset(dblock, 0, sizeof(struct tactyk_dblock__DBlock));

    if (tactyk_dblock__is_dblock(dblock)) {
        dblocks_stack_idx += 1;
        dblocks_stack[dblocks_stack_idx] = dblock;
    }
    else {
        warn("DBLOCK -- Unrecognized/Unmanaged tactyk_dblock disposed", NULL);
    }
}

void tactyk_dblock__weak_dispose(struct tactyk_dblock__DBlock *dblock) {
    if (dblock == NULL) {
        return;
    }
    if (dblock->type == tactyk_dblock__NONE) {
        return;
    }

    if (dblock->self_managed) {
        if (dblock->data != NULL) {
            tactyk_alloc__free(dblock->data);
            dblock->data = NULL;
        }
    }
    memset(dblock, 0, sizeof(struct tactyk_dblock__DBlock));

    if (tactyk_dblock__is_dblock(dblock)) {
        dblocks_stack_idx += 1;
        dblocks_stack[dblocks_stack_idx] = dblock;
    }
    else {
        warn("DBLOCK -- Unrecognized/Unmanaged tactyk_dblock disposed", NULL);
    }
}

void tactyk_dblock__set_persistence_code(struct tactyk_dblock__DBlock *dblock, uint64_t persist_code) {
    if (persist_code <= dblock->persistence_code) {
        return;
    }
    dblock->persistence_code = persist_code;

    if (dblock->child != NULL) {
        tactyk_dblock__set_persistence_code(dblock->child, persist_code);
    }
    if (dblock->next != NULL) {
        tactyk_dblock__set_persistence_code(dblock->next, persist_code);
    }
    if (dblock->token != NULL) {
        tactyk_dblock__set_persistence_code(dblock->token, persist_code);
    }
    if (dblock->store != NULL) {
        tactyk_dblock__set_persistence_code(dblock->store, persist_code);
    }
    if (dblock->type == tactyk_dblock__TABLE) {
        struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**) dblock->data;
        for (uint64_t i = 0; i < dblock->element_capacity; i += 1) {
            uint64_t ofs = i*2;
            struct tactyk_dblock__DBlock *key = fields[ofs];
            if ((key != NULL) && (key->length != 0)) {
                tactyk_dblock__set_persistence_code(key, persist_code);
            }
        }
    }
}
void tactyk_dblock__cull(uint64_t persist_code) {
    for (uint64_t i = 0; i < DBLOCKS_CAPACITY; i += 1) {
        struct tactyk_dblock__DBlock *db = &dblocks[i];
        if (db->persistence_code == persist_code) {
            tactyk_dblock__weak_dispose(db);
        }
    }
}


// set the "fixed" flag.
// This is only a prohibition on resizing the dblock internal buffer (or a promise not to resize or reposition or free a backing buffer)
// This is used for formatted/heirarchical text and for maintaining heap-allocated data structures.
void tactyk_dblock__fix(struct tactyk_dblock__DBlock *dblock) {
    dblock->fixed = true;
}

// clear the fixed flag
// If the block references an external/backing buffer, this duplicates the portion of the backing buffer it references (to make it mutable).
void tactyk_dblock__break(struct tactyk_dblock__DBlock *dblock) {
    if (dblock->fixed) {
        dblock->fixed = false;
        if (!dblock->self_managed) {
            dblock->self_managed = true;
            uint8_t* ndata = tactyk_alloc__allocate(dblock->capacity, 1);
            memcpy(ndata, dblock->data, dblock->length);
            dblock->data = ndata;
        }
    }
}
void tactyk_dblock__make_pseudosafe(struct tactyk_dblock__DBlock *dblock) {
    assert(dblock->self_managed == true);
    dblock->self_managed = false;
    dblock->fixed = true;
}
void tactyk_dblock__clear(struct tactyk_dblock__DBlock *dblock) {
    assert(dblock->self_managed == true);
    memset(dblock->data, 0, dblock->capacity);
    dblock->element_count = 0;
    dblock->length = 0;
}

int64_t tactyk_dblock__count_peers(struct tactyk_dblock__DBlock *dblock) {
    int64_t ct = 0;
    while (dblock != NULL) {
        ct += 1;
        dblock = dblock->next;
    }
    return ct;
}
int64_t tactyk_dblock__count_children(struct tactyk_dblock__DBlock *dblock) {
    dblock = dblock->child;
    int64_t ct = 0;
    while (dblock != NULL) {
        ct += 1;
        dblock = dblock->next;
    }
    return ct;
}
int64_t tactyk_dblock__count_tokens(struct tactyk_dblock__DBlock *dblock) {
    dblock = dblock->token;
    int64_t ct = 0;
    while (dblock != NULL) {
        ct += 1;
        dblock = dblock->next;
    }
    return ct;
}

struct tactyk_dblock__DBlock* tactyk_dblock__last_peer(struct tactyk_dblock__DBlock *dblock) {
    if (dblock == NULL) {
        return NULL;
    }
    while (dblock->next != NULL) {
        dblock = dblock->next;
    }
    return dblock;
}

struct tactyk_dblock__DBlock* tactyk_dblock__last_token(struct tactyk_dblock__DBlock *dblock) {
    return tactyk_dblock__last_peer(dblock->token);
}
struct tactyk_dblock__DBlock* tactyk_dblock__last_child(struct tactyk_dblock__DBlock *dblock) {
    return tactyk_dblock__last_peer(dblock->child);
}

struct tactyk_dblock__DBlock* tactyk_dblock__substring(struct tactyk_dblock__DBlock *out, struct tactyk_dblock__DBlock *dblock, uint64_t start, uint64_t amount) {
    //printf("substring out=%p dblock=%p si=%ju len=%ju\n", out, dblock, start, amount);
    if (start >= dblock->length) {
        return out;
    }
    if ( (start + amount) > dblock->length) {
        amount = dblock->length-start;
    }
    return tactyk_dblock__from_bytes(out, dblock->data, start, amount, true);
}
struct tactyk_dblock__DBlock* tactyk_dblock__concat(struct tactyk_dblock__DBlock *out, void *a, void *b) {
    struct tactyk_dblock__DBlock *dblock_a = tactyk_dblock__from_string_or_dblock(a);
    struct tactyk_dblock__DBlock *dblock_b = tactyk_dblock__from_string_or_dblock(b);
    uint64_t len = dblock_a->length + dblock_b->length;
    if (out == NULL) {
        out = tactyk_dblock__new(len);
    }
    else {
        tactyk_dblock__reallocate(out, len);
    }
    uint8_t *data = (uint8_t*) out->data;
    uint8_t *data_a = (uint8_t*) dblock_a->data;
    uint8_t *data_b = (uint8_t*) dblock_b->data;
    memcpy(&data[0],                &data_a[0], dblock_a->length);
    memcpy(&data[dblock_a->length], &data_b[0], dblock_b->length);
    out->length = dblock_a->length + dblock_b->length;
    tactyk_dblock__update_hash(out);
    if (a != dblock_a) {
        tactyk_dblock__dispose(dblock_a);
    }
    if (b != dblock_b) {
        tactyk_dblock__dispose(dblock_b);
    }
    return out;
}

void tactyk_dblock__append(struct tactyk_dblock__DBlock *out, void *b) {
    assert(out->fixed == false);
    struct tactyk_dblock__DBlock *dblock_b = tactyk_dblock__from_string_or_dblock(b);
    //printf("APPEND-init [%ju %ju]", out->length, dblock_b->length);
    //tactyk_dblock__println(out);

    //printf("this thing: ");
    //tactyk_dblock__println(dblock_b);
    //printf("append: %p\n", out);
    int64_t tlen = out->length + dblock_b->length;
    tactyk_dblock__expand(out, tlen);

    uint8_t *data_a = (uint8_t*) out->data;
    uint8_t *data_b = (uint8_t*) dblock_b->data;

    memcpy(&data_a[out->length], &data_b[0], dblock_b->length);
    out->length = tlen;
    tactyk_dblock__update_hash(out);

    if (b != dblock_b) {
        tactyk_dblock__dispose(dblock_b);
    }
    //printf("APPEND-done [%ju]", out->length);
    //tactyk_dblock__println(out);
}

void tactyk_dblock__append_char(struct tactyk_dblock__DBlock *dblock, uint8_t ch) {
    assert(dblock->fixed == false);
    int64_t tlen = dblock->length + 1;
    tactyk_dblock__expand(dblock, tlen);
    uint8_t *data = (uint8_t*) dblock->data;
    data[dblock->length] = ch;
    dblock->length += 1;
    dblock->length = tlen;
    tactyk_dblock__update_hash(dblock);
}
void tactyk_dblock__append_byte(struct tactyk_dblock__DBlock *dblock, uint8_t val) {
    tactyk_dblock__append_char(dblock, val);
}
void tactyk_dblock__append_word(struct tactyk_dblock__DBlock *dblock, uint16_t val) {
    assert(dblock->fixed == false);
    int64_t tlen = dblock->length + 2;
    tactyk_dblock__expand(dblock, tlen);
    *(uint16_t*)(dblock->data+dblock->length) = val;
    dblock->length += 1;
    dblock->length = tlen;
    tactyk_dblock__update_hash(dblock);
}
void tactyk_dblock__append_dword(struct tactyk_dblock__DBlock *dblock, uint32_t val) {
    assert(dblock->fixed == false);
    int64_t tlen = dblock->length + 4;
    tactyk_dblock__expand(dblock, tlen);
    *(uint32_t*)(dblock->data+dblock->length) = val;
    dblock->length += 1;
    dblock->length = tlen;
    tactyk_dblock__update_hash(dblock);
}
void tactyk_dblock__append_qword(struct tactyk_dblock__DBlock *dblock, uint64_t val) {
    assert(dblock->fixed == false);
    int64_t tlen = dblock->length + 8;
    tactyk_dblock__expand(dblock, tlen);
    *(uint64_t*)(dblock->data+dblock->length) = val;
    dblock->length += 1;
    dblock->length = tlen;
    tactyk_dblock__update_hash(dblock);
}

void tactyk_dblock__append_substring(struct tactyk_dblock__DBlock *dblock_a, struct tactyk_dblock__DBlock *dblock_b, uint64_t start, uint64_t amount) {
    assert(dblock_a->fixed == false);
    if (start >= dblock_b->length) {
        return;
    }
    if ( (start + amount) > dblock_b->length) {
        amount = dblock_b->length-start;
    }
    uint8_t *data_b = (uint8_t*) dblock_b->data;
    int64_t tlen = dblock_a->length + amount;
    tactyk_dblock__expand(dblock_a, tlen);
    uint8_t *data_a = (uint8_t*) dblock_a->data;
    memcpy(&data_a[dblock_a->length], &data_b[start], amount);
    dblock_a->length = tlen;
    tactyk_dblock__update_hash(dblock_a);
}

bool tactyk_dblock__try_parseint(int64_t *out, struct tactyk_dblock__DBlock *dblock) {
    if (dblock->length == 0) {
        return false;
    }

    int64_t result = 0;
    int64_t sign = 1;
    int64_t i = 0;
    uint64_t radix = 10;
    char *str = (char*)dblock->data;
    switch(str[0]) {
        case '-': {
            sign = -1;
            i += 1;
            break;
        }
        case '+': {
            i += 1;
            break;
        }
    }
    switch(str[i]) {
        case 'H':
        case 'h': {
            if (str[i+1] != '.') {
                return false;
            }
            i += 2;
            radix = 16;
            break;
        }
        case 'B':
        case 'b': {
            if (str[i+1] != '.') {
                return false;
            }
            i += 2;
            radix = 2;
            break;
        }
    }
    for (; i < (int64_t)dblock->length; i++) {
        char c = str[i];
        uint64_t v = 0;
        switch(c) {
            case '0': { v=0; break; }
            case '1': { v=1; break; }
            case '2': { v=2; break; }
            case '3': { v=3; break; }
            case '4': { v=4; break; }
            case '5': { v=5; break; }
            case '6': { v=6; break; }
            case '7': { v=7; break; }
            case '8': { v=8; break; }
            case '9': { v=9; break; }

            case 'a': { v=10; break; }
            case 'b': { v=11; break; }
            case 'c': { v=12; break; }
            case 'd': { v=13; break; }
            case 'e': { v=14; break; }
            case 'f': { v=15; break; }

            case 'A': { v=10; break; }
            case 'B': { v=11; break; }
            case 'C': { v=12; break; }
            case 'D': { v=13; break; }
            case 'E': { v=14; break; }
            case 'F': { v=15; break; }
            case '_': { continue; }
            default: { return false; }
        }
        if (v >= radix) {
            return false;
        }
        result *= radix;
        result += v;
    }
    *out = result * sign;
    return true;

}
bool tactyk_dblock__try_parseuint(uint64_t *out, struct tactyk_dblock__DBlock *dblock) {

    if (dblock->length == 0) {
        return false;
    }
    int64_t result = 0;
    int64_t sign = 1;
    int64_t i = 0;
    uint64_t radix = 10;
    char *str = (char*)dblock->data;
    switch(str[0]) {
        case '+': {
            i += 1;
            break;
        }
    }
    switch(str[i]) {
        case 'H':
        case 'h': {
            if (str[i+1] != '.') {
                return false;
            }
            i += 2;
            radix = 16;
            break;
        }
        case 'B':
        case 'b': {
            if (str[i+1] != '.') {
                return false;
            }
            i += 2;
            radix = 2;
            break;
        }
    }
    for (; i < (int64_t)dblock->length; i++) {
        char c = str[i];
        uint64_t v = 0;
        switch(c) {
            case '0': { v=0; break; }
            case '1': { v=1; break; }
            case '2': { v=2; break; }
            case '3': { v=3; break; }
            case '4': { v=4; break; }
            case '5': { v=5; break; }
            case '6': { v=6; break; }
            case '7': { v=7; break; }
            case '8': { v=8; break; }
            case '9': { v=9; break; }

            case 'a': { v=10; break; }
            case 'b': { v=11; break; }
            case 'c': { v=12; break; }
            case 'd': { v=13; break; }
            case 'e': { v=14; break; }
            case 'f': { v=15; break; }

            case 'A': { v=10; break; }
            case 'B': { v=11; break; }
            case 'C': { v=12; break; }
            case 'D': { v=13; break; }
            case 'E': { v=14; break; }
            case 'F': { v=15; break; }
            case '_': { continue; }
            default: { return false; }
        }
        if (v >= radix) {
            return false;
        }
        result *= radix;
        result += v;
    }

    *out = result;
    return true;
}
bool tactyk_dblock__try_parsedouble(double *out, struct tactyk_dblock__DBlock *dblock) {
    if (dblock->length == 0) {
        return false;
    }
    char *buf = tactyk_alloc__allocate(dblock->length+1, 1);
    memcpy(buf, dblock->data, dblock->length);
    bool result = tactyk_util__try_parsedouble(out, buf, dblock->length);
    tactyk_alloc__free(buf);
    return result;
}

bool tactyk_dblock__try_parselongdouble(long double *out, struct tactyk_dblock__DBlock *dblock) {
    if (dblock->length == 0) {
        return false;
    }
    char *buf = tactyk_alloc__allocate(dblock->length+1, 1);
    memcpy(buf, dblock->data, dblock->length);
    bool result = tactyk_util__try_parselongdouble(out, buf, dblock->length);
    tactyk_alloc__free(buf);
    return result;
}

// resize the dblock.  If the dblock uses a backing buffer, this also copies the buffer and decouples from it.
void tactyk_dblock__expand(struct tactyk_dblock__DBlock *dblock, uint64_t min_length) {
    assert(dblock->fixed == false);
    bool was_self_managed = dblock->self_managed;
    if (dblock->self_managed) {
        if (dblock->capacity >= min_length) {
            return;
        }
    }
    else {
        dblock->self_managed = true;
    }
    if (dblock->capacity == 0) {
        dblock->capacity = min_length;
    }
    while (dblock->capacity < min_length) {
        dblock->capacity *= 2;
    }

    uint8_t *data = (uint8_t*) dblock->data;
    uint8_t *ndata = tactyk_alloc__allocate(dblock->capacity, 1);
    memcpy(ndata, &data[0], dblock->length);
    if (was_self_managed == true) {
        tactyk_alloc__free(data);
    }
    dblock->data = ndata;
}

void tactyk_dblock__reallocate(struct tactyk_dblock__DBlock *dblock, uint64_t min_length) {
    assert(dblock->self_managed == true);
    if (dblock->capacity < min_length) {
        while (dblock->capacity < min_length) {
            dblock->capacity *= 2;
            dblock->element_capacity *= 2;
        }
        uint8_t *ndata = tactyk_alloc__allocate(dblock->capacity, 1);
        tactyk_alloc__free(dblock->data);

        dblock->self_managed = true;
        dblock->data = ndata;
    }
    else {
        memset(dblock->data, 0, dblock->capacity);
    }
    dblock->length = 0;
    dblock->element_count = 0;
}

//  break a delimited dblock into parts.
//  This assumes that the data buffer stores text (no specific prevention in mind, other than dont do it)
//  This assumes that nothing will attempt to modify any dblock within the heirarchy of dblocks tokenization is performed on (probably should disable concat and expand)
//
//  "tokens" mode ON is intended for splitting space-delimited words
//  "tokens" mode OFF is intended for splitting newline-delimited lines.
//      This form of code reuse might be considered an "elegant" kludge.
void tactyk_dblock__tokenize(struct tactyk_dblock__DBlock *dblock, uint8_t separator, bool tokens) {
    assert(dblock->fixed == true);
    if (dblock->child != NULL) {
        tactyk_dblock__tokenize(dblock->child, separator, tokens);
    }
    if (dblock->next != NULL) {
        tactyk_dblock__tokenize(dblock->next, separator, tokens);
    }
    struct tactyk_dblock__DBlock *primary_dblock = dblock;
    struct tactyk_dblock__DBlock *db = NULL;
    struct tactyk_dblock__DBlock *next_db = NULL;
    uint8_t *ptr = dblock->data;
    uint8_t *next_ptr = NULL;
    uint64_t ln = dblock->length;
    int64_t next_ln = 0;
    uint64_t pos = 0;
    uint64_t next_pos = 0;

    if (tokens) {
        primary_dblock = tactyk_dblock__from_bytes(NULL, ptr, 0, ln, true);
        tactyk_dblock__fix(primary_dblock);
        dblock->token = primary_dblock;
    }

    while (pos < ln) {

        // find the next separator
        next_ptr = memchr(&ptr[pos], separator, ln-pos);

        if (next_ptr == NULL) {

            // end is reached, but no separators have been found (or only leading separators)
            if (db == NULL) {
                break;
            }

            // end is reached and there are still chars remaining - add one dblock for the end
            if (pos < ln) {
                next_db = tactyk_dblock__from_bytes(NULL, ptr, pos, ln-pos, true);
                tactyk_dblock__fix(next_db);
                db->next = next_db;
                break;
            }

            // end is reached, but the preceding char was a separator
            else {
                break;
            }
        }
        else {
            next_pos = next_ptr - ptr;
            if (db == NULL) {
                next_ln = next_pos - pos;

                // if the first token was separated, end the input dblock at the separator position
                if (next_ln > 0) {
                    primary_dblock->length = next_pos-pos;
                    tactyk_dblock__update_hash(primary_dblock);
                    db = primary_dblock;
                }

                // if only separators thus far, move the input dblock forward.  (remove useless leading separators)
                else {
                    primary_dblock->data = &ptr[next_pos+1];
                    primary_dblock->length = ln-next_pos;
                    tactyk_dblock__update_hash(primary_dblock);
                }
                pos = next_pos + 1;
            }
            else {
                next_ln = next_pos - pos;

                // if a preceding token and there is at least one non-separator char, append a new dblock (common case)
                if (next_ln > 0) {
                    next_db = tactyk_dblock__from_bytes(NULL, ptr, pos, next_pos-pos, true);
                    tactyk_dblock__fix(next_db);
                    db->next = next_db;
                    db = next_db;
                }
                // otherwise, ignore the repeated separator and advance
                pos = next_pos + 1;
            }
        }
    }
}

struct tactyk_dblock__DBlock* tactyk_dblock__remove_blanks(struct tactyk_dblock__DBlock *dblock, uint8_t space, uint8_t comment_indicator) {
    assert(dblock->fixed == true);
    struct tactyk_dblock__DBlock *valid_dblock = NULL;
    struct tactyk_dblock__DBlock *current_dblock = NULL;
    struct tactyk_dblock__DBlock *next_dblock = NULL;
    //printf("rem-blanks: ");
    //tactyk_dblock__println(dblock);
    while (dblock != NULL) {
        char *text = (char*)dblock->data;
        for (int64_t i = 0; i < (int64_t)dblock->length; i++) {
            char ch = text[i];
            //printf("test '%c'\n", ch);
            if (ch == space) {
                continue;
            }
            if (ch == 0) {
                //printf("1 ");
                goto rejectit;
            }
            if (ch == comment_indicator) {
                //printf("r i=%jd '%c'\n", i, ch);
                //printf("2 ");
                goto rejectit;
            }
            if (ch != comment_indicator){
                //printf("r i=%jd '%c'   %s\n", i, ch, tactyk_dblock__to_cstring(dblock));
                goto acceptit;
            }
        }
        //printf("0 ");
        rejectit:
        //printf("REJECT:  ");
        //tactyk_dblock__println(dblock);

        next_dblock = dblock->next;
        dblock->next = NULL;
        tactyk_dblock__dispose(dblock);
        dblock = next_dblock;

        //printf("nxt:");
        //tactyk_dblock__println(dblock);
        continue;

        acceptit:
        if (valid_dblock == NULL) {
            valid_dblock = dblock;
        }
        if (current_dblock != NULL) {
            current_dblock->next = dblock;
        }
        current_dblock = dblock;
        next_dblock = dblock->next;
        dblock->next = NULL;
        dblock = next_dblock;

    }
    return valid_dblock;
}

// scan sibling indentation levels and adjust to a heirarchical arrangement
void tactyk_dblock__stratify(struct tactyk_dblock__DBlock *dblock, uint8_t space) {
    assert(dblock->fixed == true);
    struct tactyk_dblock__DBlock *db_stack[256];
    int64_t stack_index = 0;
    uint8_t *data = (uint8_t*)dblock->data;
    for (int64_t i = 0; i < (int64_t)dblock->length; i++) {
        if (data[i] == space) {
            stack_index += 1;
        }
        else {
            break;
        }
    }

    db_stack[stack_index] = dblock;
    struct tactyk_dblock__DBlock *dblock_outer = dblock;           //
    dblock = dblock->next;                                         // dblock to evaluate for placement in the heirarchy
    struct tactyk_dblock__DBlock *dblock_next2 = dblock->next;     // temp variable
    dblock_outer->next = NULL;

    while (dblock != NULL) {
        data = (uint8_t*)dblock->data;
        dblock_next2 = dblock->next;
        dblock->next = NULL;

        // obtain stack index from indentation level.
        int64_t next_stack_index = 0;
        for (int64_t i = 0; i < (int64_t)dblock->length; i++) {
            if (data[i] == space) {
                next_stack_index += 1;
            }
            else {
                break;
            }
        }

        // if too deep, unwind the stack
        if (next_stack_index <= stack_index) {
            while (next_stack_index < stack_index) {
                db_stack[stack_index] = NULL;
                stack_index -= 1;
            }
                //  If no peer at the correct indentation level, the program is invalid and must be rejected for an indentation error.
              //<--Example:  This comment would trigger an invalid indentation level error
            struct tactyk_dblock__DBlock *peer = db_stack[stack_index];
            if (peer == NULL) {
                printf("ERROR:  Invalid indentation level.\n");
                exit(1);
            }
            peer->next = dblock;
            db_stack[next_stack_index] = dblock;
        }
        else {

            assert(dblock_outer->child == NULL);
            assert(dblock_outer == db_stack[stack_index]);

            dblock_outer->child = dblock;
            stack_index = next_stack_index;
            db_stack[next_stack_index] = dblock;
        }

        // the "parent" can only ever be immediately preceding dblock
        dblock_outer = dblock;
        dblock = dblock_next2;
    }
}

void tactyk_dblock__trim(struct tactyk_dblock__DBlock *dblock) {
    assert(dblock->fixed == true);

    int64_t start_pos = 0;
    int64_t end_pos = dblock->length;
    uint8_t *data = (uint8_t*)dblock->data;

    for (int64_t i = 0; i < (int64_t)dblock->length; i++) {
        char ch = data[i];
        if (ch == 0) {
            return;
        }
        else if ((ch != ' ') && (ch != '\t')) {
            start_pos = i;
            break;
        }
    }

    for (int64_t i = dblock->length-1; i > start_pos; i--) {
        char ch = data[i];

        if ((ch != ' ') && (ch != '\t')) {
            end_pos = i+1;
            break;
        }
    }
    if ((start_pos != 0) || (end_pos != (int64_t)dblock->length)) {
        dblock->data = &data[start_pos];
        dblock->length = end_pos - start_pos;
        tactyk_dblock__update_hash(dblock);
    }

    if (dblock->child != NULL) {
        tactyk_dblock__trim(dblock->child);
    }
    if (dblock->next != NULL) {
        tactyk_dblock__trim(dblock->next);
    }
}

void tactyk_dblock__update_hash(struct tactyk_dblock__DBlock *dblock) {
    uint64_t hash = 0;
    // logically split the data buffer into two segments:  a 64-bit section and an 8-bit residue section.
    uint64_t i8lim = (dblock->length) & 0x7;
    uint64_t i64lim = (dblock->length) - i8lim;
    uint64_t *data64 = &((uint64_t*)dblock->data)[0];
    uint8_t *data8 = &((uint8_t*) dblock->data)[i64lim];
    i64lim >>= 3;

    for (uint64_t i = 0; i < i64lim; i += 1) {
        uint64_t h_m = hash * 0x0123456789abcdef;
        uint64_t dat = data64[i];
        hash += 1;
        dat ^= (dat << 11);
        dat ^= (dat << 17);
        dat ^= (dat << 43);
        hash ^= h_m;
        hash ^= dat;
    }

    for (uint64_t i = 0; i < i8lim; i += 1) {
        uint64_t h_m = hash * 0x0123456789abcdef;
        uint64_t dat = data8[i];
        hash += 1;
        dat ^= (dat << 11);
        dat ^= (dat << 17);
        dat ^= (dat << 43);
        hash ^= h_m;
        hash ^= dat;
    }

    dblock->hashcode = hash;
}

// strict eauality test
bool tactyk_dblock__equals(struct tactyk_dblock__DBlock *dblock_a, struct tactyk_dblock__DBlock *dblock_b) {
    if (dblock_a->hashcode != dblock_b->hashcode) {
        //printf("reject-hash\n");
        return false;
    }
    uint64_t len = dblock_a->length;
    if (len != dblock_b->length) {
        //printf("reject-len\n");
        return false;
    }

    uint64_t i8lim = len & 0x7;
    uint64_t i64lim = (len - i8lim)>>3;

    //printf("len=%ju\n", len);

    uint64_t *data64_a = &((uint64_t*)dblock_a->data)[0];
    uint8_t *data8_a =   &((uint8_t*)dblock_a->data)[i64lim];
    uint64_t *data64_b = &((uint64_t*)dblock_b->data)[0];
    uint8_t *data8_b =   &((uint8_t*)dblock_b->data)[i64lim];


    for (uint64_t i = 0; i < i64lim; i += 1) {
        if (data64_a[i] != data64_b[i]) {
            //printf("reject-dev64 [%jd]\n", i);
            return false;
        }
    }

    for (uint64_t i = 0; i < i8lim; i += 1) {
        if (data8_a[i] != data8_b[i]) {
            //printf("reject-8  [%jd]\n", i);
            return false;
        }
    }
    //printf("accept\n");
    return true;
}

bool tactyk_dblock__equals_c_string(struct tactyk_dblock__DBlock *dblock_a, char *b) {
    if (dblock_a == NULL) {
        return false;
    }
    return (strncmp(dblock_a->data, b, dblock_a->length) == 0);
}

bool tactyk_dblock__contains(struct tactyk_dblock__DBlock *dblock_a, struct tactyk_dblock__DBlock *dblock_b) {
    int64_t limit = dblock_a->length-dblock_b->length;

    if (limit < 0) {
        return false;
    }

    uint8_t *data_a = (uint8_t*)dblock_a->data;
    uint8_t *data_b = (uint8_t*)dblock_b->data;
    for (uint64_t pos = 0; pos <= (uint64_t)limit; pos += 1) {
        for (uint64_t i = 0; i < dblock_b->length; i += 1) {
            if (data_a[pos+i] != data_b[i]) {
                goto scan_next_position;
            }
        }
        return true;
        scan_next_position:;
    }
    return false;
}

bool tactyk_dblock__contains_char(struct tactyk_dblock__DBlock *dblock_a, char c) {
    char *buf = (char*) dblock_a->data;
    for (uint64_t i = 0; i < dblock_a->length; i++) {
        if (buf[i] == c) {
            return true;
        }
    }
    return false;
}

char printbuf[256];
char tactyk_dblock__print_indented(FILE *stream, struct tactyk_dblock__DBlock *dblock, char *indent) {
    if (dblock == NULL) {
        printf("%s<NULL>", indent);
        return 0;
    }
    uint64_t len = dblock->length;
    if (len == 0) {
        return 0;
    }
    if (len < 256) {
        memcpy(printbuf, dblock->data, len);
        printbuf[len] = 0;
        fprintf(stream, "%s%s", indent, printbuf);
        return printbuf[len-1];
    }
    else {
        char *cpy = tactyk_alloc__allocate(dblock->length+1,1);
        //printf("?printit? %ju\n", len);
        memcpy(cpy, dblock->data, len);
        cpy[len] = 0;
        fprintf(stream, "%s%s", indent, cpy);
        char last = cpy[len-1];
        tactyk_alloc__free(cpy);
        return last;
    }
}
void tactyk_dblock__fprint(FILE *stream, void *ptr) {
    if ((ptr == NULL) || (ptr == TACTYK_PSEUDONULL))  {
        fprintf(stream, "[[ NULL ]]");
    }
    else {
        struct tactyk_dblock__DBlock *dblock = tactyk_dblock__from_string_or_dblock(ptr);
        tactyk_dblock__print_indented(stream, dblock, "");
    }
}
void tactyk_dblock__fprintln(FILE *stream, void *ptr) {
    if ((ptr == NULL) || (ptr == TACTYK_PSEUDONULL))  {
        fprintf(stream, "[[ NULL ]]\n");
    }
    else {
        struct tactyk_dblock__DBlock *dblock = tactyk_dblock__from_string_or_dblock(ptr);
        if (tactyk_dblock__print_indented(stream, dblock, "") != '\n') {
            fputc('\n', stream);
        }
    }
}

void tactyk_dblock__print(void *ptr) {
    tactyk_dblock__fprint(stdout, ptr);
}
void tactyk_dblock__println(void *ptr) {
    tactyk_dblock__fprintln(stdout, ptr);
}

void tactyk_dblock__print_structure_simple(struct tactyk_dblock__DBlock *dblock) {
    tactyk_dblock__fprint_structure(stdout, dblock, true, false, false, 0, '\n');
}
void tactyk_dblock__fprint_structure_simple(FILE *stream, struct tactyk_dblock__DBlock *dblock) {
    tactyk_dblock__fprint_structure(stream, dblock, true, false, false, 0, '\n');
}
void tactyk_dblock__print_structure(struct tactyk_dblock__DBlock *dblock, bool children, bool siblings, bool tokens, uint64_t indent_level, char sep) {
    tactyk_dblock__fprint_structure(stdout, dblock, children, siblings, tokens, indent_level, sep);
}
void tactyk_dblock__fprint_structure(FILE *stream, struct tactyk_dblock__DBlock *dblock, bool children, bool siblings, bool tokens, uint64_t indent_level, char sep) {
    if (indent_level > TACTYK_DBLOCK__PRINT_MAX_INDENT) {
        indent_level = TACTYK_DBLOCK__PRINT_MAX_INDENT;
    }

    char *indent = tactyk_alloc__allocate(indent_level*2+1,1);
    for (uint64_t i = 0; i < indent_level; i += 1) {
        indent[i*2+0] = ' ';
        indent[i*2+1] = ' ';
    }

    if (tactyk_dblock__print_indented(stream, dblock, indent) != sep) {
        fputc(sep, stream);
    }
    if (tokens) {
        struct tactyk_dblock__DBlock *t = dblock->token;
        while (t != NULL) {
            fputs(" | ", stream);
            tactyk_dblock__print(t);
            t = t->next;
        }
        fputs(" | ", stream);
    }

    if (children & (dblock->child != NULL)) {
        tactyk_dblock__fprint_structure(stream, dblock->child, true, true, tokens, indent_level+1, sep);
    }
    if (siblings & (dblock->next != NULL)) {
        tactyk_dblock__fprint_structure(stream, dblock->next, true, true, tokens, indent_level, sep);
    }
}

// Extract a c-string from a dblock and write it to a buffer.
// The c-string extraction stops at the first NULL char in the dblock buffer.
// This zero-fills any portions of the otuput buffer which the c-string does not cover.
// A null terminator is guaranteed.
//
// This was going to be a temporary solution, but there are a few points where C-strings are needed
void tactyk_dblock__export_cstring(char *out, uint64_t out_length, struct tactyk_dblock__DBlock *dblock) {
    memset(out, 0, out_length);
    void *ptr = memchr(dblock->data, 0, dblock->length);
    uint64_t amt;
    if (ptr == NULL) {
        amt = dblock->length;
    }
    else {
        amt = (uint64_t) (ptr - dblock->data);
        assert(amt <= dblock->length);
        if (amt >= (out_length-1)) {
            amt = out_length-1;
        }
    }
    strncpy(out, dblock->data, amt);
}

// a character getter which simulates the behavior of null-terminated strings (to allow c-string logic)
uint8_t tactyk_dblock__getchar(struct tactyk_dblock__DBlock *dblock, int64_t index) {
    if (index < 0) {
        return 0;
    }
    if (index >= (int64_t)dblock->capacity) {
        return 0;
    }
    uint8_t *data = (uint8_t*) dblock->data;
    return data[index];
}
uint8_t tactyk_dblock__lastchar(struct tactyk_dblock__DBlock *dblock) {
    if (dblock->length == 0) {
        return 0;
    }
    uint8_t *data = (uint8_t*) dblock->data;
    return data[dblock->length-1];
}

//
struct tactyk_dblock__DBlock* tactyk_dblock__new_container(uint64_t element_capacity, uint64_t stride) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    db->element_capacity = element_capacity;
    db->element_count = 0;
    db->length = 0;
    db->capacity = element_capacity*stride;
    db->data = tactyk_alloc__allocate(element_capacity, stride);
    db->self_managed = true;
    db->child = NULL;
    db->next = NULL;
    db->stride = stride;
    db->fixed = false;
    db->persistence_code = 0;
    return db;
}

struct tactyk_dblock__DBlock* tactyk_dblock__new_allocated_container(uint64_t capacity, uint64_t stride) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__new_container(capacity, stride);
    db->length = db->capacity;
    db->element_count = db->element_capacity;
    return db;
}

void* tactyk_dblock__new_object(struct tactyk_dblock__DBlock *container) {
    //assert(container->fixed == true);
    if ( (container->fixed == true) && (container->element_count >= container->element_capacity) ) {
        error("can not resize fixed-size container", NULL);
    }
    else if ( (container->fixed == false) && (container->element_count >= (container->element_capacity)) ) {
        container->element_capacity *= 2;
        tactyk_dblock__expand(container, container->element_capacity * container->stride);
    }
    uint8_t *data = (uint8_t*)container->data;
    uint8_t *ptr = &data[container->element_count * container->stride];
    container->length += container->stride;
    container->element_count += 1;
    if (container->length > container->capacity) {
        error("DBLOCK-CONTAINER capacity exceeded", NULL);
    }
    return ptr;
}

void* tactyk_dblock__index(struct tactyk_dblock__DBlock *container, uint64_t index) {
    if (container->store != NULL) {
        return tactyk_dblock__index(container->store, index);
    }
    if (index >= container->element_count) {
        error("DBLOCK-CONTAINER index out of bounds", NULL);
    }
    uint8_t *data = (uint8_t*)container->data;
    return &data[index * container->stride];
}
void* tactyk_dblock__index_allocated(struct tactyk_dblock__DBlock *container, uint64_t index) {
    if (container->store != NULL) {
        return tactyk_dblock__index(container->store, index);
    }
    if (index >= container->element_capacity) {
        error("DBLOCK-CONTAINER index out of bounds", NULL);
    }
    uint8_t *data = (uint8_t*)container->data;
    return &data[index * container->stride];
}

struct tactyk_dblock__DBlock* tactyk_dblock__interpolate(
    struct tactyk_dblock__DBlock *pattern, char var_indicator, bool *identifier_chars, struct tactyk_dblock__DBlock *main_vars, struct tactyk_dblock__DBlock *alt_vars) {
    struct tactyk_dblock__DBlock *result = tactyk_dblock__new(16);
    struct tactyk_dblock__DBlock *varname = tactyk_dblock__new(16);
    int64_t pos = 0;
    int64_t len = (int64_t)pattern->length;

    uint8_t *text = (uint8_t*)pattern->data;

    // copy text to the output buffer until '$' is reached
    scan_raw:
    while (pos < len) {
        uint8_t ch = text[pos];
        if (ch == var_indicator) {
            goto scan_var;
        }
        tactyk_dblock__append_char(result, ch);
        pos += 1;
    }

    // When "scan_raw" exits normally, the interpolated string is complete.
    tactyk_dblock__dispose(varname);

    return result;

    // copy text to the variable-name buffer until an invalid identifier char is reached
    scan_var: {
        tactyk_dblock__clear(varname);
        uint8_t ch = text[pos];
        tactyk_dblock__append_char(varname, ch);
        pos += 1;
        while (pos < len) {
            ch = text[pos];
            if (identifier_chars[ch] == false) {
                goto write_var;
            }
            tactyk_dblock__append_char(varname, ch);
            pos += 1;
        }
        goto write_var;
    }

    // If the variable can be resolved, write the resolved value to the output buffer, otherwise, write the variable name.
    write_var: {
        struct tactyk_dblock__DBlock *varvalue = tactyk_dblock__get(main_vars, varname);
        if ( (varvalue == NULL) && (alt_vars != NULL) ) {
            varvalue = tactyk_dblock__get(alt_vars, varname);
        }
        if (varvalue != NULL) {
            tactyk_dblock__append(result, varvalue);
        }
        else {
            tactyk_dblock__append(result, varname);
        }
        goto scan_raw;
    }
}

struct tactyk_dblock__DBlock* tactyk_dblock__new_table(uint64_t element_capacity) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__alloc();
    db->type = tactyk_dblock__TABLE;
    uint64_t stride = sizeof(void*);
    element_capacity = tactyk_util__next_pow2(element_capacity);
    db->element_capacity = element_capacity;
    db->element_count = 0;
    db->length = 0;
    db->capacity = element_capacity*stride*2;
    db->data = tactyk_alloc__allocate(element_capacity, stride*2);
    db->self_managed = true;
    db->child = NULL;
    db->next = NULL;
    db->stride = stride;
    db->fixed = false;
    db->persistence_code = 0;
    return db;
}
struct tactyk_dblock__DBlock* tactyk_dblock__new_managedobject_table(uint64_t element_capacity, uint64_t stride) {
    struct tactyk_dblock__DBlock *db = tactyk_dblock__new_table(element_capacity);
    db->store = tactyk_dblock__new_container(element_capacity, stride);
    return db;
}

void tactyk_dblock__reset_table(struct tactyk_dblock__DBlock *table, bool overwrite) {
    void **fields = (void**) table->data;

    for (uint64_t i = 0; i < table->element_capacity; i += 2) {
        struct tactyk_dblock__DBlock *tbl_key = (struct tactyk_dblock__DBlock*) fields[i];
        if ( (tbl_key != NULL) && (tbl_key != TACTYK_PSEUDONULL) && (tbl_key->self_managed == true) ) {
            tactyk_dblock__dispose(tbl_key);
        }
    }

    table->element_count = 0;
    if (overwrite) {
        memset(table->data, 0, table->capacity);
    }
}

int64_t tactyk_dblock__table_find(struct tactyk_dblock__DBlock *table, struct tactyk_dblock__DBlock *key) {
    uint64_t hash = key->hashcode;
    uint64_t pref_index = hash & (table->element_capacity-1);
    //uint64_t stride = sizeof(void*);
    struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**)table->data;
    for (uint64_t i = pref_index; i < table->element_capacity; i += 1) {
        uint64_t ofs = i * 2;
        struct tactyk_dblock__DBlock *tbl_key = fields[ofs];
        struct tactyk_dblock__DBlock *tbl_value = fields[ofs+1];
        if (tbl_value == NULL) {
            return i;
        }
        if (tactyk_dblock__equals(key, tbl_key)) {
            return i;
        }
    }
    for (uint64_t i = 0; i < pref_index; i += 1) {
        uint64_t ofs = i * 2;
        struct tactyk_dblock__DBlock *tbl_key = fields[ofs];
        struct tactyk_dblock__DBlock *tbl_value = fields[ofs+1];
        if (tbl_value == NULL) {
            return i;
        }
        if (tactyk_dblock__equals(key, tbl_key)) {
            return i;
        }
    }
    return -1;
}


void tactyk_dblock__rebuild_table(struct tactyk_dblock__DBlock *table, uint64_t element_capacity) {
    assert(element_capacity >= table->element_capacity);
    uint64_t prev_element_capacity = table->element_capacity;
    table->element_capacity = element_capacity;
    struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**)table->data;

    uint64_t stride = sizeof(void*);
    uint64_t min_length = element_capacity*stride*2;

    if (table->capacity < min_length) {
        table->capacity = min_length;
        table->data = tactyk_alloc__allocate(min_length, 1);
    }
    table->element_count = 0;
    for (uint64_t i = 0; i < prev_element_capacity; i += 1) {
        //printf("rb... %ju\n", i);
        uint64_t ofs = i * 2;
        struct tactyk_dblock__DBlock *key = fields[ofs];
        struct tactyk_dblock__DBlock *value = fields[ofs+1];

        if ((value != NULL) && (value != TACTYK_PSEUDONULL)) {
            tactyk_dblock__put(table, key, value);
        }
    }
    tactyk_alloc__free(fields);
}


void tactyk_dblock__put(struct tactyk_dblock__DBlock *table, void *key, void *value) {
    if (tactyk_dblock__is_dblock(value)) {
        tactyk_dblock__set_persistence_code((struct tactyk_dblock__DBlock*)value, table->persistence_code);
    }
    assert(table->element_count <= table->element_capacity);

    if ((table->element_count) == table->element_capacity) {
        tactyk_dblock__rebuild_table(table, table->element_count*2);
    }

    struct tactyk_dblock__DBlock *tmp_key = tactyk_dblock__from_string_or_dblock(key);
    int64_t idx = tactyk_dblock__table_find(table, tmp_key);
    assert(idx != -1);

    void **fields = (void**)table->data;

    uint64_t offset = (uint64_t)idx*2;
    void *tbl_value = fields[offset+1];

    if ( (value == NULL) && (tbl_value == NULL) ) {
        //nothing.
    }
    else if ( (value != NULL) && (tbl_value == NULL) ) {
        // if using a temp key, make it persistent [by using it directly and not disposing it]
        if (tmp_key != key) {
            tactyk_dblock__set_persistence_code(tmp_key, table->persistence_code);
            fields[offset] = tmp_key;
            fields[offset+1] = value;
            table->element_count += 1;
            return;
        }

        // if using a dynamic key, put a copy into the table
        else if (tmp_key->self_managed) {
            struct tactyk_dblock__DBlock *cpy_key = tactyk_dblock__shallow_copy(tmp_key);
            tactyk_dblock__set_persistence_code(cpy_key, table->persistence_code);
            fields[offset] = cpy_key;
            fields[offset+1] = value;
            table->element_count += 1;
            return;
        }

        // if using a persistent key (externally managed), use the key directly.
        else {
            tactyk_dblock__set_persistence_code(tmp_key, table->persistence_code);
            fields[offset] = tmp_key;
            fields[offset+1] = value;
            table->element_count += 1;
            return;
        }
    }
    else if ( (value == NULL) && (tbl_value != NULL) ) {
        fields[offset+1] = TACTYK_PSEUDONULL;
    }
    else if ( (value != NULL) && (tbl_value != NULL) ) {
        fields[offset+1] = value;
    }

    if (tmp_key != key) {
        tactyk_dblock__dispose(tmp_key);
    }
}

void* tactyk_dblock__get(struct tactyk_dblock__DBlock *table, void *key) {
    struct tactyk_dblock__DBlock *tmp_key = tactyk_dblock__from_string_or_dblock(key);
    int64_t idx = tactyk_dblock__table_find(table, tmp_key);
    if (idx == -1) {
        if (tmp_key != key) {
            tactyk_dblock__dispose(tmp_key);
        }
        return NULL;
    }
    void **fields = (void**)table->data;

    uint64_t offset = (uint64_t)idx*2;
    struct tactyk_dblock__DBlock *tbl_key = fields[offset];
    void *tbl_value = fields[offset+1];

    if (tmp_key != key) {
        tactyk_dblock__dispose(tmp_key);
    }
    if (tbl_value != TACTYK_PSEUDONULL) {
        return tbl_value;
    }
    return NULL;
}

void* tactyk_dblock__new_managedobject(struct tactyk_dblock__DBlock *table, void* key) {
    void* item = tactyk_dblock__new_object(table->store);
    tactyk_dblock__put(table, key, item);
    return item;
}

void* tactyk_dblock__release(struct tactyk_dblock__DBlock *dblock) {
    if (dblock->store != NULL) {
         dblock->store->self_managed = false;
         void *data = dblock->store->data;
         dblock->store->data = NULL;
         tactyk_dblock__dispose(dblock);
         return data;
    }
    else {
        dblock->self_managed = false;
        void *data = dblock->data;
        dblock->data = NULL;
        tactyk_dblock__dispose(dblock);
        return data;

    }
}

void tactyk_dblock__delete(struct tactyk_dblock__DBlock *table, void *key) {
    tactyk_dblock__put(table, key, NULL);
}

// if ptr is a pointer to a dblock, return it.
// if not so, assume it is a c-string, copy it and return a dblock
struct tactyk_dblock__DBlock* tactyk_dblock__from_string_or_dblock(void *ptr) {
    if (tactyk_dblock__is_dblock(ptr)) {
        return (struct tactyk_dblock__DBlock*)ptr;
    }
    else {
        return tactyk_dblock__from_c_string((char*)ptr);
    }
}
