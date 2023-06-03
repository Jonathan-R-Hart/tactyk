#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "tactyk_report.h"
#include "tactyk_dblock.h"

char tactyk_report__buffer[TACTYK_REPORT__BUFSIZE];
FILE *rpt_stream;

void tactyk_report__init() {
    rpt_stream = fmemopen(tactyk_report__buffer, TACTYK_REPORT__BUFSIZE, "w");
}

void tactyk_report__reset() {
    memset(rpt_stream, 0, TACTYK_REPORT__BUFSIZE);
    fseek(rpt_stream, 0, SEEK_SET);
}
void tactyk_report__msg(char *msg) {
    fprintf(rpt_stream, "%s\n", msg);
    fflush(rpt_stream);
}

void tactyk_report__dblock(char *desc, struct tactyk_dblock__DBlock *dblock) {
    tactyk_dblock__fprintln(rpt_stream, dblock);
    fflush(rpt_stream);
}
void tactyk_report__dblock_full(char *desc, struct tactyk_dblock__DBlock *dblock) {
    tactyk_dblock__fprint_structure_simple(rpt_stream, dblock);
    fflush(rpt_stream);
}

void tactyk_report__bool(char *desc, bool value) {
    if (value) {
        fprintf(rpt_stream, "%s: TRUE\n", desc);
    }
    else {
        fprintf(rpt_stream, "%s: FALSE\n", desc);
    }
    fflush(rpt_stream);
}
void tactyk_report__int(char *desc, int64_t value) {
    fprintf(rpt_stream, "%s: %jd\n", desc, value);
    fflush(rpt_stream);
}
void tactyk_report__uint(char *desc, uint64_t value) {
    fprintf(rpt_stream, "%s:  %ju\n", desc, value);
    fflush(rpt_stream);
}
void tactyk_report__hex(char *desc, uint64_t value) {
    fprintf(rpt_stream, "%s:  %X.%X.%X.%X\n", desc, 
        (uint16_t) (((value)>>48) & 0xffff),  
        (uint16_t) (((value)>>32) & 0xffff),  
        (uint16_t) (((value)>>16) & 0xffff),  
        (uint16_t) (((value)>>0)  & 0xffff)
    );
    fflush(rpt_stream);
}
void tactyk_report__float80(char *desc, long double value) {
    fprintf(rpt_stream, "%s: %Lf\n", desc, value);
    fflush(rpt_stream);
}
void tactyk_report__float64(char *desc, double value) {
    fprintf(rpt_stream, "%s: %f\n", desc, value);
    fflush(rpt_stream);
}
void tactyk_report__float32(char *desc, float value) {
    fprintf(rpt_stream, "%s: %f\n", desc, value);
    fflush(rpt_stream);
}



