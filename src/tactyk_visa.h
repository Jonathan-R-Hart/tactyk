//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifndef TACTYK_VISA__INCLUDE_GUARD
#define TACTYK_VISA__INCLUDE_GUARD

#include <stdbool.h>

#include "tactyk_emit.h"

void tactyk_visa__init(char *fname);
void tactyk_visa__init_emit(struct tactyk_emit__Context *ctx);

#endif // TACTYK_VISA__INCLUDE_GUARD
