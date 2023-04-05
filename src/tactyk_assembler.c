//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#include <string.h>

#include "tactyk_assembler.h"
#include "tactyk_dblock.h"

bool readHex(char* text, int64_t *out) {
    int64_t r = 0;
    int64_t len = strlen(text);
    for (int64_t i = 0; i < len; i++) {
        char c = text[i];
        r <<= 4;
        if ((c >= '0') && (c <= '9')) {
            r += (c-'0');
        }
        else if ((c >= 'A') && (c <= 'F')) {
            r += (c-'A'+10);
        }
        else if ((c >= 'a') && (c <= 'f')) {
            r += (c-'a'+10);
        }
        else {
            return false;
        }
    }
    *out = r;
    return true;
}


struct tactyk_assembly* tactyk_assemble(char *asm_fname, char *obj_name, char *sym_fname) {
//struct tactyk_assembly* tactyk_assemble() {
    struct tactyk_assembly *out = calloc(1, sizeof(struct tactyk_assembly));
    out->defs = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_asmvm__identifier));
    out->labels = tactyk_dblock__new_managedobject_table(1024, sizeof(struct tactyk_asmvm__identifier));
    //out->defs = tactyk_table__new(1024);
    //out->labels = tactyk_table__new(4096);
    char sys_cmd[256];
    sprintf(sys_cmd, "nasm -Ox -f bin %s -o %s", asm_fname, obj_name);
    //system("nasm -Ox -f bin "ASM_INPUT_FILENAME" -o "ASM_OUTPUT_FILENAME);
    system(sys_cmd);

    FILE *tf = fopen( obj_name, "rb" );

    if (tf == NULL) {
        printf("FAILED TO LOAD '%s'\n", obj_name);
    }
    else {
        fseek(tf, 0, SEEK_END);
        out->length = ftell(tf);
        fseek(tf, 0, SEEK_SET);
        out->bin = calloc(1, out->length);
        fread(out->bin, out->length, 1, tf);

        fclose(tf);
    }

    // THis is a rather nonsensical way to read the symbols from an assembly map file and should be replaced.
    //  There also does not seem to be a good reason to read ASM constants.
    tf = fopen( sym_fname, "r" );
    char line[1000];
    while (fgets(line, 999, tf) != NULL) {
        int64_t len = strlen(line);

        if (line[0] == '-') continue;
        if (len == 1) continue;

        int i = 0;
        int64_t hxv1 = 0;
        int64_t hxv2 = 0;
        char* token;

        int64_t pos = 0;
        //read_hexnum_1:
        for (; i < len; i++) {
            switch(line[i]) {
                case ' ': {
                    line[i] = 0;
                    if (line[pos] != 0) {
                        if (readHex(&line[pos], &hxv1)) {
                            pos = i+1;
                            i += 1;
                            goto read_hexnum_2;
                        }
                        else {
                            goto continue_outer;
                        }
                    }
                    else {
                        pos = i+1;
                    }
                }
            }
        }
        read_hexnum_2:
        for (; i < len; i++) {
            switch(line[i]) {
                case '\n':
                case ' ': {
                    line[i] = 0;
                    if (line[pos] != 0) {
                        if (readHex(&line[pos], &hxv2)) {
                            pos = i+1;
                            i += 1;
                            goto read_token;
                        }
                        else {
                            token = &line[pos];
                            struct tactyk_asmvm__identifier *id = tactyk_dblock__new_managedobject(out->defs,token);
                            strcpy(id->txt, token);
                            id->value = hxv1;
                            goto continue_outer;
                        }
                    }
                    else {
                        pos = i+1;
                    }
                }
            }
        }
        read_token:
        for (; i < len; i++) {
            switch(line[i]) {
                case '\n':
                case ' ': {
                    line[i] = 0;
                    if (line[pos] != 0) {
                        token = &line[pos];
                        struct tactyk_asmvm__identifier *id = tactyk_dblock__new_managedobject(out->labels,token);
                        strcpy(id->txt, token);
                        id->value = hxv1;
                        goto continue_outer;
                    }
                    else {
                        pos = i+1;
                    }
                }
            }
        }
        continue_outer:;
    }

    return out;
}
