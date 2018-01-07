#include "ps2keyboard.h"
#include "videoout.h"
#include "keyinput.h"
#include "ff_wrap.h"
#include <stdint.h>

char getCharactorCode(void);
void cls(void);
void wait60thsec(unsigned short n);
void exit(int );
void memcpy(void*,void*,int);
void memset(void*,int,int);
int inputfilepath(char *,char);

void mem4copy(int *dest,int *src,int n){
  int i;
  for(i=0;i<n;i++){
    *dest++=*src++;
  }
}

const unsigned functiontable[]={
  /*general functions 0x00-0x0F*/
  /*0x00*/(uint32_t)xprintf,
  /*0x01*/(uint32_t)xsprintf,
  /*0x02*/mem4copy,
  /*0x03*/(uint32_t)wait60thsec,
  /*0x04*/(uint32_t)cls,
  /*0x05*/&drawcount,
  /*0x06*/exit,
  /*0x07*/memcpy,
  /*0x08*/memset,
  /*0x09*/0,
  /*0x0A-0x0F*/0,0,0,0,0,0,

  /*file system 0x10-0x2F*/
  /*0x10*/(uint32_t)fopen,
  /*0x11*/(uint32_t)fread,
  /*0x12*/(uint32_t)f_getcwd,
  /*0x13*/(uint32_t)f_opendir,
  /*0x14*/(uint32_t)f_readdir,
  /*0x15*/(uint32_t)f_findfirst,
  /*0x16*/(uint32_t)f_findnext,
  /*0x17*/(uint32_t)f_chdir,
  /*0x18*/(uint32_t)f_unlink,
  /*0x19*/(uint32_t)f_rename,
  /*0x1a*/(uint32_t)f_getfree,
  /*0x1b*/(uint32_t)f_mkdir,
  /*0x1c*/(uint32_t)fclose,
  /*0x1d*/(uint32_t)fwrite,
  /*0x1e*/(uint32_t)f_lseek,
  /*0x1f*/(uint32_t)f_chdrive,
  /*0x20*/(uint32_t)f_printf,
  /*0x21*/(uint32_t)f_putc,
  /*0x22*/(uint32_t)f_puts,
  /*0x23*/(uint32_t)f_gets,
  /*0x24*/(uint32_t)f_truncate,
  /*0x25*/(uint32_t)f_stat,
  /*0x26*/(uint32_t)f_chmod,
  /*0x27*/(uint32_t)f_utime,
  /*0x28*/0,
  /*0x29*/0,
  /*0x2A-0x2F*/0,0,0,0,0,0,

  /* Text functions 0x30-0x3F*/
  /*0x30*/(uint32_t)setcursor,
  /*0x31*/(uint32_t)setcursorcolor,
  /*0x32*/(uint32_t)&TVRAM,
  /*0x33*/(uint32_t)set_width,
  /*0x34*/(uint32_t)set_palette,
  /*0x35*/(uint32_t)&cursor,
  /*0x36*/(uint32_t)&cursorcolor,
  /*0x37*/(uint32_t)&insertmode,
  /*0x38*/(uint32_t)&fontp,
  /*0x39*/(uint32_t)startPCG,
  /*0x3a*/(uint32_t)stopPCG,
  0,0,0,0,0,

  /*keyboard functions 0x40-0x4F*/
  /*0x40*/(uint32_t)&vkey,
  /*0x41*/(uint32_t)ps2readkey,
  /*0x42*/(uint32_t)getcursorchar,
  /*0x43*/(uint32_t)resetcursorchar,
  /*0x44*/(uint32_t)blinkcursorchar,
  /*0x45*/(uint32_t)&ps2keystatus[0],
  /*0x46*/(uint32_t)getCharactorCode,
  /*0x47*/(uint32_t)inputchar,
  /*0x48*/(uint32_t)inputfilepath,
  /*0x49-0x4F*/0,0,0,0,0,0,0,

  /*graphic mode 0x50-0x5F*/
  /*0x50*/(uint32_t)set_graphmode,
  /*0x51*/(uint32_t)g_set_palette,
  /*0x52*/(uint32_t)&gVRAM,
  /*0x53*/(uint32_t)g_pset,
  /*0x54*/(uint32_t)g_putbmpmn,
  /*0x55*/(uint32_t)g_clrbmpmn,
  /*0x56*/(uint32_t)g_gline,
  /*0x57*/(uint32_t)g_hline,
  /*0x58*/(uint32_t)g_circle,
  /*0x59*/(uint32_t)g_boxfill,
  /*0x5A*/(uint32_t)g_putfont,
  /*0x5B*/(uint32_t)g_printstr,
  /*0x5C*/(uint32_t)g_color,

  /**/
};
