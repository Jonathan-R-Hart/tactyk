#include <stdint.h>

#include "tactyk_dblock.h"

// tactyk error and warning reporting interface

#define TACTYK_REPORT__BUFSIZE (1<<18)

void tactyk_report__init();
char* tactyk_report__get();
void tactyk_report__reset();
void tactyk_report__msg(char *msg);

void tactyk_report__dblock(char *desc, struct tactyk_dblock__DBlock *dblock);
void tactyk_report__dblock_full(char *desc, struct tactyk_dblock__DBlock *dblock);

void tactyk_report__string(char *desc, char *value);
void tactyk_report__bool(char *desc, bool value);
void tactyk_report__int(char *desc, int64_t value);
void tactyk_report__uint(char *desc, uint64_t value);
void tactyk_report__ptr(char *desc, void *value);
void tactyk_report__hex(char *desc, uint64_t value);
void tactyk_report__float80(char *desc, long double value);
void tactyk_report__float64(char *desc, double value);
void tactyk_report__float32(char *desc, float value);
