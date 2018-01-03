#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "ff.h"
#include <stdint.h>
#include <unistd.h>
#include "filer.h"
#include "flash.h"
#include "xprintf.h"

void make(char *path);
static
void copy(const char *filename);

extern
uint8_t flash_buffer[];


static void putchara(unsigned char ch){
  putchar(ch);
}


int main(int argc,char **argv){
  char path[1024];
  char oldpath[1024];
  
  xdev_out(putchara);
  if(argc != 2){
    fprintf(stderr,"param:path of source\n");
  }
  init_flashfs();

  strcpy(path,".");

  getcwd(oldpath,sizeof(oldpath));
  chdir(argv[1]);
  make(path);
  chdir(oldpath);  

  sector_dump();
  /* uint32_t work[4096]; */
  /* filer(".",work,sizeof(work)); */
  return 0;
}

char tmpfilename[1024];

void make(char *path){
  DIR *dir;
  struct dirent *dp;
  int len;
  dir = opendir(path);
  for(dp=readdir(dir);dp!=NULL;dp=readdir(dir)){
    if(dp->d_type == DT_DIR){
      if(dp->d_name[0] == '.')continue;
      char *bp = path;
      len = 1;
      while(*++bp)len++;
      sprintf(bp, "/%s",dp->d_name);
      fprintf(stderr,"search into [%s]\n",path);
      f_mkdir(path);
      make(path);
      path[len]=0;
    }else{
      sprintf(tmpfilename,"%s/%s",path,dp->d_name);
      fprintf(stderr,"%s\n",tmpfilename);
      copy(tmpfilename);
    }
  }
  closedir(dir);  
}

static
void copy(const char *filename){
  FILE *fp1;
  FIL fp2;
  uint8_t buff[1024*100];
  unsigned int read;
  unsigned int writtennum;
  FRESULT res;
  
  fp1 = fopen(filename,"r");
  if(fp1==NULL){
    fprintf(stderr,"file error[%s]",filename);
  }
  read = fread(buff,1,sizeof(buff),fp1);
  fclose(fp1);

  res = f_open(&fp2,filename,FA_CREATE_ALWAYS | FA_WRITE);
  fprintf(stderr,"%d\n",res);
  f_write(&fp2,buff,read,&writtennum); 
  f_close(&fp2);
}
