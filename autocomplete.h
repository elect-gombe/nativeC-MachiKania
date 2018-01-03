#ifndef __AUTOCOMPLETE_H
#define __AUTOCOMPLETE_H
#include "stdint.h"

typedef struct{
  uint8_t attr;
} ctype_t;

typedef struct{
  char name[16];
  uint8_t attr;
} citem_t;

#define _MAX_COMPLETE_LENGTH 9
#define _MAX_PATH_LENGTH 64

typedef struct{
  citem_t items[_MAX_COMPLETE_LENGTH];
  int length;
  char matchpart[_MAX_PATH_LENGTH];
} clist_t;

int inputfilepath(char *name, ctype_t *type);

#endif
