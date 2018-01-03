#ifndef __SYS_H
#define __SYS_H

unsigned *g_func_p;
int main(int argc,char **argv);
int startup(int argc,char **argv,unsigned* fp){
  g_func_p = fp;
  return main(argc,argv);
}

#define printf ((int(*)(const char*,...))g_func_p[0x0 ])
#define sprintf ((int(*)(char*,const char*,...))g_func_p[0x1 ])

#define setcursor ((void(*)(unsigned char,unsigned char,unsigned char))g_func_p[0x10 ])
#define setcursorcolor ((void(*)(unsigned char))g_func_p[0x11 ])
#define getchar ((char(*)(void))g_func_p[0x12])
#define cls ((void(*)(void))g_func_p[0x13])
#define wait60thsec ((void(*)(int ))g_func_p[0x14])

#define ps2readkey ((unsigned char (*)(void))g_func_p[0x13])
#define getcursorchar ((void(*)(void))g_func_p[0x14])

#define TVRAM ((unsigned short*)g_func_p[0x20])
#define vkey ((unsigned short*)g_func_p[0x21])
#define cursor ((unsigned char*)g_func_p[0x26])
#define cursorcolor ((unsigned char*)g_func_p[0x27])

#define set_palette ((void (*)(unsigned char,unsigned char,unsigned char,unsigned char))g_func_p[0x28])
#define set_width ((void (*)(unsigned char))g_func_p[0x29])
#define fontp ((unsigned char *)g_func_p[0x2A])

#define startPCG ((void (*)(unsigned char*,int))g_func_p[0x2B])
#define stopPCG ((void (*)(void))g_func_p[0x2C])

#define vkey

#endif
