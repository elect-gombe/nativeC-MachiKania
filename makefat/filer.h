#ifndef __FILER_H
#define __FILER_H

#include "ff.h"
#include <stdio.h>

#define _MAX_PATH_LENGTH 64
#define _ITEM_NUM 64

#define _N_LINE 16


typedef struct{
  char name[16];
  unsigned int size;
  uint8_t attr;
} item_t;

typedef struct{
  int filenum;
  item_t files[_ITEM_NUM];
  char path[_MAX_PATH_LENGTH];
  uint8_t buff[8];//at least. may be more larger.
} filer_t;

typedef struct{
  int cursor;
  int ofs;
  int sel_num;
} scrsub_t;

FRESULT filer(char *path,void *work,size_t size_work);
#endif
