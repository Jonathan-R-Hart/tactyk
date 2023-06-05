#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "tactyk_report.h"
#include "tactyk_dblock.h"

char tactyk_report__buffer[TACTYK_REPORT__BUFSIZE];
FILE *rpt_stream;
char tactyk_report__indent_buf[16];
int64_t tactyk_report__indent_spaces;

void tactyk_report__init() {
    rpt_stream = fmemopen(tactyk_report__buffer, TACTYK_REPORT__BUFSIZE, "w");
    tactyk_report__indent(-16);
}

void tactyk_report__break() {
    fputc('\n', rpt_stream);
}

void tactyk_report__indent(int64_t amount) {
    tactyk_report__indent_spaces += amount;
    int64_t amt = tactyk_report__indent_spaces;
    if (amt < 0) {
        amt = 0;
        tactyk_report__indent_spaces = 0;
    }
    if (amt > 15) {
        amt = 15;
    }
    for (uint64_t i = 0; i < amt; i++) {
        tactyk_report__indent_buf[i] = ' ';
    }
    tactyk_report__indent_buf[amt] = 0;
}

void tactyk_report__reset() {
    //memset(tactyk_report__buffer, 0, TACTYK_REPORT__BUFSIZE);
    tactyk_report__buffer[0] = 0;
    fseek(rpt_stream, 0, SEEK_SET);
    tactyk_report__indent(0);
}
char* tactyk_report__get() {
    fflush(rpt_stream);
    tactyk_report__buffer[ftell(rpt_stream)] = 0;
    return tactyk_report__buffer;
}
void tactyk_report__msg(char *msg) {
    fprintf(rpt_stream, "%s%s\n", tactyk_report__indent_buf, msg);
}

void tactyk_report__dblock(char *desc, struct tactyk_dblock__DBlock *dblock) {
    fprintf(rpt_stream, "%s%s: ", tactyk_report__indent_buf, desc);
    tactyk_dblock__fprintln(rpt_stream, dblock);
}
void tactyk_report__dblock_args(char *desc, struct tactyk_dblock__DBlock *dblock) {
    fprintf(rpt_stream, "%s%s: ", tactyk_report__indent_buf, desc);
    tactyk_dblock__fprint_structure(rpt_stream, dblock->token->next, false, true, false, 0, ' ');
    fputc('\n', rpt_stream);
}
void tactyk_report__dblock_var(char *desc, struct tactyk_dblock__DBlock *varname, struct tactyk_dblock__DBlock *varvalue) {
    if (desc != NULL) {
        fprintf(rpt_stream, "%s%s ", tactyk_report__indent_buf, desc);
    }
    else {
         fprintf(rpt_stream, "%s", tactyk_report__indent_buf);
    }
    tactyk_dblock__fprint(rpt_stream, varname);
    fprintf(rpt_stream, ": ");
    tactyk_dblock__fprintln(rpt_stream, varvalue);
}
void tactyk_report__dblock_full(char *desc, struct tactyk_dblock__DBlock *dblock) {
    fprintf(rpt_stream, "%s%s:\n", tactyk_report__indent_buf, desc);
    tactyk_dblock__fprint_structure(rpt_stream, dblock, true, false, false, tactyk_report__indent_spaces, '\n');
}
void tactyk_report__dblock_content(char *desc, struct tactyk_dblock__DBlock *dblock) {
    fprintf(rpt_stream, "%s%s:\n", tactyk_report__indent_buf, desc);
    tactyk_dblock__fprint_structure(rpt_stream, dblock->child, false, true, false, tactyk_report__indent_spaces, '\n');
}
void tactyk_report__dblock_list_vars(char *desc, struct tactyk_dblock__DBlock *vars) {
    fprintf(rpt_stream, "%s%s:\n", tactyk_report__indent_buf, desc);
    if (vars->element_count == 0) {
        fprintf(rpt_stream, "%sNONE\n", tactyk_report__indent_buf);
    }
    else {
        struct tactyk_dblock__DBlock **fields = (struct tactyk_dblock__DBlock**)vars->data;
        for (uint64_t i = 0; i < vars->element_capacity; i += 2) {
            struct tactyk_dblock__DBlock *name = fields[i];
            struct tactyk_dblock__DBlock *value = fields[i+1];
            if (name != NULL) {
                tactyk_report__dblock_var(" ", name, value);
            }
        }
    }
}



void tactyk_report__string(char *desc, char *value) {
    fprintf(rpt_stream, "%s%s: %s\n", tactyk_report__indent_buf, desc, value);
}

void tactyk_report__bool(char *desc, bool value) {
    if (value) {
        fprintf(rpt_stream, "%s%s: TRUE\n", tactyk_report__indent_buf, desc);
    }
    else {
        fprintf(rpt_stream, "%s%s: FALSE\n", tactyk_report__indent_buf, desc);
    }
}
void tactyk_report__int(char *desc, int64_t value) {
    fprintf(rpt_stream, "%s%s: %jd\n", tactyk_report__indent_buf, desc, value);
}
void tactyk_report__uint(char *desc, uint64_t value) {
    fprintf(rpt_stream, "%s%s: %ju\n", tactyk_report__indent_buf, desc, value);
}
void tactyk_report__ptr(char *desc, void *value) {
    fprintf(rpt_stream, "%s%s: %p\n", tactyk_report__indent_buf, desc, value);
}
void tactyk_report__hex(char *desc, uint64_t value) {
    fprintf(rpt_stream, "%s%s: %X.%X.%X.%X\n", tactyk_report__indent_buf, desc, 
        (uint16_t) (((value)>>48) & 0xffff),  
        (uint16_t) (((value)>>32) & 0xffff),  
        (uint16_t) (((value)>>16) & 0xffff),  
        (uint16_t) (((value)>>0)  & 0xffff)
    );
}
void tactyk_report__float80(char *desc, long double value) {
    fprintf(rpt_stream, "%s%s: %Lf\n", tactyk_report__indent_buf, desc, value);
}
void tactyk_report__float64(char *desc, double value) {
    fprintf(rpt_stream, "%s%s: %f\n", tactyk_report__indent_buf, desc, value);
}
void tactyk_report__float32(char *desc, float value) {
    fprintf(rpt_stream, "%s%s: %f\n", tactyk_report__indent_buf, desc, value);
}



