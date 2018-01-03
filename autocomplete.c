#include "autocomplete.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include "xprintf.h"
#include "keyinput.h"
#include "videoout.h"
#include "ps2keyboard.h"

static
void printlist(clist_t *cl){
  int i;
  if(cl->length){
    for(i=0;i<cl->length;i++){
      xprintf("%-13s",cl->items[i].name);
      if(i%3==2)xprintf(" ");
    }
  }else{
    i=1;
    xprintf("%-13s","no hit");
  }
  for(;i<_MAX_COMPLETE_LENGTH;i++){
    xprintf("%13s","");
  }
}

static
int csearch(char *name,ctype_t *type,clist_t *cl){
  FRESULT res;
  DIR dr;
  FILINFO fno;
  int i=0,j;
  char tmp[16];
  char *filepart;
  char path[_MAX_PATH_LENGTH];

  strcpy(path,name);
  filepart = path;
  while(*filepart)filepart++;
  while(path!=filepart&&*filepart!='/')filepart--;
  if(path!=filepart){
    *filepart='\0';
    filepart++;
    /* xprintf("name(%s)  path:(%s)  pattern:(%s)\n\n",name,path,filepart); */
    strcpy(tmp,filepart);
    strcat(tmp,"*");
    /* xprintf("\nsearch for [%s]\n",name); */
    res = f_findfirst(&dr,&fno,path,tmp);
  }else{//no dir
    /* xprintf("name(%s)  path:.  pattern:(%s)\n\n",name,filepart); */
    strcpy(tmp,filepart);
    strcat(tmp,"*");
    /* xprintf("\nsearch for [%s]\n",name); */
    res = f_findfirst(&dr,&fno,"",tmp);
  }
  
  while(res == FR_OK&&fno.fname[0]&&i<_MAX_COMPLETE_LENGTH){
    cl->items[i].attr = fno.fattrib;
    strcpy(cl->items[i].name,fno.fname);
    if(fno.fattrib&AM_DIR){
      if(cl->items[i].name[0] == '.'){
	i--;//ignore
      }else{
	strcat(cl->items[i].name,"/");
      }
    }else if(type->attr == AM_DIR){
      i--;//ignore
    }
    i++;
    res = f_findnext(&dr, &fno);
  }
  cl->length = i;
  if(i==0){
    printlist(cl);
  }else if(i==1){
    if(path!=filepart){
      strcat(path,"/");
      strcat(path,cl->items[0].name);
      strcpy(name,path);
    }else{
      strcpy(name,cl->items[0].name);
      if(cl->items[0].attr & AM_DIR)return 0;
      return 1;//all auto-completed
    }
  }else{
    strcpy(cl->matchpart,cl->items[0].name);
    for(i=1;i<cl->length;i++){
      for(j=0;cl->matchpart[j]==cl->items[i].name[j];j++);
      cl->matchpart[j] = 0;
    }
    /* xprintf("*match:(%s)\n",cl->matchpart); */
    if(path!=filepart){
      strcat(path,"/");
      strcat(path,cl->matchpart);
      strcpy(name,path);
    }else{
      strcpy(name,cl->matchpart);
    }    
    printlist(cl);
  }

  return 0;
}

/*name length must be good enough! :) */
/* size of buffer is about 1k*/
int inputfilepath(char *name, ctype_t *type){
  clist_t lst;

  char m;
  char *sb = name;
  int eofilename = 0;
  char *pcursor = cursor-strlen(name);
  int isfinished = 0;
  while(*sb)sb++;
  while(!eofilename){
    m = cursorinputchar();
    switch(m){
    case '\n':
      return 0;
    case ' ':
      if(sb==name)break;
      return 1;
    case '\t':
      printchar('\n');
      isfinished=csearch(name,type,&lst);
      cursor = pcursor;
      if(cursor > TVRAM+40*23)//push up
	cursor -=40*((cursor-TVRAM-23*40)/40);
      sb = name;
      while(*sb){
	printchar(*sb);
	sb++;
      }
      if(isfinished)return 1;
      break;
    case '\b':
      if(sb != name){
	cursor--;
	printchar(' ');
	cursor--;
	sb--;
	*sb = 0;
      }else{
	return -1;
      }
      break;
    default:
      *sb++ = m;
      *sb = 0;
      printchar(m);
    }
  }
  return 0;
}
