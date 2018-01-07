#include "videoout.h"
#include "ff.h"
#include "xprintf.h"
#include <xc.h>
#include "nativec.h"
#include <stdint.h>
#include "ps2keyboard.h"
#include "videoout.h"
#include "keyinput.h"
#include "autocomplete.h"

#define PERSISTENT_RAM_SIZE (RAMSIZE+256)
int ARAM[RAMSIZE/4] __attribute__((persistent,address(0xA0000000+0x10000-PERSISTENT_RAM_SIZE)));;
int *RAM;
extern const unsigned functiontable[];

#define puts(x) xprintf("%s\n",(x))

#pragma config PMDL1WAY = OFF, IOL1WAY = OFF
#pragma config FPLLIDIV = DIV_1, FPLLMUL = MUL_16, FPLLODIV = DIV_1
#pragma config FNOSC = PRIPLL, FSOSCEN = OFF, POSCMOD = XT, OSCIOFNC = OFF
#pragma config FPBDIV = DIV_1, FWDTEN = OFF, JTAGEN = OFF, ICESEL = ICS_PGx1

void wait60thsec(unsigned short n){
  // 60分のn秒ウェイト（ビデオ画面の最下行信号出力終了まで待つ）
  n+=drawcount;
  while(drawcount!=n);
}

int main(void){
  asm volatile("la $sp,%0"::"i"(0xA0000000+0x10000-256));
  asm volatile("j _main");
}

int _main(void){
  int i;

  if(DEVCFG1 & 0x8000){
    // Set Clock switching enabled and reset
    NVMWriteWord(&DEVCFG1,DEVCFG1 & 0xffff7fff);
    SoftReset();
  }

#define mBMXSetRAMKernProgOffset(offset)	(BMXDKPBA = (offset))
#define mBMXSetRAMUserDataOffset(offset)	(BMXDUDBA = (offset))
#define mBMXSetRAMUserProgOffset(offset)	(BMXDUPBA = (offset))

  // Make RAM executable. See also "char RAM[RAMSIZE]" in globalvars.c
  mBMXSetRAMKernProgOffset(0x10000-sizeof(ARAM));
  mBMXSetRAMUserDataOffset(0x10000);
  mBMXSetRAMUserProgOffset(0x10000);

  /* ポートの初期設定 */
  TRISA = 0x0000; // PORTA全て出力
  ANSELA = 0x0000; // 全てデジタル
  TRISB = KEYSTART | KEYFIRE | KEYUP | KEYDOWN | KEYLEFT | KEYRIGHT;// ボタン接続ポート入力設定
  ANSELB = 0x0000; // 全てデジタル
  CNPUBSET=KEYSTART | KEYFIRE | KEYUP | KEYDOWN | KEYLEFT | KEYRIGHT;// プルアップ設定
  ODCB = 0x0300;	//RB8,RB9はオープンドレイン

  // 周辺機能ピン割り当て
  SDI2R=2; //RPA4:SDI2
  RPB5R=4; //RPB5:SDO2
  xdev_out(printchar);

  ps2mode(); //RA1オン（PS/2有効化マクロ）

  init_composite();
  cls();
//  blue_screen();
  set_width(1);

  if(ps2init()){ //PS/2初期化
    //キーボードが見つからない場合
    printstr("Keyboard Not Found\n");
  }
  else printstr("keyboard ...OK\n");

  xprintf("smlrc\n");
  if(fsinit()){
    puts("initialize faild");
  }else{
    puts("internal fat mounted");
  }

  // close all files
  fcloseall();
  RAM = fframset(ARAM,10);

  while(1){
    char *argv[12];
    sh(1,argv);
  }

  while(1);
  return 0;
}

#define PARAM_LEN 64
#define SH_PARAM_NUM 12
void interructivesh(int *argc,char argv[SH_PARAM_NUM][PARAM_LEN]){
  int key;
  char *ap = argv[0];
  int i;
  char pwd[PARAM_LEN];
  f_getcwd(pwd, sizeof(pwd));
  *argc = 0;
  
  xprintf("%s\nmsh>",pwd);
  while(*ap!='\n'){
    if(*argc!=0){
      ctype_t type = {0};
      int next = inputfilepath(ap,&type);
      if(next == 0){
	if(*ap){
	  (*argc)++;
	}
	printchar('\n');
	return;
      }
      if(next == 1){
	cursor++;
      }
      else {
	cursor--;
	printchar(' ');
	cursor--;
      }
      (*argc)+=next;
      ap = argv[*argc];
    }else{//first contact
      while(*ap)ap++;
      key = cursorinputchar();
      if(key == ' '||key == '\t'){
	(*argc)+=1;
	ap = argv[*argc];
	printchar(' ');
      }else if(key == '\b'){
	if(ap == argv[0])continue;
	cursor--;
	printchar(' ');
	cursor--;
	*--ap='\0';
      }else if(key == '\n'){
	printchar('\n');
	if(argv[0][0])(*argc)++;
	return ;
      }else{
	*ap++ = key;
	printchar(key);
	*ap = '\0';
      }
    }
  }
}

void sh(int argc,char **argv){
  char line[SH_PARAM_NUM][PARAM_LEN]={{0}};
  if(argc==1){
    interructivesh(&argc,line);
    int i;
    for(i=0;i<argc;i++){
      argv[i] = line[i];
    }
  }
  if(strcmp(argv[0],"cc")==0){
    return cc(argc,argv);
  }else if(strcmp(argv[0],"smlrc")==0){
    return smlrcmain(argc,argv);
  }else if(strcmp(argv[0],"as")==0){
    return asmain(argc,argv);
  }else if(strcmp(argv[0],"ld")==0){
    return ldmain(argc,argv,((unsigned)ARAM)+sizeof(FIL)*MAX_USER_APP_FILE);
  }else if(strcmp(argv[0],"edt")==0){
    aoutload("editor.out",argc,argv,functiontable,0);
    cls();
    return;
  }else if(strncmp(argv[0],"./",2)==0){
    aoutload(argv[0],argc,argv,functiontable,1);
    return;
  }else if(strcmp(argv[0],"free")==0){
    int nclust;
    FATFS *f;
    f_getfree("", &nclust, &f);
    xprintf("free:%d\n",nclust*f->csize*512/1024);
  }else if(argc!=0){
    char execution[PARAM_LEN];
    xsprintf(execution,"/bin/%s",argv[0]);
    aoutload(execution,argc,argv,functiontable,0);
    return;
  }
}

    /* const char *argv[]={"a.out","test.c","test.asm"}; */
    /* if(smlrcmain(sizeof(argv)/sizeof(argv[0]), argv)==0){ */
    /*   puts("compilation finished"); */
    /* }else{ */
    /*   puts("compilation failure?"); */
    /*   continue; */
    /* } */
  
    /* char buff[2048]; */
    /* int br; */
    /* FIL *fp; */
    
    /* fp = fopen("test.asm","r"); */
    /* if(!fp){ */
    /*   xprintf("file not generated\n"); */
    /* }else{ */
    /*   f_read(fp,  buff, sizeof(buff), &br); */
    /*   buff[br] = 0; */
    /* } */
    /* i=0; */
    /* while(buff[i]){ */
    /*   printchar(buff[i++]); */
    /* } */
    /* fcloseall(); */
  
    /* const char *argv2[]={"","-o","test.o","test.asm"}; */
    /* if(asmain(4,argv2)==0){ */
    /*   puts("assemble finished"); */
    /* }else{ */
    /*   puts("err occered while assemble"); */
    /*   continue; */
    /* } */
    /* fp = fopen("test.o","r"); */
    /* if(fp==NULL)xprintf("test.o file not found"); */
    /* else puts("test.o exist"); */
    /* f_read(fp,  buff, sizeof(buff), &br); */
    /* xprintf("%d byte",br); */
    /* fcloseall(); */
    /* /\* f_unlink("test.asm"); *\/ */

    /* const char *argv3[]={"","-o","a.out","-e","startup","test.o"}; */
    /* if(ldmain(sizeof(argv3)/sizeof(argv3[0]),argv3,((unsigned)ARAM)+sizeof(FIL)*MAX_USER_APP_FILE)==0){ */
    /*   puts("ld finished"); */
    /* }else{ */
    /*   puts("err occered while ld"); */
    /*   continue; */
    /* } */
    /* fp = fopen("a.out","r"); */
    /* if(fp==NULL)xprintf("a.out file not found"); */
    /* else puts("a.out exist"); */
    /* f_read(fp,  buff, sizeof(buff), &br); */
    /* xprintf("%d byte\n",br); */
    /* fcloseall(); */
