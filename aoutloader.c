#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include "ff.h"
#include "ps2keyboard.h"
#include "nativec.h"

//read aout format.

struct  aoutexec {
  uint32_t a_midmag;      /* magic number */
  uint32_t a_text;        /* size of text segment */
  uint32_t a_data;        /* size of initialized data */
  uint32_t a_bss;         /* size of uninitialized data */
  uint32_t a_reltext;     /* size of text relocation info */
  uint32_t a_reldata;     /* size of data relocation info */
  uint32_t a_syms;        /* size of symbol table */
  uint32_t a_entry;       /* entry point */
};

struct memsect {
    unsigned int vaddr;
    unsigned len;
};

struct exec{
    unsigned int ep_taddr, ep_tsize, ep_daddr, ep_dsize;
    struct memsect text, data, bss, heap, stack;
};

#define _FRAME_A0 0
#define _FRAME_A1 1
#define _FRAME_A2 2
#define _FRAME_A3 3
#define _FRAME_SP 4
#define _FRAME_FP 5
#define _FRAME_NUM 6

#define NO_ADDR 0xFFFFFFFF

/*
 * filename ... a.out format execution file
 * argc     ... arg count
 * argv     ... arg vector
 * lft      ... library function table
 *
 * this execution file is to be not used gp.
 * compile with option -mno-gpopt to use absolute address.
 */

#define _XMIT_MESSAGE 0

char getCharactorCode(void);

int aoutload(const char *filename,int argc,char **argv,void *lft,int msg){
  FIL *fp;
  struct aoutexec aout;
  struct exec exe;
  int br;
  uint32_t frame[_FRAME_NUM];
  int i;

  // close all files
  fcloseall();
  RAM = fframset(ARAM,2);
  
  fp = fopen(filename,"r");
  if(fp==NULL){
    xprintf("file not found\n");
    fcloseall();
    RAM = fframset(ARAM,10);
    return -1;
  }

  f_read(fp, &aout, sizeof(struct aoutexec), &br);
  if(msg){
  xprintf("Exec file header:\n");
  xprintf("a_midmag =  0x%08x\n", aout.a_midmag);     /* magic number */
  xprintf("a_text =    %d\n",  aout.a_text);       /* size of text segment */
  xprintf("a_data =    %d\n",  aout.a_data);       /* size of initialized data */
  xprintf("a_bss =     %d\n",  aout.a_bss);        /* size of uninitialized data */
  xprintf("a_reltext = %d\n",  aout.a_reltext);    /* size of text relocation info */
  xprintf("a_reldata = %d\n",  aout.a_reldata);    /* size of data relocation info */
  xprintf("a_syms =    %d\n",  aout.a_syms);       /* size of symbol table */
  xprintf("a_entry =   0x%08x\n", aout.a_entry);      /* entry point */
  }
  
  exe.text.vaddr = exe.heap.vaddr = NO_ADDR;
  exe.text.len = exe.heap.len = 0;

  exe.data.vaddr = (unsigned int)&RAM[0];
  exe.data.len = aout.a_text + aout.a_data;
  exe.bss.vaddr = exe.data.vaddr + exe.data.len;
  exe.bss.len = aout.a_bss;
  exe.heap.vaddr = exe.bss.vaddr + exe.bss.len;
  exe.heap.len = 0;
  exe.stack.len = 2048;
  exe.stack.vaddr = (unsigned int)&RAM[0] + AVAILABLE_MEM_INCLUDING_STACK - exe.stack.len;

  if(msg){
  xprintf("text =  0x%08x..0x%08x, len=%d\n", exe.text.vaddr, exe.text.vaddr+exe.text.len, exe.text.len);
  xprintf("data =  0x%08x..0x%08x, len=%d\n", exe.data.vaddr, exe.data.vaddr+exe.data.len, exe.data.len);
  xprintf("bss =   0x%08x..0x%08x, len=%d\n", exe.bss.vaddr, exe.bss.vaddr+exe.bss.len, exe.bss.len);
  xprintf("heap =  0x%08x..0x%08x, len=%d\n", exe.heap.vaddr, exe.heap.vaddr+exe.heap.len, exe.heap.len);
  xprintf("stack = 0x%08x..0x%08x, len=%d\n", exe.stack.vaddr, exe.stack.vaddr+exe.stack.len, exe.stack.len);
  }

  /* inputchar(); //1文字入力待ち   */

  f_read (fp,(void*)exe.data.vaddr,  exe.data.len, &br);

  memset((void*)exe.bss.vaddr,0,exe.bss.len);

/* #if _XMIT_MESSAGE */
/*   getCharactorCode(); */

/*   for(i=0;i<(exe.data.len+aout.a_bss)/4;i++){ */
/*     xprintf("0x%08x ",((uint32_t*)exe.data.vaddr)[i]); */
/*     if(i%3==2)xprintf("\n"); */
/*   } */
/* #endif */
  
  frame[_FRAME_A0] = (uint32_t)argc;
  frame[_FRAME_A1] = (uint32_t)argv;
  frame[_FRAME_A2] = (uint32_t)lft;
  //  frame[_FRAME_SP] = (uint32_t)exe.stack.vaddr;// to use system stack.
  frame[_FRAME_FP] = (uint32_t)exe.stack.vaddr;
  
  /* xprintf("press `any-key`."); */
  /* inputchar(); //1文字入力待ち */
  fclose(fp);

  if(msg){
  getCharactorCode();
  }
  aoutrun(frame,aout.a_entry);
  
  fcloseall();
  /* int exesz,available_mem; */
  /* available_mem = AVAILABLE_MEM_INCLUDING_STACK; */
  /* exesz = aout.a_bss+exe.data.len; */
  /* xprintf("=========size infomation========\n this program size was %d byte.\n[%d/%d is available]\n",exesz,RAMSIZE-exesz,RAMSIZE); */
  
  return 0;
}


/*
 * 
 */
void *g_end_addr;
int aoutrun(void *frame,void *start){
  static unsigned int stored_sp;
  // Store s0-s7, fp, and ra in stacks
  asm volatile("#":::"s0");
  asm volatile("#":::"s1");
  asm volatile("#":::"s2");
  asm volatile("#":::"s3");
  asm volatile("#":::"s4");
  asm volatile("#":::"s5");
  asm volatile("#":::"s6");
  asm volatile("#":::"s7");
  asm volatile("#":::"30");
  asm volatile("#":::"ra");
  // Store sp in stored_sp
  asm volatile("la $v0,%0"::"i"(&stored_sp));
  asm volatile("sw $sp,0($v0)");
  // Shift sp for safety
  asm volatile("addiu $sp,$sp,-8");
  // Store end address in g_end_addr
  asm volatile("la $v0,%0"::"i"(&g_end_addr));
  asm volatile("la $v1,labelaout");
  asm volatile("sw $v1,0($v0)");
  // Set fp and execute program

  asm volatile("addu $t0,$zero,$a0");
  asm volatile("addu $t1,$zero,$a1");
  asm volatile("lw $a0,0($t0)");
  asm volatile("lw $a1,4($t0)");
  asm volatile("lw $a2,8($t0)");
  /* asm volatile("lw $sp,16($t0)"); */
  asm volatile("lw $fp,20($t0)");
  
  asm volatile("jalr $31,$t1");
  asm volatile("nop");      //delay slot.
  // Restore sp from stored_sp
  asm volatile("labelaout:");
  asm volatile("la $t0,%0"::"i"(&stored_sp));
  asm volatile("lw $sp,0($t0)");
  // Restore registers from stack and return
  return;
}

/* wrapper of vkey reading.
 * Enter to \n, BackSpace to \b, Tab to \t, Esc to \033
 */
char getCharactorCode(void){
  char m;
  m=cursorinputchar();
  switch(vkey&0xFF){
  case VK_RETURN:
  case VK_SEPARATOR:
    m = '\n';
    break;
  case VK_BACK:
    m = '\b';
    break;
  case VK_TAB:
    m = '\t';
    break;
  case VK_ESCAPE:
    m = '\033';
    break;
  case VK_NUMPAD0:
  case VK_NUMPAD1:
  case VK_NUMPAD2:
  case VK_NUMPAD3:
  case VK_NUMPAD4:
  case VK_NUMPAD5:
  case VK_NUMPAD6:
  case VK_NUMPAD7:
  case VK_NUMPAD8:
  case VK_NUMPAD9:
    m = (vkey&0xFF)-VK_NUMPAD0+'0';
  default:
    break;
  }
  
  return m;
}
