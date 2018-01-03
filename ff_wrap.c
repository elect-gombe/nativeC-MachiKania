#include "ff.h"
#include "diskio.h"
#include <stdint.h>
#include "videoout.h"
#include "xprintf.h"
#include "sd.h"
#include "flash.h"

//in and out and some including etc...
// maxinum files depend on ram size.
//if they give 17 kbyte of ram memory, we can handle 30 files.
//or reduce to 8 kbyte, only 15 files. adjust your application requirements.
//default user application file num is defined MAX_USER_APP_FILE(maybe it's 2)

FATFS fs[2];
FIL *files;
int up_to_files = 0;
uint32_t usingfile;
int no_sd;

#define wait(x)
#define printstr(x)
#define printnum(x)
#define puts(str) xprintf("%s\n",(str))

int fsinit(void){
  if(f_mount(&fs[0],"0:", 1)){
    puts("0:initfaild");
    return -1;
  }
  if(f_mount(&fs[1],"1:", 1)){
    no_sd = 1;
    puts("1:initfaild\n");
    return -1;
  }
  if(f_chdrive("1:")!=FR_OK){
    puts("chdir faild");
    return -1;
  }
  return 0;
}

DSTATUS disk_status (BYTE pdrv){
  if(pdrv == 0){
    return disk_status_flash();
  }else{
    return disk_status_sd();
  }
}

DSTATUS disk_initialize (BYTE pdrv){
  if(pdrv == 0){
    return disk_initialize_flash();
  }else{
    return disk_initialize_sd();
  }
}

DRESULT disk_read(BYTE pdrv,BYTE *buff,DWORD sector,UINT count){
  if(pdrv == 0){
    return disk_read_flash(buff,sector,count);
  }else{
    return disk_read_sd(buff,sector,count);
  }
}

DRESULT disk_write(BYTE pdrv,const BYTE *buff,DWORD sector,UINT count){
  if(pdrv == 0){
    return disk_write_flash(buff,sector,count);
  }else{
    return disk_write_sd(buff,sector,count);
  }
}

DRESULT disk_ioctl(BYTE pdrv,BYTE cmd,void* buff){
  if(pdrv == 0){
    return disk_ioctl_flash(cmd,buff);
  }else{
    return disk_ioctl_sd(cmd,buff);
  }
}

// make sure that all files must be all closed.
// to call ffcloseall to close all files.
void* fframset(void *ram,int number){
  files = ram;
  up_to_files = number;
  return (void*)(((uint8_t*)ram)+sizeof(FIL)*number);
}

FIL* fopen2(const char *filename,uint8_t fa){
  FIL *fp=0;
  int i;
  FRESULT res;

  for(i=0;i<up_to_files;i++){
    if((usingfile&(1<<i))==0){
      fp = &files[i];
      break;
    }
  }
  if(i==up_to_files){
    printstr("Cannot allocate memory\n");
    printnum(usingfile);
    wait(60);
    return 0;
  }
  if((res = f_open(fp,filename,fa))!=FR_OK){
    xprintf("file open failure:%d\n",res);
    return 0;
  }else{
    usingfile |= 1<<i;
  }

  return fp;  
}

#define FILE void
#define EOF (-1)

FIL* fopen(const char *filename,const char *mode){
  uint8_t fa = 0;

  if(mode[0] == 'r'&&mode[1] == '\0'){
    fa = FA_READ;
  }else if(mode[0] == 'r'&&mode[1] == '+'){
    fa = FA_READ|FA_WRITE;
  }else if(mode[0] == 'w'){
    fa = FA_CREATE_ALWAYS|FA_WRITE;
    if(mode[1] == '+'){
      fa |= FA_READ;
    }
  }else if(mode[0] == 'a'){
    /* fa = FA_OPEN_APPEND | FA_WRITE; */
    /* if(mode[1] == '+'){ */
    /*   fa |= FA_READ; */
    /* }     */
  }else if(mode[0] == 'w'){
    if(mode[1] == 'x'){
      fa = FA_CREATE_NEW | FA_WRITE;
    }else if(mode[1] == '+'&&mode[2] == 'x'){
      fa |= FA_READ;
    }
  }

  return fopen2(filename,fa);
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int fclose(FIL *f){
  int i;
  for(i=0;i<up_to_files;i++){
    if(f == &files[i]){
      if(usingfile&(1<<i)){
	usingfile &= ~(1<<i);
      }else{
	printstr("Cannot close file\n");
	wait(60);
	return -1;
      }
    }
  }
  return f_close(f);
}

int fcloseall(void){
  int i;
  FRESULT res;
  for(i=0;i<up_to_files;i++){
    if(usingfile&(1<<i)){
      usingfile &= ~(1<<i);
      res = f_close(&files[i]);
      if(res)break;
    }
  }
  return res;
}
