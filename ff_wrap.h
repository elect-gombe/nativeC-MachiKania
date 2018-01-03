#ifndef __FF_WRAP_H
#define __FF_WRAP_H
#include "ff.h"
#include "xprintf.h"


#define fgetpos(x,y) ((fpos_t*)y)->u.align = f_tell(((FIL*)(x)))
#define fsetpos(x,y) f_lseek(((FIL*)(x)), ((fpos_t*)y)->u.align)

#define fprintf f_printf
#define printf xprintf

#define puts(str) printf("%s\n",str)

#define fputc(x,y) f_putc((x),((FIL*)(y)))
#define fputs(x,y) f_puts((x),((FIL*)(y)))

#define unlink f_unlink

extern int no_sd;

static
inline int fread(void *ptr,int size,int nmemb,FIL *fp){
  unsigned int br;
  f_read(fp, ptr, size*nmemb, &br);
  
  return br/size;
}

static
inline int fwrite(const void *ptr,int size,int nmemb,FIL *fp){
  unsigned int br;
  f_write(fp, ptr, size*nmemb, &br);
  return br/size;
}

static
inline int fgetc(FIL *fp){
  char m;
  unsigned int br;
  int res;
  res = f_read(fp,&m,1,&br);
  if(res == 0&&br==1){
    return m;
  }else{
    /* printf("%d\n",res); */
    return -1;
  }
}

#define FILE FIL

FIL* fopen(const char *filename,const char *mode);
int fclose(FIL *fp);
void* fframset(void *ram,int number);
int fcloseall(void);

#endif
