#ifndef __FILE_H
#define __FILE_H

//don't worry about these contents.
#define FILE void

#define fopen ((FILE*(*)(const char*,const char*))g_func_p[0x2 ])
#define fread ((int (*)(void*,int,int,FILE*))g_func_p[0x3 ])
#define getcwd ((int(*)(char *,int))g_func_p[0x4 ])
#define chdir ((int (*)(const char*))g_func_p[0x9 ])
#define unlink ((int (*)(const char*))g_func_p[0xA ])
#define rename ((int (*)(const char*,const char*))g_func_p[0xB ])
#define getfree ((int (*)(const char *path,DWORD *dclust,FATFS **fs))g_func_p[0xC ])
#define mkdir ((int (*)(const char *path))g_func_p[0xD ]])
#define fclose ((int(*)(FILE *))g_func_p[0xE ])
#define fwrite ((int(*)(const void*,int,int,FILE*))g_func_p[0xF ])

#endif
