//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_H
#define TACTYK_H

#define TACTYK__TITLE_SHORT "TACTYK"
#define TACTYK_SE__SUBTITLE_SHORT "SE"
#define TACTYK__TITLE_LONG "TACTYK (should) Affix Captions To Your Kitten"
#define TACTYK_SE__SUBTITLE "Scripting Engine"
#ifndef TACTYK__VERSION
    #define TACTYK__VERSION "9999"
#endif // TACTYK__VERSION

#define TACTYK_SE__DESCRIPTION (TACTYK__TITLE_LONG" - "TACTYK_SE__SUBTITLE" ["TACTYK__TITLE_SHORT"-"TACTYK_SE__SUBTITLE_SHORT"], VERSION "TACTYK__VERSION)

#include <stdint.h>

#include "tactyk_dblock.h"

typedef struct tactyk_dblock__DBlock* t_string;
typedef struct tactyk_dblock__DBlock* t_table;
typedef struct tactyk_dblock__DBlock* t_buffer;
typedef struct tactyk_dblock__DBlock* t_tree;

#define t_string__new(A) tactyk_dblock__from_c_string(A);

typedef void (*tactyk__error_handler)(char *message, void *data);

extern tactyk__error_handler error;
extern tactyk__error_handler warn;

uint64_t tactyk__rand_uint64();

#endif  // TACTYK_H

