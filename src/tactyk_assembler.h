//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_ASSEMBLER__INCLUDE_GUARD
#define TACTYK_ASSEMBLER__INCLUDE_GUARD

#define ASM_INPUT_FILENAME "/tmp/tactyk_input.asm"
#define ASM_OUTPUT_FILENAME "/tmp/tactyk_output.o"
#define ASM_SYMBOLS_FILENAME "/tmp/tactyk_symbols.map"

#define MAX_ASM_COMPONENTS 16

#include <stdint.h>
#include <stdbool.h>

#include "tactyk_asmvm.h"
#include "tactyk_dblock.h"

struct tactyk_assembly {
    char *name;
    char *bin;
    uint64_t length;
    struct tactyk_dblock__DBlock *defs;
    struct tactyk_dblock__DBlock *labels;
};

bool readHex(char* text, int64_t *out);
struct tactyk_assembly* tactyk_assemble(char *asm_fname, char *obj_name, char *sym_fname);

#endif // TACTYK_ASSEMBLER__INCLUDE_GUARD
