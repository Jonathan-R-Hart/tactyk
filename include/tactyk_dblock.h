//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_DBLOCK__INCLUDE_GUARD
#define TACTYK_DBLOCK__INCLUDE_GUARD

#include <stdint.h>
#include <stdbool.h>

enum tactyk_dblock__Type {
    tactyk_dblock__ARRAY,
    tactyk_dblock__TABLE,
    tactyk_dblock__STRING,
    tactyk_dblock__EXTERNAL,
    tactyk_dblock__TREE
};

struct tactyk_dblock__DBlock {
    uint64_t type;
    uint64_t length;
    uint64_t stride;
    uint64_t element_capacity;
    uint64_t element_count;
    uint64_t capacity;
    uint64_t hashcode;
    void *ptr;
    void *data;
    uint64_t persistence;
    bool fixed;
    bool self_managed;
    struct tactyk_dblock__DBlock *next;
    struct tactyk_dblock__DBlock *child;
    struct tactyk_dblock__DBlock *token;
    struct tactyk_dblock__DBlock *store;
};

void tactyk_dblock__init();
void tactyk_dblock__testit();

// copy the dblock and its data buffer, mark it as self-managed.
struct tactyk_dblock__DBlock* tactyk_dblock__shallow_copy(struct tactyk_dblock__DBlock *src);
// recursively copy the dblock and all subordinate dblocks
struct tactyk_dblock__DBlock* tactyk_dblock__deep_copy(struct tactyk_dblock__DBlock *src);

// basic dblock constructors
struct tactyk_dblock__DBlock* tactyk_dblock__new(uint64_t capacity);
struct tactyk_dblock__DBlock* tactyk_dblock__from_ptr(void *ptr);
struct tactyk_dblock__DBlock* tactyk_dblock__from_bytes(struct tactyk_dblock__DBlock *out, uint8_t *data, uint64_t start_index, uint64_t length, bool is_safe_data);

// true if the input pointer references any memory location in the global dblock buffer (primitive type check)
bool tactyk_dblock__is_dblock(void *ptr);

// if ptr is within the global dblock buffer, return it, otherwise create a new dblock from a copy of the c-string.
struct tactyk_dblock__DBlock* tactyk_dblock__from_string_or_dblock(void *ptr);

// mark the dblock as having a fixed internal buffer size
void tactyk_dblock__fix(struct tactyk_dblock__DBlock *dblock);
// make the dblcok itnernal buffer resizeable again.
void tactyk_dblock__break(struct tactyk_dblock__DBlock *dblock);
// hide the dblock from the standard dispose function (pretend to have an externally managed buffer)
void tactyk_dblock__make_pseudosafe(struct tactyk_dblock__DBlock *dblock);
// set dblock length to zero (might be worthwhile to consider also wiping its memory)
void tactyk_dblock__clear(struct tactyk_dblock__DBlock *dblock);
// recalculate the dblock hashcode (intended only for strings).
void tactyk_dblock__update_hash(struct tactyk_dblock__DBlock *dblock);
// Increase the size of the internal data buffer.  If the dblock uses externally managed buffer, this also copies the buffer (decoupling it from the backing data).
void tactyk_dblock__expand(struct tactyk_dblock__DBlock *dblock, uint64_t min_length);
// clear and resize the internal data buffer.
void tactyk_dblock__reallocate(struct tactyk_dblock__DBlock *dblock, uint64_t min_length);
// clear and de-allocate buffers from this and all linked or associated dblocks.
void tactyk_dblock__dispose(struct tactyk_dblock__DBlock *dblock);
void tactyk_dblock__set_content(struct tactyk_dblock__DBlock *dest, struct tactyk_dblock__DBlock *source);

// count the number of adjacent dblocks (number of times the pointer chain, 'dblock->next->next-->...' can be followed before encountering a NULL pointer)
// the resulting sum includes itself.
int64_t tactyk_dblock__count_peers(struct tactyk_dblock__DBlock *dblock);
// count the number of "child" dblocks (number of times the pointer chain, '(dblock->child)->next->next-->...' can be followed before encountering a NULL pointer)
// the resulting sum does not include itself.
int64_t tactyk_dblock__count_children(struct tactyk_dblock__DBlock *dblock);
// count the number of "token" dblocks (number of times the pointer chain, '(dblock->token)->next->next-->...' can be followed before encountering a NULL pointer)
// the resulting sum does not include itself.
int64_t tactyk_dblock__count_tokens(struct tactyk_dblock__DBlock *dblock);

struct tactyk_dblock__DBlock* tactyk_dblock__last_peer(struct tactyk_dblock__DBlock *dblock);
struct tactyk_dblock__DBlock* tactyk_dblock__last_token(struct tactyk_dblock__DBlock *dblock);
struct tactyk_dblock__DBlock* tactyk_dblock__last_child(struct tactyk_dblock__DBlock *dblock);

// create a dblock from a c-string, and use that c-string as a backing buffer (externally-managed buffer)
struct tactyk_dblock__DBlock* tactyk_dblock__from_safe_c_string(char *data);
// create a dblock from a c-string, using a copy of the c-string for an internal data buffer (self-managed buffer)
struct tactyk_dblock__DBlock* tactyk_dblock__from_c_string(char *data);
// convert a signed integer to a string, then create a dblock from that string
struct tactyk_dblock__DBlock* tactyk_dblock__from_int(int64_t value);
// convert an unsigned integer to a string, then create a dblock from that string
struct tactyk_dblock__DBlock* tactyk_dblock__from_uint(uint64_t value);

// create a dblock from a substring taken from another dblock.
// The created dblock uses the source dblock as a backing buffer (externally managed)
struct tactyk_dblock__DBlock* tactyk_dblock__substring(struct tactyk_dblock__DBlock* out, struct tactyk_dblock__DBlock *dblock, uint64_t start, uint64_t amount);
// concatenate the content of two dblocks to create a combined dblock.
struct tactyk_dblock__DBlock* tactyk_dblock__concat(struct tactyk_dblock__DBlock* out, void *a, void *b);
// append the content of one dblock to another dblock
void tactyk_dblock__append(struct tactyk_dblock__DBlock *out, void *dblock);
// append a single byte to a dblock
void tactyk_dblock__append_char(struct tactyk_dblock__DBlock *dblock, uint8_t ch);
// append a substring of one dblock to another dblock
void tactyk_dblock__append_substring(struct tactyk_dblock__DBlock *out, struct tactyk_dblock__DBlock *dblock, uint64_t start, uint64_t amount);
// attempt to parse the dblock's content as a signed integer.  if it succeeds, export the result via pointer and return true, otherwise return false
bool tactyk_dblock__try_parseint(int64_t *out, struct tactyk_dblock__DBlock *dblock_a);
// attempt to parse the dblock's content as an unsigned integer.  if it succeeds, export the result via pointer and return true, otherwise return false
bool tactyk_dblock__try_parseuint(uint64_t *out, struct tactyk_dblock__DBlock *dblock_a);
// attempt to parse the dblock's content as floating point number.  if it succeeds, export the result via pointer and return true, otherwise return false
bool tactyk_dblock__try_parsedouble(double *out, struct tactyk_dblock__DBlock *dblock);

// Structured text parsing
// These functions are used to create data structures from formatted text using only plain delimiter chars and the "off-side" rule.

// Apply the "off-side" rule.
// This scans leading spaces of this dblock and all "siblinng dblocks (dblock->next->next-> ...)
// and rearranges them such that "nested" dblocks are referenced from the "child" property, and "sibling" dblocks by the "next" property
void tactyk_dblock__stratify(struct tactyk_dblock__DBlock *dblock, uint8_t space);
// Remove comments and blank lines
// This scans this dblock and all "siblinng dblocks (dblock->next->next-> ...) and removed any that contain no syntactically meaningful content
struct tactyk_dblock__DBlock* tactyk_dblock__remove_blanks(struct tactyk_dblock__DBlock *dblock, uint8_t space, uint8_t comment_indicator);
// Tokenize the dblock and all sibling and child dblocks.
// This scans dblock text content for all instnaces of a separator and splits it into a series of dblocks.
//  If the "token" flag is set, then the tokenization is "non-destructive" and will only create a series of tokens attached to the "token" property (and recursively by token->next)
//  If the "token" flag is not set, then the tokenization is "destrictuve" and the resulting dblocks will be attached recursively via the "next" property
void tactyk_dblock__tokenize(struct tactyk_dblock__DBlock *dblock, uint8_t separator, bool tokens);
// remove all leading and trailing spaces from the content od this dblock and all "sibling" dblocks.
void tactyk_dblock__trim(struct tactyk_dblock__DBlock *dblock);

// return true if the dblocks contain identical content
//  this compares each byte up to the dblock->length (any trailing data in the buffer is ignored) and returns true if all bytes match
//  If the dblocks have mismatched hashcodes then it will return false without comparing the buffers
bool tactyk_dblock__equals(struct tactyk_dblock__DBlock *dblock_a, struct tactyk_dblock__DBlock *dblock_b);

// seach a dblock's internal buffer for a copy of the content of another dblock and return true if the search succeeds.
bool tactyk_dblock__contains(struct tactyk_dblock__DBlock *dblock_a, struct tactyk_dblock__DBlock *dblock_b);

// debug output functions

// print the content of a dblock
void tactyk_dblock__print(void *ptr);
// print the content of a dblock, then print a newline char
void tactyk_dblock__println(void *ptr);
//print the content of a dblock, optionally recursively print content of sibling dblocks, and optionally recursively print the content of child dblocks, and optionally show the "tokens"
//  with the tokenization further emphasized
void tactyk_dblock__print_structure(struct tactyk_dblock__DBlock *dblock, bool children, bool siblings, bool tokens, uint64_t indent_level);
// shorthand for tactyk_dblock__print_structure(dblock, true, false, false)
void tactyk_dblock__print_structure_simple(struct tactyk_dblock__DBlock *dblock);

// copy the content of a dblock to a buffer.
// If the output buffer is larger than the dblock buffer, the extra space will be zero-filled.
// If the dblock buffer is larger than the output buffer, the extracted data will be truncated.
// This does ensure the presence of a null terminator.
void tactyk_dblock__export_cstring(char *buf, uint64_t buf_length, struct tactyk_dblock__DBlock *dblock);

// get a char from the dblock at a particular position.  If the position is out of bounds, this returns zero without indicating an error.
uint8_t tactyk_dblock__getchar(struct tactyk_dblock__DBlock *dblock, int64_t index);
// get last char from a dblock.
uint8_t tactyk_dblock__lastchar(struct tactyk_dblock__DBlock *dblock);

// dblock-container: a flat array of structs
// instantiate a dblock which holds an array of data structures.  stride specifies the size of each element.  Capacity specifies the initial size of the data set.
struct tactyk_dblock__DBlock* tactyk_dblock__new_container(uint64_t capacity, uint64_t stride);
// container with objects "pre-allocated"
struct tactyk_dblock__DBlock* tactyk_dblock__new_allocated_container(uint64_t capacity, uint64_t stride);
// return a pointer to the next uninitialized container entry.
void* tactyk_dblock__new_object(struct tactyk_dblock__DBlock *container);
// return a pointer to a specific container entry
void* tactyk_dblock__index(struct tactyk_dblock__DBlock *container, uint64_t index);

// Use string interpolation to transform a template into a product.
// "var_indicator" is a sigil char that indicates the presence of an identifier
// "identifier_chars" is a boolean array which indicates which chars are valid identifier chars
// "main_cars" and "alt_vars" are the data sources used to perform string interpolation with ("main_vars" is prioritized)
// The template is scanned from beginning to end.  When the identifier sigil is encountered, it reads all chars up to the first invalid char, then quieries the data sources
//  for a matching entry.  If such an entry is found, the value of the entry is copied into the output.  If no entry is found, the identifier is copied as-is into the output.
//  Anything not part of an identifier is also copied into the output.
struct tactyk_dblock__DBlock* tactyk_dblock__interpolate(
    struct tactyk_dblock__DBlock *tmpl, char var_indicator, bool *identifier_chars, struct tactyk_dblock__DBlock *main_vars, struct tactyk_dblock__DBlock *alt_vars);

// dblock-table: An associative array
// This uses a hashtable with linear probing to handle collisions.

struct tactyk_dblock__DBlock* tactyk_dblock__new_table(uint64_t capacity);
// zero-fill the table and reset entry count to zero.
void tactyk_dblock__reset_table(struct tactyk_dblock__DBlock *table, bool overwrite);
// Increase the size of the table, re-position all entries in the larger buffer, and removes any placeholders entries
void tactyk_dblock__rebuild_table(struct tactyk_dblock__DBlock *table, uint64_t capacity);
// insert a new table entry or update an existing table entry
void tactyk_dblock__put(struct tactyk_dblock__DBlock *table, void *key, void *value);
// fetch a table entry
void* tactyk_dblock__get(struct tactyk_dblock__DBlock *table, void *key);
//remove a table entry (replaces it with a placeholder)
void tactyk_dblock__delete(struct tactyk_dblock__DBlock *table, void *key);

// instantiate a new associative array, with a dblock-container as a backing store for the table.
// This is a convenient and consistant method for having data structures which are bound to a name and stored in a flat array.
struct tactyk_dblock__DBlock* tactyk_dblock__new_managedobject_table(uint64_t element_capacity, uint64_t stride);
// bind the next element to a name, create a table entry for it, and return a pointer to its ununitialized data.
void* tactyk_dblock__new_managedobject(struct tactyk_dblock__DBlock *table, void* key);

// detach a backing buffer from a dblock and dispose the dblock, then return the detached buffer
void* tactyk_dblock__release(struct tactyk_dblock__DBlock *dblock);

#endif // TACTYK_DBLOCK__INCLUDE_GUARD










