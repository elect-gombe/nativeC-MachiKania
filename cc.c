/*frontend of c compiler.*/

#include "xprintf.h"
#include "nativec.h"
#include "ff.h"
#include "ff_wrap.h"

/* now we are surporting only this style.
 *    *.c=>*.s=>*.o=>[a.out]  (compile, assemle and linking) (*.c compile mode.)
 *              *.o=>[a.out]  (linking only)                 (*.o linking)
 *    *.c=>*.s=>*.o           (compile & assemble)           (-c option, output object code.)
 *    *.c=>*.s                (just generate assemble list)  (-S option, output assemble code)
 *
 * Another option to help development.
 *
 *  -Dname=val          define name as value.
 *  -Ipath              define include path.
 *  -o name             define output name. default is a.out
 *
 * ex.
 *  cc test.c
 *  cc test.c -c -o test.o
 *  cc test.o hoge.o -o hoge.out
 * 
 *  Addition, We provide NO-SDCARD mode. In this mode, we delete all procedure files, 
 * such *.o, *.s.
 */

#define _CC_MAX_FILENAME 64
#define _CC_MAX_INPUT_FILES 10
typedef struct{
  char is_compileonly;
  char is_assembleonly;
  char is_onscreen;
  char outputname[_CC_MAX_FILENAME];
  char inputfiles[_CC_MAX_INPUT_FILES][_CC_MAX_FILENAME];
  int inputnum;
} cc_t;

int strlen(const char*);
int strcpy(char *,const char *);

int smlrcmain(int argc,const char **argv);
int asmain(int argc,const char **argv);
int ldmain(int argc,const char **argv,unsigned baseaddress);

void printarg(int argc,const char **argv){
  int i;
  xprintf("\"");
  for(i=0;i<argc;i++){
    xprintf("%s ",argv[i]);
  }
  xprintf("\"\n");
}

int cc(int argc,const char **argv){
  cc_t ccobj={0};
  
  int i;
  const char *ap;
  int ret;
  printarg(argc,argv);

  xsprintf(ccobj.outputname,"a.out");

  // step 1, args analysis.
  for(i=1;i<argc;i++){
    ap = argv[i];
    if(ap[0]=='-'){
      switch(ap[1]){
      case 'p':
	ccobj.is_onscreen = 1;
	break;
      case 'c':
	ccobj.is_compileonly = 1;
	break;
      case 'S':
	ccobj.is_assembleonly = 1;
	break;
      case 'o':
	if(argv[i+1][0]){
	  strcpy(ccobj.outputname,argv[i+1]);
	  i++;
	  break;
	}
      default:
	xprintf("unknown option%s\n",ap);
	ret = -1;
	goto fin;
      }
    }else{
      xsprintf(ccobj.inputfiles[ccobj.inputnum],ap);
      ccobj.inputnum++;
      if(ccobj.inputnum==_CC_MAX_INPUT_FILES){
	xprintf("too many input files.");
	ret = -1;
	goto fin;
      }
    }
  }
  if(ccobj.inputnum==0){
    xprintf("input file required.\n");
    ret = -1;
    goto fin;
  }
  if(ccobj.is_compileonly&&ccobj.inputnum!=1){
    xprintf("-c -o option is required just ONE source file.\n");
    ret = -1;
    goto fin;
  }
  if(!ccobj.is_onscreen){
    stop_composite();
  }

  //step2 compilation.
  int len;
  char outfilename[_CC_MAX_FILENAME];
  const char *varg[_CC_MAX_INPUT_FILES+3];
  for(i=0;i<ccobj.inputnum;i++){
    len = strlen(ccobj.inputfiles[i]);
    switch(ccobj.inputfiles[i][len-1]){
    case 'C':
    case 'c':/* *.c file compilation*/
      ccobj.inputfiles[i][len-1] = '\0';
      if(!ccobj.is_compileonly)
	xsprintf(outfilename, "%ss" ,ccobj.inputfiles[i]);
      else
	xsprintf(outfilename,"%s",ccobj.outputname);
      ccobj.inputfiles[i][len-1] = 'c';
      varg[0] = "smlrc";
      varg[1] = ccobj.inputfiles[i];
      varg[2] = outfilename;
      varg[3] = "-SI";
      varg[4] = "/include/";
      printarg(5,varg);
      if(smlrcmain(5,varg)){
	ret = -2;
	goto fin;
      }
      fcloseall();
      ccobj.inputfiles[i][len-1] = 's';
    case 's':
    case 'S':
      if(ccobj.is_assembleonly)break;
      ccobj.inputfiles[i][len-1] = '\0';
      xsprintf(outfilename, "%so" ,ccobj.inputfiles[i]);
      ccobj.inputfiles[i][len-1] = 's';
      varg[0] = "as";
      varg[1] = "-o";
      varg[2] = outfilename;
      varg[3] = ccobj.inputfiles[i];
      printarg(4,varg);
      if( asmain(4,varg) ){
	ret = -3;
	goto fin;
      }
      fcloseall();
      if(no_sd){
	if(f_unlink(ccobj.inputfiles[i])){
	  printf("error while removing `%s`\n",ccobj.inputfiles[i]);
	}
      }
      ccobj.inputfiles[i][len-1] = 'o';
      break;
    case 'o':
    case 'O':
      break;
    default:
      xprintf("unknown file type [%s]\n",ccobj.inputfiles[i]);
      ret = -1;
      goto fin;
    }
  }

  if(ccobj.is_compileonly||ccobj.is_assembleonly){
    ret = 0;
    goto fin;
  }
  //step3 linking.
  /*[exefile] [inputs] -o [output]*/
  varg[0] = "ld";
  varg[1] = "-o";
  varg[2] = ccobj.outputname;
  varg[3] = "-e";
  varg[4] = "startup";
  for(i=0;i<ccobj.inputnum;i++){
    varg[i+5] = ccobj.inputfiles[i];
  }

  printarg(i+5,varg);
  if(ldmain(i+5,varg,((unsigned)ARAM)+sizeof(FIL)*MAX_USER_APP_FILE)){
    ret = -4;
    goto fin;
  }
  fcloseall();
  if(no_sd){
    for(i=0;i<ccobj.inputnum;i++){
      if(f_unlink(ccobj.inputfiles[i])){
	printf("error:while removing `%s`\n",ccobj.inputfiles[i]);
      }
    }
  }

  ret = 0;
 fin:
  if(!ccobj.is_onscreen){
    start_composite();
  }
  return ret;
}
