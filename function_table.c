#include "ps2keyboard.h"
#include "videoout.h"
#include "keyinput.h"
#include "ff_wrap.h"
#include <stdint.h>

char getCharactorCode(void);
void cls(void);
void wait60thsec(unsigned short n);

const unsigned functiontable[]={
  /*0x00*/(uint32_t)xprintf,
  /*0x01*/(uint32_t)xsprintf,

  /*0x02*/(uint32_t)fopen,
  /*0x03*/(uint32_t)fread,
  /*0x04*/(uint32_t)f_getcwd,
  /*0x05*/(uint32_t)f_opendir,
  /*0x06*/(uint32_t)f_readdir,
  /*0x07*/(uint32_t)f_findfirst,
  /*0x08*/(uint32_t)f_findnext,
  /*0x09*/(uint32_t)f_chdir,
  /*0x0A*/(uint32_t)f_unlink,
  /*0x0B*/(uint32_t)f_rename,
  /*0x0C*/(uint32_t)f_getfree,
  /*0x0D*/(uint32_t)f_mkdir,
  /*0x0E*/(uint32_t)fclose,
  /*0x0F*/(uint32_t)fwrite,

  /*0x10*/(uint32_t)setcursor,
  /*0x11*/(uint32_t)setcursorcolor,
  /*0x12*/(uint32_t)getCharactorCode,
  /*0x13*/(uint32_t)cls,
  /*0x14*/(uint32_t)wait60thsec,
  0,0,0,0,0,0,0,0,0,0,0,
  /*0x20*/(uint32_t)TVRAM,
  /*0x21*/(uint32_t)&vkey,
  /*0x22*/(uint32_t)ps2readkey,
  /*0x23*/(uint32_t)getcursorchar,
  /*0x24*/(uint32_t)resetcursorchar,
  /*0x25*/(uint32_t)blinkcursorchar,
  /*0x26*/(uint32_t)&cursor,
  /*0x27*/(uint32_t)&cursorcolor,
  /*0x28*/(uint32_t)&insertmode,
  /*0x29*/(uint32_t)&set_width,
  /*0x2A*/(uint32_t)&fontp,
  /*0x2B*/(uint32_t)&startPCG,
  /*0x2C*/(uint32_t)&stopPCG,
  /*0x2D*/(uint32_t)&ps2keystatus[0],
  /*0x2E*/(uint32_t)&set_palette,
  /*0x2F*/0,
  /*0x30*/(uint32_t)&set_graphmode,
  /*0x31*/(uint32_t)&g_set_palette,
  /*0x32*/(uint32_t)&gVRAM,
};
