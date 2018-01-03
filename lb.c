/*
Copyright (c) 2013-2016, Alexey Frunze
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ff.h"
#include "videoout.h"
#include "ff_wrap.h"
#include "nativec.h"

#ifndef __APPLE__
/*****************************************************************************/
/*                                                                           */
/*                             minimal stdlibc                               */
/*                                                                           */
/*            Just enough code for Smaller C to compile into a               */
/*               16/32-bit DOS/Windows/Linux/RetroBSD "EXE".                 */
/*                                                                           */
/*****************************************************************************/

#ifndef __SMALLER_C__
#error must be compiled with Smaller C
#endif

#ifndef __SMALLER_C_SCHAR__
#ifndef __SMALLER_C_UCHAR__
#error __SMALLER_C_SCHAR__ or __SMALLER_C_UCHAR__ must be defined
#endif
#endif

#ifndef __SMALLER_C_16__
#ifndef __SMALLER_C_32__
#error __SMALLER_C_16__ or __SMALLER_C_32__ must be defined
#endif
#endif

#ifndef _RETROBSD
#ifndef _LINUX
#ifndef _WIN32
#ifndef _DOS
#define _DOS
#endif
#endif
#endif
#endif

#define NULL 0
#ifndef EOF
#define EOF (-1)
#endif
#ifndef FILE
#define FILE void
#endif
#define putchar(x) printchar(x)
#define puts(str) printf("%s\n",str)

void exit(int vres){
  printstr("error occered! type `any key` to recover!\n");
  printnum(vres);
  start_composite();
  inputchar();
  main();
}

const 
unsigned char __chartype__[1 + 256] =
{
  0x00, // for EOF=-1
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, 0x01,0x03,0x03,0x03,0x03,0x03,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x02,0x04,0x04,0x04,0x04,0x04,0x04,0x04, 0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
  0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08, 0x08,0x08,0x04,0x04,0x04,0x04,0x04,0x04,
  0x04,0x50,0x50,0x50,0x50,0x50,0x50,0x10, 0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10, 0x10,0x10,0x10,0x04,0x04,0x04,0x04,0x04,
  0x04,0x60,0x60,0x60,0x60,0x60,0x60,0x20, 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20, 0x20,0x20,0x20,0x04,0x04,0x04,0x04,0x01,
};

int isspace(int c)
{
  return __chartype__[c + 1] & 0x02;
}

int isdigit(int c)
{
  return __chartype__[c + 1] & 0x08;
}

int isalpha(int c)
{
  return __chartype__[c + 1] & 0x30;
}

int isalnum(int c)
{
  return __chartype__[c + 1] & 0x38;
}

int atoi(char* s)
{
  // simplified version
  int r = 0;
  int neg = 0;
  if (*s == '-')
  {
    neg = 1;
    ++s;
  }
  else if (*s == '+')
  {
    ++s;
  }
  while (isdigit(*s & 0xFFu))
    r = r * 10u + *s++ - '0';
  if (neg)
    r = -r;
  return r;
}

unsigned strlen(char* str)
{
#ifndef __SMALLER_C_16__
  char* s;

  if (str == NULL)
    return 0;

  for (s = str; *s; ++s);

  return s - str;
#else
  asm("mov di, [bp+4]\n"
      "mov cx, -1\n"
      "xor al, al\n"
      "cld\n"
      "repnz scasb\n"
      "mov ax, cx\n"
      "not ax\n"
      "dec ax");
#endif
}

char* strcpy(char* dst, char* src)
{
#ifndef __SMALLER_C_16__
  char* p = dst;

  while ((*p++ = *src++) != 0);
#else
  asm("mov di, [bp+6]\n"
      "mov si, di\n"
      "mov cx, -1\n"
      "xor al, al\n"
      "cld\n"
      "repnz scasb\n"
      "not cx\n"
      "mov di, [bp+4]\n"
      "rep movsb");
#endif

  return dst;
}

char* strchr(char* s, int c)
{
#ifndef __SMALLER_C_16__
  char ch = c;

  while (*s)
  {
    if (*s == ch)
      return s;
    ++s;
  }

  if (!ch)
    return s;

  return NULL;
#else
  asm("mov si, [bp+4]\n"
      "mov bl, [bp+6]\n"
      "xor ax, ax\n"
      "cld\n"
      "strchrlp:\n"
      "lodsb\n"
      "cmp al, bl\n"
      "je strchrmch\n"
      "or al, al\n"
      "jnz strchrlp\n"
      "jmp strchrend");
  asm("strchrmch:\n"
      "lea ax, [si-1]\n"
      "strchrend:");
#endif
}

int strcmp(const char* s1, const char* s2)
{
#ifndef __SMALLER_C_16__
  while (*s1 == *s2)
  {
    if (!*s1)
      return 0;
    ++s1;
    ++s2;
  }

  return (*s1 & 0xFF) - (*s2 & 0xFF);
#else
  asm("mov di, [bp+6]\n"
      "mov si, di\n"
      "mov cx, -1\n"
      "xor ax, ax\n"
      "mov bx, ax\n"
      "cld\n"
      "repnz scasb\n"
      "not cx");
  asm("mov di, [bp+4]\n"
      "repe cmpsb\n"
      "mov al, [di-1]\n"
      "mov bl, [si-1]\n"
      "sub ax, bx");
#endif
}

int strncmp(char* s1, char* s2, unsigned n)
{
#ifndef __SMALLER_C_16__
  if (!n)
    return 0;

  do
  {
    if (*s1 != *s2++)
      return (*s1 & 0xFF) - (*--s2 & 0xFF);
    if (!*s1++)
      break;
  } while (--n);

  return 0;
#else
  asm("mov cx, [bp+8]\n"
      "xor ax, ax\n"
      "jcxz strncmpend\n"
      "mov bx, ax\n"
      "mov di, [bp+6]\n"
      "mov si, di\n"
      "cld\n"
      "repnz scasb\n"
      "sub cx, [bp+8]\n"
      "neg cx");
  asm("mov di, [bp+4]\n"
      "repe cmpsb\n"
      "mov al, [di-1]\n"
      "mov bl, [si-1]\n"
      "sub ax, bx\n"
      "strncmpend:");
#endif
}

void* memmove(void* dst, void* src, unsigned n)
{
#ifndef __SMALLER_C_16__
  char* d = dst;
  char* s = src;

  if (s < d)
  {
    s += n;
    d += n;
    while (n--)
      *--d = *--s;
  }
  else
  {
    while (n--)
      *d++ = *s++;
  }
#else
  if (src < dst)
  {
    asm("mov di, [bp+4]\n"
        "mov si, [bp+6]\n"
        "mov cx, [bp+8]\n"
        "add di, cx\n"
        "dec di\n"
        "add si, cx\n"
        "dec si\n"
        "std\n"
        "rep movsb");
  }
  else
  {
    asm("mov di, [bp+4]\n"
        "mov si, [bp+6]\n"
        "mov cx, [bp+8]\n"
        "cld\n"
        "rep movsb");
  }
#endif
  return dst;
}

void* memcpy(void* dst, void* src, unsigned n)
{
#ifndef __SMALLER_C_16__
  char* p1 = dst;
  char* p2 = src;

  while (n--)
    *p1++ = *p2++;
#else
  asm("mov di, [bp+4]\n"
      "mov si, [bp+6]\n"
      "mov cx, [bp+8]\n"
      "shr cx, 1\n"
      "cld\n"
      "rep movsw\n"
      "rcl cx, 1\n"
      "rep movsb");
#endif
  return dst;
}

void* memset(void* s, int c, unsigned n)
{
#ifndef __SMALLER_C_16__
  char* p = s;

  while (n--)
    *p++ = c;
#else
  asm("mov di, [bp+4]\n"
      "mov al, [bp+6]\n"
      "mov ah, al\n"
      "mov cx, [bp+8]\n"
      "shr cx, 1\n"
      "cld\n"
      "rep stosw\n"
      "rcl cx, 1\n"
      "rep stosb");
#endif
  return s;
}

#ifdef __SMALLER_C_32__
int memcmp(void* s1, void* s2, unsigned n)
{
  if (n)
  {
    unsigned char *p1 = s1, *p2 = s2;
    do
    {
      if (*p1++ != *p2++)
        return (*--p1 - *--p2);
    } while (--n);
  }
  return 0;
}
#endif

int __putchar__(char** buf, FILE* stream, int c)
{
  if (buf)
  {
    *(*buf)++ = c;
  }
  else if (stream)
  {
    fputc(c, stream);
  }
  else
  {
    putchar(c);
  }
  return 1;
}

int __vsprintf__(char** buf, FILE* stream, char* fmt, void* vl)
{
  // Simplified version!
  // No I/O error checking!
  int* pp = vl;
  int cnt = 0;
  char* p;
  char* phex;
  char s[1/*sign*/+10/*magnitude*/+1/*\0*/]; // up to 11 octal digits in 32-bit numbers
  char* pc;
  int n, sign, msign;
  int minlen, len;
  int leadchar = ' ';

  for (p = fmt; *p != '\0'; ++p)
  {
    if (*p != '%' || p[1] == '%')
    {
      __putchar__(buf, stream, *p);
      p = p + (*p == '%');
      ++cnt;
      continue;
    }
    ++p;
    minlen = 0;
    msign = 0;
    if (*p == '+') { msign = 1; ++p; }
    else if (*p == '-') { msign = -1; ++p; }
    if (isdigit(*p & 0xFFu))
    {
      if (*p == '0')
        leadchar = '0';
      else
        leadchar = ' ';
      while (isdigit(*p & 0xFFu))
        minlen = minlen * 10 + *p++ - '0';
      if (msign < 0)
        minlen = -minlen;
      msign = 0;
    }
    if (!msign)
    {
      if (*p == '+') { msign = 1; ++p; }
      else if (*p == '-') { msign = -1; ++p; }
    }
    phex = "0123456789abcdef";
    switch (*p)
    {
    case 'c':
      while (minlen > 1) { __putchar__(buf, stream, ' '); ++cnt; --minlen; }
      __putchar__(buf, stream, *pp++);
      while (-minlen > 1) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      ++cnt;
      break;
    case 's':
      pc = (char*)*pp++;
      len = 0;
      if (pc)
        len = strlen(pc);
      while (minlen > len) { __putchar__(buf, stream, ' '); ++cnt; --minlen; }
      if (len)
        while (*pc != '\0')
        {
          __putchar__(buf, stream, *pc++);
          ++cnt;
        }
      while (-minlen > len) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      break;
    case 'i':
    case 'd':
      pc = &s[sizeof s - 1];
      *pc = '\0';
      len = 0;
      n = *pp++;
      sign = 1 - 2 * (n < 0);
      do
      {
        *--pc = '0' + (n - n / 10 * 10) * sign;
        n = n / 10;
        ++len;
      } while (n);
      if (sign < 0)
      {
        *--pc = '-';
        ++len;
      }
      else if (msign > 0)
      {
        *--pc = '+';
        ++len;
        msign = 0;
      }
      while (minlen > len) { __putchar__(buf, stream, leadchar); ++cnt; --minlen; }
      while (*pc != '\0')
      {
        __putchar__(buf, stream, *pc++);
        ++cnt;
      }
      while (-minlen > len) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      break;
    case 'u':
      pc = &s[sizeof s - 1];
      *pc = '\0';
      len = 0;
      n = *pp++;
      do
      {
        unsigned nn = n;
        *--pc = '0' + nn % 10;
        n = nn / 10;
        ++len;
      } while (n);
      if (msign > 0)
      {
        *--pc = '+';
        ++len;
        msign = 0;
      }
      while (minlen > len) { __putchar__(buf, stream, leadchar); ++cnt; --minlen; }
      while (*pc != '\0')
      {
        __putchar__(buf, stream, *pc++);
        ++cnt;
      }
      while (-minlen > len) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      break;
    case 'X':
      phex = "0123456789ABCDEF";
      // fallthrough
    case 'p':
    case 'x':
      pc = &s[sizeof s - 1];
      *pc = '\0';
      len = 0;
      n = *pp++;
      do
      {
        *--pc = phex[n & 0xF];
        n = (n >> 4) & ((1 << (8 * sizeof n - 4)) - 1); // drop sign-extended bits
        ++len;
      } while (n);
      while (minlen > len) { __putchar__(buf, stream, leadchar); ++cnt; --minlen; }
      while (*pc != '\0')
      {
        __putchar__(buf, stream, *pc++);
        ++cnt;
      }
      while (-minlen > len) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      break;
    case 'o':
      pc = &s[sizeof s - 1];
      *pc = '\0';
      len = 0;
      n = *pp++;
      do
      {
        *--pc = '0' + (n & 7);
        n = (n >> 3) & ((1 << (8 * sizeof n - 3)) - 1); // drop sign-extended bits
        ++len;
      } while (n);
      while (minlen > len) { __putchar__(buf, stream, leadchar); ++cnt; --minlen; }
      while (*pc != '\0')
      {
        __putchar__(buf, stream, *pc++);
        ++cnt;
      }
      while (-minlen > len) { __putchar__(buf, stream, ' '); ++cnt; ++minlen; }
      break;
    default:
      return -1;
    }
  }

  return cnt;
}

int vprintf(char* fmt, void* vl)
{
  return __vsprintf__(NULL, NULL, fmt, vl);
}

int vsprintf(char* buf, char* fmt, void* vl)
{
  return __vsprintf__(&buf, NULL, fmt, vl);
}

#endif /*__APPLE__*/

