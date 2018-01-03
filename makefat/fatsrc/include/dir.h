#ifndef __DIR_H
#define __DIR_H

#ifndef __FILE_H
#include <file.h>

typedef struct {
  unsigned fsize;
  unsigned short fdate;
  unsigned short ftime;
  unsigned char	fattrib;
  char	fname[13];
} FILINFO;

#define DIR void

#define opendir ((int(*)(DIR *,const char*))g_func_p[0x5 ])
#define readdir ((int(*)(void*,FILINFO*))g_func_p[0x6 ])
#define findfirst ((int(*)(DIR*,FILEINFO*,const char*,const char*))g_func_p[0x7 ])
#define findnext ((int (*)(DIR *,FILINFO*))g_func_p[0x8 ])

#endif
