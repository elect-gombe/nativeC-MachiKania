
#include <stdio.h>
#include <stdint.h>
#include "ff.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "autocomplete.h"
#include "ffdir.h"
#include "xprintf.h"

extern FATFS fs;

static
void put_rc (FRESULT rc)
{
  const char *str =
    "OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
    "INVALID_NAME\0" "DENIED\0" "FILE_EXIST (recursive delete not enabled)\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
    "INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
    "LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0" "INVALID_PARAMETER\0";
  FRESULT i;

  for (i = 0; i != rc && *str; i++) {
    while (*str++) ;
  }
  xprintf("rc=%u FR_%s\n", (UINT)rc, str);
  if(rc){
    sleep(1);
  }
}

static
FRESULT load_dir (filer_t *fw) {
  DIR dir;
  FRESULT res;
  FILINFO fno;

  fw->filenum = 0;
  res = f_getcwd(fw->path, _MAX_PATH_LENGTH);
  if (res == FR_OK) {
    res = f_opendir(&dir, "");
    if (res == FR_OK) {
      do {
	res = f_readdir(&dir, &fno);
	if (res || !fno.fname[0]) break;
	fw->files[fw->filenum].size = fno.fsize;
	fw->files[fw->filenum].attr = fno.fattrib;
	strcpy(fw->files[fw->filenum].name, fno.fname);
	fw->filenum++;
      } while (fw->filenum < _ITEM_NUM);
    }
  }  

  return res;
}

char *size2str(uint64_t size,char *str){
  if(size < 10000){
    xsprintf(str,"%lu.%02luKB",size/1000UL,size%1000UL/10UL);
  }else if(size < 1000000UL){
    xsprintf(str,"  %3luKB",size/1000UL);
  }else if(size < 10000000UL){
    xsprintf(str,"%lu.%02luMB",size/1000000UL,size%1000000UL/10000UL);
  }else if(size < 1000000000UL){
    xsprintf(str,"  %3luMB",size/1000000UL);
  }else if(size < 10000000000UL){
    xsprintf(str,"%lu.%02luGB",size/1000000000UL,size%1000000000UL/10000000UL);
  }else{
    xsprintf(str,"  %3luGB",size/1000000000UL);
  }
  return str;
}

static
FRESULT redraw(filer_t *fw,const scrsub_t *sb){
  int i;
  uint64_t size;
  FRESULT res;
  FATFS *fsp;
  xprintf("\033[1;1H");
  
  xprintf("\033[33m\n\n%s\n\033[39m",fw->path);
  for(i=sb->ofs;i<_N_LINE+sb->ofs;i++){
    if(i >= fw->filenum){puts("");continue;}
    if(sb->cursor == i)xprintf(">");
    else xprintf(" ");
    if(fw->files[i].attr & 0x80)xprintf("\033[32m");
    if(fw->files[i].attr & AM_DIR)
      xprintf("%-14s   <DIR>\n",fw->files[i].name);
    else
      xprintf("%-14s  %s\n",fw->files[i].name,size2str(fw->files[i].size,(void*)fw->buff));

    if(fw->files[i].attr & 0x80)xprintf("\033[39m");
  }
  res = f_getfree("", &size, &fsp);
  if(res)return res;
  if(sb->sel_num == 0){
    xprintf("%d items %s free\n\n\n",fw->filenum,size2str(size*fsp->csize*512,(void*)fw->buff));
  }else{
    xprintf("%d/%d items selected \n\n\n",sb->sel_num,fw->filenum);
  }
  return res;
}


FRESULT cp_file (
		 char *desti_path,//must be writable!
		 filer_t *fw,
		 INT item,
		 UINT sz_work
		 )
{
  UINT sz_buf, br, bw;
  int i;
  FRESULT res;
  FILINFO fno;
  FIL fil0,fil1;
  
  for (sz_buf = 0x4000; sz_buf > sz_work - sizeof (filer_t); sz_buf >>= 1) ;

  res = f_open(&fil0, fw->files[item].name, FA_READ);
  if (res) return res;

  i = strlen(desti_path);
  xsprintf(&desti_path[i], "./%s", fw->files[item].name);
  res = f_open(&fil1, desti_path, FA_WRITE | FA_CREATE_ALWAYS);
  if (res) {
    f_close(&fil0);
    desti_path[i] = 0;
    return res;
  }

  for (;;) {
    res = f_read(&fil0, fw->buff, sz_buf, &br);
    if (res || !br) break;
    res = f_write(&fil1, fw->buff, br, &bw);
    if (res) break;
    if (br > bw) { res = 100; break; }
  }

  f_close(&fil1);
  f_close(&fil0);

  if (res == 100) { 
    f_unlink(desti_path);
  }

  desti_path[i] = 0;
  return res;
}

FRESULT filer(char *path,void *work,size_t size_work){
  filer_t *fw;
  scrsub_t sb = {
    .cursor = 0,
    .sel_num = 0,
    .ofs = 0
  };
  char key;
  int reload;
  int i,j;
  FRESULT res;
  char tn[_MAX_PATH_LENGTH];

  ctype_t type;

  xprintf("\033[2J");
  system("stty -echo -icanon min 1 time 0");  
  if(size_work < sizeof(filer_t))
    return -1;

  fw = work;//word alignment.
  f_chdir(path);

  type.attr = 0;
  inputfilepath(tn,&type,"file:");
  type.attr = AM_DIR;
  inputfilepath(tn,&type,"dir:");
  while(1){
    xprintf("\033[2J");
    sb.cursor = 0;
    sb.sel_num = 0;
    sb.ofs = 0;
    load_dir(fw);
    redraw(fw,&sb);
    reload = 0;
    while(!reload){
      key = getchar();
      switch(key){
      case '8'://up
	if(sb.cursor){
	  sb.cursor--;
	  if(sb.cursor < sb.ofs)sb.ofs--;
	  redraw(fw, &sb);
	}
	break;
      case '2'://down
	if(sb.cursor+1 < fw->filenum){
	  sb.cursor++;
	  if(sb.cursor-sb.ofs >= _N_LINE)sb.ofs++;
	  redraw(fw, &sb);
	}
	break;
      case 'k':
	type.attr = AM_DIR;
	tn[0] = 0;
	inputfilepath(tn,&type,"mkdir:");
	res = f_mkdir(tn);
	if(res)put_rc(res);
	reload = 1;
	break;
      case 'r':
	{
	  char msg[30],len;
	  xsprintf(msg,"<rename> %s->:",fw->files[sb.cursor].name);
	  type.attr = AM_DIR;
	  tn[0] = 0;
	  inputfilepath(tn,&type,msg);
	  len = strlen(tn);
	  if(tn[len-1]=='/'){
	    strcat(tn,fw->files[sb.cursor].name);
	  }
	}
	f_rename(fw->files[sb.cursor].name,tn);
	reload = 1;
	break;
      case 'c':
	type.attr = AM_DIR;
	tn[0] = 0;
	inputfilepath(tn,&type,"copy to");
	j=0;
	if(sb.sel_num){
	  for(i=0;i<sb.sel_num;i++){
	    for(;j<fw->filenum&&!(fw->files[j].attr&0x80);j++);
	    if(j==fw->filenum){
	      printf("\n\nerr\n");
	      sleep(1);
	      break;
	    }
	    put_rc(cp_file(tn, fw,j++, size_work));
	  }
	}else{
	  put_rc(cp_file(tn, fw,sb.cursor, size_work));	  
	}
	reload = 1;
	break;
      case 'd':
	if(sb.sel_num){
	  i=0;
	  do{
	    for(;(fw->files[i].attr&0x80)==0;i++);
	    res = f_unlink(fw->files[i].name);
	    if(res){
	      put_rc(res);
	    }
	    i++;
	  }while(--sb.sel_num);
	  sb.sel_num++;
	  reload = 1;
	}else{
	  res = f_unlink(fw->files[sb.cursor].name);
	  if(res){
	    put_rc(res);
	    sleep(1);
	    break;
	  }
	  reload = 1;
	}
	break;
      case '\n'://decide/open
	if(fw->files[sb.cursor].attr & AM_DIR){
	  f_chdir(fw->files[sb.cursor].name);
	  reload = 1;
	}
	break;	
      case ' '://select
	fw->files[sb.cursor].attr ^= 0x80;
	if(fw->files[sb.cursor].attr & 0x80)sb.sel_num++;
	else sb.sel_num--;
	redraw(fw, &sb);
	break;
      case 'q':
	return FR_OK;
      }
    }
  }
}
