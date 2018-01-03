#ifndef _NATIVE_C
#define _NATIVE_C

#define RAMSIZE (58000)
extern int *RAM;
extern int ARAM[RAMSIZE/4];

#define AVAILABLE_MEM (0xA0010000-((unsigned)RAM)-256-10000)
#define AVAILABLE_MEM_INCLUDING_STACK (0xA0010000-((unsigned)RAM)-256)

#define SLOWMODE 1
#define MAX_USER_APP_FILE 2

#endif
