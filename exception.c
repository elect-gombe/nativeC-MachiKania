/*
   This file is provided under the LGPL license ver 2.1.
   Written by K.Tanaka & Katsumi
   http://www.ze.em-net.ne.jp/~kenken/index.html
   http://hp.vector.co.jp/authors/VA016157/
*/

#include <xc.h>

/*
	Enable following line if memory dump is needed when exception occurs.
*/

//#define DUMPFILE "~~MEMORY.DMP"

#ifdef DUMPFILE
void dumpMemory(){
	unsigned int i;
	FSFILE *fp;
	printstr("\n"DUMPFILE" ");
	if(FSInit()==FALSE){
		printstr("cannot be created.\n");
		return;
	}
	fp=FSfopen(DUMPFILE,"w");
	if(fp==NULL) {
		printstr("not saved.\n");
		return;
	}
	for(i=0;i<PERSISTENT_RAM_SIZE;i+=512){
		if (FSfwrite(&RAM[i],1,512,fp)<512) break;
	}
	FSfclose(fp);	
	printstr("saved.\n");
}
#else
void dumpMemory(){}
#endif //ifdef DUMPFILE
//char RAM[RAMSIZE] __attribute__((persistent,address(0xA0000000+PIC32MX_RAMSIZE-PERSISTENT_RAM_SIZE)));

unsigned g_ex_data[64]  __attribute__((persistent,address(0xA0000000+0x10000-256)));
#define FILENAME_FLASH_ADDRESS 0x9D005800
#define PIC32MX_RAMSIZE 0x10000
#define PIC32MX_FLASHSIZE 0x40000

void _general_exception_handler (int place){
  int i;
  // $v1 is g_ex_data
  asm volatile("la $v1,%0"::"i"(&g_ex_data[0]));
  // Prepare proper stack area before SoftReset
  asm volatile("addiu $sp,$v1,0xfff0");
  // g_ex_data[2]=$s6
  asm volatile("sw $s6,8($v1)");
  // g_ex_data[3]=Cause
  asm volatile("mfc0 $v0,$13");
  asm volatile("sw $v0,12($v1)");
  // g_ex_data[4]=EPC 
  asm volatile("mfc0 $v0,$14");
  asm volatile("sw $v0,16($v1)");
  // Exception occured
  g_ex_data[0]=1;
  // g_s6
  // Clear 2 MLB bits of EPC
  g_ex_data[4]&=0xfffffffc;
  g_ex_data[5] = place;
  // If EPC is within RAM, store data in exception area.
  // Wait until all buttons are released and reset MachiKania.
#ifdef __DEBUG
  asm volatile("j 0xBFC00000");
#else
  asm volatile("j SoftReset");
#endif
}

void blue_screen(void){
  int i,j,s6,s6g;
  if (RCONbits.POR || RCONbits.EXTR) {
    // After power on or reset. Reset flags and return.
    RCONbits.POR=0;
    RCONbits.EXTR=0;
    for(i=0;i<sizeof(g_ex_data)/sizeof(g_ex_data[0]);i++){
      // Reset all RAM area including g_ex_data[]
      g_ex_data[i]=0;
    }
    return;
  } else if (g_ex_data[0]==0) {
    // No exception found.
    return;
  }
  // Exception occured before SoftReset().
  // Prepare data
  s6=g_ex_data[2];
  s6g=g_ex_data[1];
  s6=s6&0x7fffffff;
  s6g=s6g&0x7fffffff;
  //set_bgcolor(255,0,0);
  xprintf("STOP");
  xprintf("\nException at ");
  xprintf("0x%08x",g_ex_data[4]);
  xprintf("\n      Cause: ");
  xprintf("0x%08x",g_ex_data[3]);
  xprintf("\n ");
  switch((g_ex_data[3]>>2)&0x1f){
  case 0:  xprintf("(Interrupt)"); break;
  case 1:  xprintf("(TLB modification)"); break;
  case 2:  xprintf("(TLB load/fetch)"); break;
  case 3:  xprintf("(TLB store)"); break;
  case 4:  xprintf("(Address load/fetch error )"); break;
  case 5:  xprintf("(Address store error)"); break;
  case 6:  xprintf("(Bus fetch error)"); break;
  case 7:  xprintf("(Bus load/store error)"); break;
  case 8:  xprintf("(Syscall)"); break;
  case 9:  xprintf("(Breakpoint)"); break;
  case 10: xprintf("(Reserved instruction)"); break;
  case 11: xprintf("(Coprocessor Unusable)"); break;
  case 12: xprintf("(Integer Overflow)"); break;
  case 13: xprintf("(Trap)"); break;
  case 23: xprintf("(Reference to Watch address)"); break;
  case 24: xprintf("(Machine check)"); break;
  default: xprintf("(Unknown)"); break;
  }
  xprintf("\n");
  xprintf("Reset MachiKania to contine.\n\n");
  xprintf("\n");
  xprintf("last pc was %08x",g_ex_data[5]);

#ifndef __DEBUG
  dumpMemory();
#endif
  while(1) asm("wait");
}
