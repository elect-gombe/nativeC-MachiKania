/*
 * Linker for RetroBSD, MIPS32 architecture.
 *
 * Copyright (C) 2011 Serge Vakulenko, <serge@vak.ru>
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */
#ifdef CROSS
#   include <sys/types.h>
#   include <sys/select.h>
#   include <sys/stat.h>
#   include <sys/time.h>
#   include <sys/fcntl.h>
#   include <sys/signal.h>
#   include <stdio.h>
#   include <string.h>
#   include <stdlib.h>
#   include <unistd.h>
#   define MAXNAMLEN 63
#else
#   include <string.h>
#   include <signal.h>
#   include <unistd.h>
#endif

#include "nativec.h"

#   define MAXNAMLEN 16
#include <stdarg.h>
#include "a.out.h"
#include "ar.h"
#include "ff.h"
#include "ff_wrap.h"
#include "ranlib.h"


#define W               4               /* word size in bytes */
#define BADDR           (RAM)          /* start address in memory */
#define SYMDEF          "__.SYMDEF"
#define IS_LOCSYM(s)    ((s)->n_name[0] == 'L' || \
                         (s)->n_name[0] == '.')
#define hexdig(c)       ((c)<='9' ? (c) - '0' : ((c)&7) + 9)

struct exec filhdr;             /* aout header */

#define HDRSZ sizeof(struct exec)

struct archdr {                 /* archive header */
	char *  ar_name;
	long	ar_date;
	int     ar_uid;
	int     ar_gid;
	int	ar_mode;
	long    ar_size;
} archdr;

FILE *text, *reloc;             /* input management */

                                /* output management */
FILE *outb, *toutb, *doutb, *troutb, *droutb, *soutb;

                                /* symbol management */
struct _local {
	unsigned locindex;              /* index to symbol in file */
	struct nlist *locsymbol;        /* ptr to symbol table */
};

#define NSYM            1000
#define NSYMPR          400
#define NLIBS           256
#define RANTABSZ        400

/* struct nlist cursym;            /\* current symbol *\/ */
/* struct nlist symtab [NSYM];     /\* table of symbols *\/ */
/* struct nlist **symhash [NSYM];  /\* pointers to hash table *\/ */
/* struct nlist *lastsym;          /\* last entered symbol *\/ */
/* struct nlist *hshtab [NSYM+2];  /\* hash table for symbols *\/ */
/* struct local local [NSYMPR]; */
/* int symindex;                   /\* next free entry of symbol table *\/ */
/* unsigned basaddr = BADDR;       /\* base address of loading *\/ */
/* struct ranlib rantab [RANTABSZ]; */
/* int rancount;                   /\* number of elements in rantab *\/ */

typedef struct {
  struct nlist cursym;            /* current symbol */
  struct nlist symtab [NSYM];     /* table of symbols */
  struct nlist **symhash [NSYM];  /* pointers to hash table */
  struct nlist *lastsym;          /* last entered symbol */
  struct nlist *hshtab [NSYM+2];  /* hash table for symbols */
  struct _local local [NSYMPR];
  int symindex;                   /* next free entry of symbol table */
  struct ranlib rantab [RANTABSZ];
  int rancount;                   /* number of elements in rantab */
  unsigned liblist [NLIBS], *libp;
  int _filenum;

  struct nlist *p_etext, *p_edata, *p_end, *p_gp, *entrypt;
  
  int     trace;                /* internal trace flag */
  int     xflag;                  /* discard local symbols */
  int     Xflag;                  /* discard locals starting with 'L' or '.' */
  int     Sflag;                  /* discard all except locals and globals*/
  int     rflag;                  /* preserve relocation bits, don't define commons */
  int     output_relinfo;
  int     sflag;                  /* discard all symbols */
  int     dflag;                  /* define common even with rflag */
  int     verbose;                /* verbose mode */
  
  unsigned tsize, dsize, bsize, ssize, nsym;

  unsigned ctrel, cdrel, cbrel;

  unsigned torigin, dorigin, borigin;

  unsigned gp;                    /* allocated address */

  int	ofilfnd;
  char	*filname;
  int	errlev;
  char    tfname [6];
  char	*ofilename;
} ld_t;

int delarg = 4;
unsigned basaddr;// = BADDR;       /* base address of loading */
unsigned gpoffset = 0x8000;     /* offset from data start */

ld_t *g_ld;

#define cursym g_ld->cursym
#define symtab g_ld->symtab
#define symhash g_ld->symhash
#define lastsym g_ld->lastsym
#define hshtab g_ld->hshtab
#define local g_ld->local
#define symindex g_ld->symindex
#define rantab g_ld->rantab
#define rancount g_ld->rancount
#define liblist g_ld->liblist
#define libp g_ld->libp
#define filenum g_ld->_filenum

#define trace g_ld->trace
#define xflag g_ld->xflag
#define Xflag g_ld->Xflag
#define Sflag g_ld->Sflag
#define rflag g_ld->rflag
#define output_relinfo g_ld->output_relinfo
#define sflag g_ld->sflag
#define dflag g_ld->dflag
#define verbose g_ld->verbose

#define tsize g_ld->tsize
#define dsize g_ld->dsize
#define bsize g_ld->bsize
#define ssize g_ld->ssize
#define nsym g_ld->nsym
#define ctrel g_ld->ctrel
#define cdrel g_ld->cdrel
#define cbrel g_ld->cbrel
#define torigin g_ld->torigin
#define dorigin g_ld->dorigin
#define borigin g_ld->borigin
#define gp g_ld->gp
#define ofilfnd g_ld->ofilfnd
#define filname g_ld->filname
#define errlev g_ld->errlev
#define tfname g_ld->tfname
#define ofilename g_ld->ofilename

#define p_etext g_ld->p_etext
#define p_edata g_ld->p_edata
#define p_end g_ld->p_end
#define p_gp g_ld->p_gp
#define entrypt g_ld->entrypt




/*
 * library management
 */


#define ALIGN(x,y)     ((x)+(y)-1-((x)+(y)-1)%(y))

#include <limits.h>

int _space_sign(const char *s, const char **endptr)
{
  while (isspace((unsigned char)*s))
    ++s;
  int sign = 0;
  switch (*s)
  {
  case '-':
    sign = -1;
    // fall through
  case '+':
    ++s;
    break;
  }
  *endptr = s;
  return sign;
}

long strtol(const char * __restrict__ s, char ** __restrict__ endptr, int radix)
{
  int sign = _space_sign(s, (const char**)&s);
  long result;

  if (s[0] == '0')
    {
      ++s;
      if ((s[1] | 0x20) == 'x')
	{
	  if (radix == 0 || radix == 16)
	    {
	      ++s;
	      radix = 16;
	    }
	}
      else if (radix == 0)
	radix = 8;
    }
  else if (radix == 0)
    radix = 10;

  int c;
  for (result = 0; c = tolower((unsigned char)*s), isdigit(c) || ('a' <= c && c <= 'z'); s++)
    {
      int d = isdigit(c) ? c - '0' : c - 'a' + 10;
      if (d >= radix)
	break;
      if (result > (LONG_MAX - d - sign) / radix)
	{
	  result = sign ? LONG_MIN : LONG_MAX;
	}
      else
	{
	  result = result * radix + d;
	}
    }

  if (endptr != NULL)
    *endptr = (char*)s;

  if (sign != 0)
    result = -result;
  return result;
}

static inline
int getc(FILE *fp){
  unsigned int br;
  unsigned char m;
  f_read(fp,&m,1,&br);
  if(br==0)return -1;
  return m;
}

static 
void putc(char m,FIL *fp){
  unsigned int br;
  f_write(fp,&m,1,&br);
  if(br==0)printf("err as.c:438");
}

static
unsigned int fgetword (f)
        register FILE *f;
{
        register unsigned int h;

        h = getc (f);
        h |= getc (f) << 8;
        h |= getc (f) << 16;
        h |= getc (f) << 24;
        return h;
}

static
void fputword (h, f)
        register unsigned int h;
        register FILE *f;
{
        putc (h, f);
        putc (h >> 8, f);
        putc (h >> 16, f);
        putc (h >> 24, f);
}

static
int fgethdr (text, h)
        register FILE *text;
        register struct exec *h;
{
        h->a_midmag   = fgetword (text);
        h->a_text    = fgetword (text);
        h->a_data    = fgetword (text);
        h->a_bss     = fgetword (text);
        h->a_reltext = fgetword (text);
        h->a_reldata = fgetword (text);
        h->a_syms    = fgetword (text);
        h->a_entry   = fgetword (text);
        return (1);
}

static
void fputhdr (hdr, coutb)
        register struct exec *hdr;
        register FILE *coutb;
{
        fputword (hdr->a_magic, coutb);
        fputword (hdr->a_text, coutb);
        fputword (hdr->a_data, coutb);
        fputword (hdr->a_bss, coutb);
        fputword (hdr->a_reltext, coutb);
        fputword (hdr->a_reldata, coutb);
        fputword (hdr->a_syms, coutb);
        fputword (hdr->a_entry, coutb);
}

/*
 * Read a relocation record: 1 to 6 bytes.
 */
static
void fgetrel (f, r)
        register FILE *f;
        register struct reloc *r;
{
        r->flags = getc (f);
        if ((r->flags & RSMASK) == REXT) {
                r->index = getc (f);
                r->index |= getc (f) << 8;
                r->index |= getc (f) << 16;
        }
        if ((r->flags & RFMASK) == RHIGH16 ||
            (r->flags & RFMASK) == RHIGH16S) {
                r->offset = getc (f);
                r->offset |= getc (f) << 8;
        }
}

/*
 * Emit a relocation record: 1 to 6 bytes.
 * Return a written length.
 */
static
unsigned fputrel (r, f)
        register struct reloc *r;
        register FILE *f;
{
        register unsigned nbytes = 1;

        putc (r->flags, f);
        if ((r->flags & RSMASK) == REXT) {
                putc (r->index, f);
                putc (r->index >> 8, f);
                putc (r->index >> 16, f);
                nbytes += 3;
        }
        if ((r->flags & RFMASK) == RHIGH16 ||
            (r->flags & RFMASK) == RHIGH16S) {
                putc (r->offset, f);
                putc (r->offset >> 8, f);
                nbytes += 2;
        }
        return nbytes;
}

static
void delexit (int sig)
{
	unlink ("l.out");
	/* chmod (ofilename, 0777 & ~umask(0)); */
	if(delarg)
	  exit (delarg);
}

static
void error (int n, const char *s, ...)
{
        va_list ap;

        va_start (ap, s);
	if (! errlev)
                printf ("ld: ");
	if (filname)
                printf ("%s: ", filname);
	xvprintf(s,ap);
	va_end (ap);
	printf ("\n");
	if (n > 1)
                delexit (0);
	errlev = n;
}

static
int fgetsym (text, sym)
        register FILE *text;
        register struct nlist *sym;
{
	register int c;

	c = getc (text);
	if (c <= 0)
		return 0;
	sym->n_len = c;
	
	sym->n_name = umm_malloc (sym->n_len + 1);
	if (! sym->n_name)
		error (2, "out of memory");
	sym->n_type = getc (text);
	sym->n_value = fgetword (text);
	for (c=0; c<sym->n_len; c++)
		sym->n_name [c] = getc (text);
	sym->n_name [sym->n_len] = '\0';
	return sym->n_len + 6;
}

static
void fputsym (s, file)
        register struct nlist *s;
        register FILE *file;
{
	register int i;

	putc (s->n_len, file);
	putc (s->n_type, file);
	fputword (s->n_value, file);
	for (i=0; i<s->n_len; i++)
		putc (s->n_name[i], file);
}

/*
 * Read the file header of the archive.
 */
static
int fgetarhdr (fd, h)
        register FILE *fd;
        register struct archdr *h;
{
	struct ar_hdr hdr;
	register int len, nr;
	register char *p;
	char buf[20];

	/* Read arhive name.  Spaces should never happen. */
	nr = fread (buf, 1, sizeof (hdr.ar_name), fd);
	if (buf[0] == ' ')
		return 0;
        buf[sizeof(hdr.ar_name)] = 0;

	/* Long name support.  Set the "real" size of the file,
	 * and the long name flag/size. */
	h->ar_size = 0;
	if (strncmp (buf, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0) {
		len = atoi (buf + sizeof(AR_EFMT1) - 1);
		if (len <= 0 || len > MAXNAMLEN)
			return 0;
                h->ar_name = umm_malloc (len + 1);
                if (! h->ar_name)
                        return 0;
	        fread (h->ar_name, 1, len, fd);
		h->ar_name[len] = 0;
		h->ar_size -= len;
	} else {
		/* Strip trailing spaces, null terminate. */
		p = buf + nr - 1;
		while (*p == ' ')
                        --p;
		*++p = '\0';

	        len = p - buf;
                h->ar_name = umm_malloc (len + 1);
                if (! h->ar_name)
                        return 0;
                strcpy (h->ar_name, buf);
	}

	/* Read arhive date. */
	fread (buf, 1, sizeof (hdr.ar_date), fd);
	buf[sizeof(hdr.ar_date)] = 0;
        h->ar_date = strtol (buf, 0, 10);

	/* Read user id. */
	fread (buf, 1, sizeof (hdr.ar_uid), fd);
	buf[sizeof(hdr.ar_uid)] = 0;
        h->ar_uid = strtol (buf, 0, 10);

	/* Read group id. */
	fread (buf, 1, sizeof (hdr.ar_gid), fd);
        buf[sizeof (hdr.ar_gid)] = 0;
        h->ar_gid = strtol (buf, 0, 10);

	/* Read mode (octal). */
	nr = fread (buf, 1, sizeof (hdr.ar_mode), fd);
        buf[sizeof(hdr.ar_mode)] = 0;
        h->ar_mode = strtol (buf, 0, 8);

	/* Read archive size. */
	nr = fread (buf, 1, sizeof (hdr.ar_size), fd);
        buf[sizeof (hdr.ar_size)] = 0;
        h->ar_size = strtol (buf, 0, 10);

	/* Check secondary magic. */
	nr = fread (buf, 1, sizeof (hdr.ar_fmag), fd);
	buf[sizeof (hdr.ar_fmag)] = 0;
	if (strcmp (buf, ARFMAG) != 0){
	  umm_free(hdr.ar_name);
	  return 0;
	}

	return 1;
}

static
void freerantab ()
{
	register struct ranlib *p;

	for (p=rantab; p<rantab+rancount; ++p)
		umm_free (p->ran_name);
}

static
int fgetran (text, sym)
        register FILE *text;
        register struct ranlib *sym;
{
	register int c;

	/* read struct ranlib from file */
	/* 1 byte - length of name */
	/* 4 bytes - seek in archive */
	/* 'len' bytes - symbol name */
	/* if len == 0 then eof */
	/* return 1 if ok, 0 on eof */

	sym->ran_len = getc (text);
	if (sym->ran_len <= 0)
		return (0);
	sym->ran_name = umm_malloc (sym->ran_len + 1);
	if (! sym->ran_name)
		error (2, "out of memory");
	sym->ran_off = fgetword (text);
	for (c=0; c<sym->ran_len; c++)
		sym->ran_name [c] = getc (text);
	sym->ran_name [sym->ran_len] = '\0';
	return (1);
}

static
void getrantab ()
{
	register struct ranlib *p;

	for (p=rantab; p<rantab+RANTABSZ; ++p) {
		if (! fgetran (text, p)) {
			rancount = p - rantab;
			return;
		}
	}
	error (2, "ranlib buffer overflow");
}

static
void ldrsym (sp, val, type)
        register struct nlist *sp;
        unsigned val;
{
	if (sp == 0)
                return;
	if (sp->n_type != N_EXT+N_UNDF) {
		printf ("%s: ", sp->n_name);
		error (1, "name redefined");
		return;
	}
	sp->n_type = type;
	sp->n_value = val;
}

static
void tcreat (buf, tempflg)
        register FILE **buf;
        register int tempflg;
{
  tfname[4] = filenum+++'a';
	*buf = fopen (tempflg ? tfname : ofilename, "w+");
	if (! *buf)
		error (2, tempflg ?
			"cannot create temporary file" :
			"cannot create output file");
}

static
int reltype (stype)
        int stype;
{
	switch (stype & N_TYPE) {
	case N_UNDF:    return (0);
	case N_ABS:     return (RABS);
	case N_TEXT:    return (RTEXT);
	case N_DATA:    return (RDATA);
	case N_BSS:     return (RBSS);
	case N_STRNG:   return (RDATA);
	case N_COMM:    return (RBSS);
	case N_FN:      return (0);
	default:        return (0);
	}
}

struct nlist *lookloc (lp, sn)
        register struct _local *lp;
        register int sn;
{
	register struct _local *clp;

	for (clp=local; clp<lp; clp++)
		if (clp->locindex == sn)
			return (clp->locsymbol);
	if (trace) {
		printf( "*** %d ***\n", sn);
		for (clp=local; clp<lp; clp++)
			printf( "%u, ", clp->locindex);
		printf( "\n");
	}
	error (2, "bad symbol reference");
	return 0;
}

static
void printrel (word, rel)
        register unsigned word;
        register struct reloc *rel;
{
	printf ("%08x %02x ", word, rel->flags);

        if ((rel->flags & RSMASK) == REXT)
                printf ("%-3d ", rel->index);
        else
                printf ("    ");

        if ((rel->flags & RFMASK) == RHIGH16 ||
            (rel->flags & RFMASK) == RHIGH16S)
                printf ("%08x", rel->offset);
        else
                printf ("        ");
}

/*
 * Relocate the word by a given offset.
 * Return the new value of word and update rel.
 */
static
unsigned relword (lp, word, rel, offset)
        struct _local *lp;
        register unsigned word;
        register struct reloc *rel;
        unsigned offset;
{
	register unsigned addr, delta;
	register struct nlist *sp = 0;

	if (trace > 2)
                printrel (word, rel);
	/*
         * Extract an address field from the instruction.
         */
	switch (rel->flags & RFMASK) {
	case RBYTE16:
		addr = word & 0xffff;
		break;
	case RBYTE32:
		addr = word;
		break;
	case RWORD16:
		addr = (word & 0xffff) << 2;
		break;
	case RWORD26:
		addr = (word & 0x3ffffff) << 2;
		break;
	case RHIGH16:
		addr = (word & 0xffff) << 16;
	        addr += rel->offset;
		break;
	case RHIGH16S:
		addr = (word & 0xffff) << 16;
	        addr += (signed short) rel->offset;
		break;
	default:
		addr = 0;
		break;
	}

	/*
         * Compute a delta for address.
         * Update the relocation info, if needed.
         */
	switch (rel->flags & RSMASK) {
	case RTEXT:
		delta = ctrel;
		break;
	case RDATA:
		delta = cdrel;
		break;
	case RBSS:
		delta = cbrel;
		break;
	case REXT:
		sp = lookloc (lp, rel->index);
		if (sp->n_type == N_EXT+N_UNDF ||
		    sp->n_type == N_EXT+N_COMM) {
                        rel->index = nsym + (sp - symtab);
			sp = 0;
                        delta = 0;
                } else {
                        rel->flags &= RFMASK | RGPREL;
                        rel->flags |= reltype (sp->n_type);
                        delta = sp->n_value;
		}
		break;
	default:
                delta = 0;
		break;
	}

	if ((rel->flags & RGPREL) && ! output_relinfo) {
            /*
             * GP relative address.
             */
            delta -= gp;
            rel->flags &= ~RGPREL;
	}

	/*
         * Update the address field of the instruction.
         * Update the relocation info, if needed.
         */
	switch (rel->flags & RFMASK) {
	case RBYTE16:
	        addr += delta;
		word &= ~0xffff;
		word |= addr & 0xffff;
		break;
	case RBYTE32:
		word = addr + delta;
		break;
	case RWORD16:
	        if (! sp)
                    break;
	        addr += delta - offset - 4;
                word &= ~0xffff;
                word |= (addr >> 2) & 0xffff;
                rel->flags = RABS;
		break;
	case RWORD26:
	        addr += delta;
		word &= ~0x3ffffff;
		word |= (addr >> 2) & 0x3ffffff;
		break;
	case RHIGH16:
	        addr += delta;
		word &= ~0xffff;
		word |= (addr >> 16) & 0xffff;
		break;
	case RHIGH16S:
	        addr += delta;
		word &= ~0xffff;
		word |= ((addr + 0x8000) >> 16) & 0xffff;
		break;
	}
	if (trace > 2) {
		//printf (" +%#x ", delta);
		printf (" -> ");
                printrel (word, rel);
		printf ("\n");
        }
	return word;
}

static
void relocate (lp, b1, b2, len, origin)
        struct _local *lp;
        FILE *b1, *b2;
        unsigned len, origin;
{
	unsigned word, offset;
	struct reloc rel;

	for (offset=0; offset<len; offset+=W) {
		word = fgetword (text);
		fgetrel (reloc, &rel);
		word = relword (lp, word, &rel, offset + origin);
		fputword (word, b1);
		if (output_relinfo)
                        fputrel (&rel, b2);
	}
}

static
unsigned copy_and_close (buf)
        register FILE *buf;
{
	register int c;
	unsigned nbytes;

	f_lseek (buf,0);
	nbytes = 0;
	while ((c = getc (buf)) != EOF) {
                putc (c, outb);
                nbytes++;
        }
	fclose (buf);
	return nbytes;
}

static
int mkfsym (s, wflag)
        register char *s;
{
	register char *p;

	if (sflag || xflag)
                return (0);
	for (p=s; *p;)
                if (*p++ == '/')
                        s = p;
	if (! wflag)
                return (p - s + 6);
	cursym.n_len = p - s;
	cursym.n_name = umm_malloc (cursym.n_len + 1);
	if (! cursym.n_name)
		error (2, "out of memory");
	for (p=cursym.n_name; *s; p++, s++)
                *p = *s;
	cursym.n_type = N_FN;
	cursym.n_value = torigin;
	fputsym (&cursym, soutb);
	umm_free (cursym.n_name);
	return (cursym.n_len + 6);
}

static
int getfile (cp)
        register char *cp;
{
	int c;
        static char libname [] = "/lib/libxxxxxxx";
        char magic [SARMAG];

	text = 0;
	filname = cp;
	if (cp[0] == '-' && cp[1] == 'l') {
		if (cp[2] == '\0')
                        cp = "-la";
                filname = libname;
		for (c = 0; cp [c+2]; c++)
                        filname [c + 8] = cp [c+2];
		filname [c + 8] = '.';
		filname [c + 8 + 1] = 'a';
		filname [c + 8 + 2] = '\0';
	}
	text = fopen (filname, "r");
	if (! text)
		error (2, "cannot open");
	reloc = fopen (filname, "r");
	if (! reloc)
		error (2, "cannot open");

        /* Read file magic. */
	if (fread(magic, 1, SARMAG, text) != SARMAG)
		return (0);     /* regular file */
        if (strncmp (magic, ARMAG, SARMAG) != 0)
		return (0);     /* regular file */
        if (! fgetarhdr (text, &archdr))
		return (0);     /* regular file */
	if (strncmp (archdr.ar_name, SYMDEF, sizeof (SYMDEF)) != 0) {
	        umm_free (archdr.ar_name);
		return (1);     /* regular archive */
        }
        umm_free (archdr.ar_name);
	return (2);             /* randomized archive */
}

static
int enter (hp)
        register struct nlist **hp;
{
	register struct nlist *sp;

	if (*hp) {
		lastsym = *hp;
		return (0);
	}
        if (symindex >= NSYM)
                error (2, "symbol table overflow");

        symhash [symindex] = hp;
        *hp = lastsym = sp = &symtab[symindex++];
        sp->n_len = cursym.n_len;
        sp->n_name = cursym.n_name;
        sp->n_type = cursym.n_type;
        sp->n_value = cursym.n_value;
        return (1);
}

static
void symreloc()
{
	switch (cursym.n_type) {
	case N_TEXT:
	case N_EXT+N_TEXT:
		cursym.n_value += ctrel;
		return;
	case N_DATA:
	case N_EXT+N_DATA:
		cursym.n_value += cdrel;
		return;
	case N_BSS:
	case N_EXT+N_BSS:
		cursym.n_value += cbrel;
		return;
	case N_EXT+N_UNDF:
	case N_EXT+N_COMM:
		return;
	}
	if (cursym.n_type & N_EXT)
                cursym.n_type = N_EXT+N_ABS;
}

/*
 * Suboptimal 32-bit hash function.
 * Copyright (C) 2006 Serge Vakulenko.
 */
static
unsigned hash_rot13 (s)
    register const char *s;
{
    register unsigned hash, c;

    hash = 0;
    while ((c = (unsigned char) *s++) != 0) {
        hash += c;
        hash -= (hash << 13) | (hash >> 19);
    }
    return hash;
}

static
struct nlist **lookup()
{
	int clash;
	register char *cp, *cp1;
	register struct nlist **hp;

        hp = &hshtab[hash_rot13 (cursym.n_name) % NSYM + 2];
	while (*hp != 0) {
		cp1 = (*hp)->n_name;
		clash = 0;
		for (cp = cursym.n_name; *cp;) {
			if (*cp++ != *cp1++) {
				clash = 1;
				break;
			}
                }
		if (! clash)
			break;
		if (++hp >= &hshtab[NSYM+2])
			hp = hshtab;
	}
	return (hp);
}

static
struct nlist **slookup (s)
        char *s;
{
	cursym.n_len = strlen (s) + 1;
	cursym.n_name = s;
	cursym.n_type = N_EXT+N_UNDF;
	cursym.n_value = 0;
	return (lookup ());
}

static
void readhdr (loc)
        unsigned loc;
{
	f_lseek (text, loc);
	if (! fgethdr (text, &filhdr))
		error (2, "bad format");
	if (N_GETMAGIC(filhdr) != RMAGIC)
		error (2, "bad magic");
	if (filhdr.a_text % W)
		error (2, "bad length of text");
	if (filhdr.a_data % W)
		error (2, "bad length of data");
        /* BSS segment is allowed to be unaligned. */
}

/*
 * single file
 */
static
int load1 (loc, libflg, nloc)
        unsigned loc;
{
	register struct nlist *sp;
	int savindex, ndef, type, symlen, nsymbol;

	readhdr (loc);
	if (N_GETMAGIC(filhdr) != RMAGIC) {
		error (1, "file not relocatable");
		return (0);
	}
	f_lseek (reloc, loc + N_SYMOFF (filhdr));

	ctrel = tsize;
	cdrel = dsize - filhdr.a_text;
	cbrel = bsize - (filhdr.a_text + filhdr.a_data);

	loc += HDRSZ + filhdr.a_text + filhdr.a_data  +
                filhdr.a_reltext + filhdr.a_reldata;
	f_lseek (text, loc);
	ndef = 0;
	savindex = symindex;
	if (nloc)
                nsymbol = 1;
        else
                nsymbol = 0;
	for (;;) {
		symlen = fgetsym (text, &cursym);
		if (symlen == 0)
			break;
		type = cursym.n_type;
		if (Sflag && ((type & N_TYPE) == N_ABS ||
			(type & N_TYPE) > N_COMM))
		{
			umm_free (cursym.n_name);
			continue;
		}
		if (! (type & N_EXT)) {
			if (! (sflag || xflag ||
                            (Xflag && IS_LOCSYM(&cursym))))
                        {
				nsymbol++;
				nloc += symlen;
			}
			umm_free (cursym.n_name);
			continue;
		}
		symreloc ();
		if (enter (lookup ()))
                        continue;
		umm_free (cursym.n_name);
		if (cursym.n_type == N_EXT+N_UNDF)
                        continue;
		sp = lastsym;
		if (sp->n_type == N_EXT+N_UNDF ||
			sp->n_type == N_EXT+N_COMM)
		{
			if (cursym.n_type == N_EXT+N_COMM) {
				sp->n_type = cursym.n_type;
				if (cursym.n_value > sp->n_value)
					sp->n_value = cursym.n_value;
			}
			else if (sp->n_type==N_EXT+N_UNDF ||
				cursym.n_type==N_EXT+N_DATA ||
				cursym.n_type==N_EXT+N_BSS)
			{
				ndef++;
				sp->n_type = cursym.n_type;
				sp->n_value = cursym.n_value;
			}
		}
	}
	if (! libflg || ndef) {
		tsize += filhdr.a_text;
		dsize += filhdr.a_data;
		bsize += filhdr.a_bss;
		ssize += nloc;
		nsym += nsymbol;

		/* Alignment. */
                tsize = (tsize + 3) & ~3;
                dsize = (dsize + 3) & ~3;
                bsize = (bsize + 3) & ~3;
		return (1);
	}

	/*
	 * No symbols defined by this library member.
	 * Rip out the hash table entries and reset the symbol table.
	 */
	while (symindex > savindex) {
		register struct nlist **p;

		p = symhash[--symindex];
		umm_free ((*p)->n_name);
		*p = 0;
	}
	return (0);
}

static
void addlibp (nloc)
        register unsigned nloc;
{
        *libp++ = nloc;
	if (libp >= &liblist[NLIBS])
		error (2, "library table overflow");
}

static
int step (nloc)
        register unsigned nloc;
{
	f_lseek (text, nloc);
	if (! fgetarhdr (text, &archdr)) {
		return (0);
	}
	if (load1 (nloc + ARHDRSZ, 1, mkfsym (archdr.ar_name, 0))) {
		addlibp (nloc);
                if (trace)
                        printf ("load '%s' offset %08x\n", archdr.ar_name, nloc);
        }
        umm_free (archdr.ar_name);
	return (1);
}

static
int ldrand ()
{
	register struct ranlib *p;
	struct nlist **pp;
	unsigned *oldp = libp;

	for (p=rantab; p<rantab+rancount; ++p) {
		pp = slookup (p->ran_name);
		if (! *pp)
			continue;
		if ((*pp)->n_type == N_EXT+N_UNDF)
			step (p->ran_off);
	}
	return (oldp != libp);
}

/*
 * scan a library to find defined symbols
 */
static
void load1lib (off0)
        unsigned off0;
{
        register unsigned offset;
        register unsigned *oldp;

        /* repeat while any symbols found */
        do {
                oldp = libp;
                offset = off0;
                while (step (offset))
                        offset += archdr.ar_size + ARHDRSZ;
        } while (libp != oldp);
        addlibp (-1);
}

/*
 * scan file to find defined symbols
 */
static
void load1arg (cp)
        register char *cp;
{
	switch (getfile (cp)) {
	case 0:                 /* regular file */
		load1 (0L, 0, mkfsym (cp, 0));
		break;
	case 1:                 /* regular archive */
                load1lib (SARMAG);
		break;
	case 2:                 /* archive with table of contents */
		getrantab ();
		while (ldrand ())
                        continue;
	        freerantab ();
		addlibp (-1);
		break;
	case 3:                 /* out of date table of contents */
		error (0, "out of date (warning)");
                load1lib (SARMAG + archdr.ar_size + ARHDRSZ);
		break;
	}
	fclose (text);
	fclose (reloc);
}

static
void pass1 (argc, argv)
        char **argv;
{
	register int c, i;
	register char *ap, **p;
	char save;

	/* scan files once to find symdefs */

	p = argv + 1;
	libp = liblist;
	for (c=1; c<argc; ++c) {
		filname = 0;
		ap = *p++;

		if (*ap != '-') {
			load1arg (ap);
			continue;
		}
		for (i=1; ap[i]; i++) {
			switch (ap [i]) {

				/* output file name */
			case 'o':
				if (++c >= argc)
					error (2, "-o: argument missing");
				ofilename = *p++;
				ofilfnd++;
				continue;

				/* 'use' */
			case 'u':
				if (++c >= argc)
					error (2, "-u: argument missing");
				enter (slookup (*p++));
				continue;

				/* 'entry' */
			case 'e':
				if (++c >= argc)
					error (2, "-e: argument missing");
				enter (slookup (*p++));
				entrypt = lastsym;
				continue;

				/* base address of loading */
			case 'T':
			  printf("not surported\n");
			        /* basaddr = atol (ap+i+1); */
				break;

				/* library */
			case 'l':
				save = ap [--i];
				ap [i] = '-';
				load1arg (&ap[i]);
				ap [i] = save;
				break;

				/* discard local symbols */
			case 'x':
				xflag++;
				continue;

				/* discard locals starting with 'L' or '.' */
			case 'X':
				Xflag++;
				continue;

				/* discard all except locals and globals*/
			case 'S':
				Sflag++;
				continue;

				/* preserve rel. bits, don't define common */
			case 'r':
				rflag++;
				output_relinfo++;
				continue;

				/* discard all symbols */
			case 's':
				sflag++;
				xflag++;
				continue;

				/* define common even with rflag */
			case 'd':
				dflag++;
				continue;

				/* tracing */
			case 't':
				trace++;
				continue;

				/* verbose */
			case 'v':
				verbose++;
				continue;

			default:
				error (2, "unknown flag");
			}
			break;
		}
	}
}

static
void middle()
{
	register struct nlist *sp, *symp;
	register unsigned t, cmsize;
	int nund;
	unsigned cmorigin;

	p_etext = *slookup ("_etext");
	p_edata = *slookup ("_edata");
	p_end = *slookup ("_end");
        p_gp = *slookup ("_gp");

	/*
	 * If there are any undefined symbols, save the relocation bits.
	 */
	symp = &symtab[symindex];
	if (! output_relinfo) {
		for (sp=symtab; sp<symp; sp++)
			if (sp->n_type == N_EXT+N_UNDF &&
				sp != p_end && sp != p_edata &&
                                sp != p_etext && sp != p_gp)
			{
				output_relinfo++;
				dflag = 0;
				break;
			}
	}
	if (output_relinfo)
                sflag = 0;

	/*
	 * Assign common locations.
	 * Align text size to 16 bytes.
	 */
	cmsize = 0;
	tsize  = (tsize + 15) & ~15;
	if (dflag || ! output_relinfo) {
		ldrsym (p_etext, tsize, N_EXT+N_TEXT);
		ldrsym (p_edata, dsize, N_EXT+N_DATA);
		ldrsym (p_end, bsize, N_EXT+N_BSS);

                /* Set GP as offset from the start of data segment. */
		ldrsym (p_gp, gpoffset, N_EXT+N_DATA);

		for (sp=symtab; sp<symp; sp++) {
			if ((sp->n_type & N_TYPE) == N_COMM) {
				t = sp->n_value;
				sp->n_value = cmsize;
				cmsize += t;
                                cmsize = (cmsize + 3) & ~3;
			}
                }
	}

	/*
	 * Now set symbols to their final value.
	 */
	torigin = basaddr;
	dorigin = torigin + tsize;
	gp = dorigin + gpoffset;
	cmorigin = dorigin + dsize;
	borigin = cmorigin + cmsize;
	nund = 0;
	for (sp=symtab; sp<symp; sp++) {
		switch (sp->n_type) {
		case N_EXT+N_UNDF:
			if (! rflag) {
                                errlev |= 1;
				if (sp == p_end || sp == p_edata ||
                                    sp == p_etext || sp == p_gp)
                                        break;
				if (! nund)
					printf ("Undefined:\n");
				nund++;
				printf ("\t%s\n", sp->n_name);
			}
			break;
		default:
		case N_EXT+N_ABS:
			break;
		case N_EXT+N_TEXT:
			sp->n_value += torigin;
			break;
		case N_EXT+N_DATA:
			sp->n_value += dorigin;
			break;
		case N_EXT+N_BSS:
			sp->n_value += borigin;
			break;
		case N_COMM:
		case N_EXT+N_COMM:
			sp->n_type = N_EXT+N_BSS;
			sp->n_value += cmorigin;
			break;
		}
	}
	if (sflag || xflag)
                ssize = 0;
	bsize += cmsize;

	/*
	 * Compute ssize; add length of local symbols, if need,
	 * and one more zero byte. Alignment will be taken at setupout.
	 */
	if (sflag)
                ssize = 0;
	else {
		if (xflag)
                        ssize = 0;
		for (sp = symtab; sp < &symtab[symindex]; sp++)
			ssize += sp->n_len + 6;
		ssize++;
	}
}

static
void setupout ()
{
	tcreat (&outb, 0);

	tcreat (&toutb, 1);
	tcreat (&doutb, 1);

	if (! sflag || ! xflag)
                tcreat (&soutb, 1);
	if (output_relinfo) {
		tcreat (&troutb, 1);
		tcreat (&droutb, 1);
	}
	f_lseek (outb, sizeof(filhdr));
}

static
void load2 (loc)
        unsigned loc;
{
	register struct nlist *sp;
	register struct _local *lp;
	register int symno;
	int type;
	unsigned count;

	readhdr (loc);
	ctrel = torigin;
	cdrel = dorigin - filhdr.a_text;
	cbrel = borigin - (filhdr.a_text + filhdr.a_data);

	if (trace > 1)
		printf ("ctrel=%08x, cdrel=%08x, cbrel=%08x\n",
			ctrel, cdrel, cbrel);
	/*
	 * Reread the symbol table, recording the numbering
	 * of symbols for fixing external references.
	 */
	lp = local;
	symno = -1;
	loc += HDRSZ;
	f_lseek (text, loc + filhdr.a_text + filhdr.a_data +
                filhdr.a_reltext + filhdr.a_reldata);
	for (;;) {
		symno++;
		count = fgetsym (text, &cursym);
		if (count == 0)
			break;
		symreloc ();
		type = cursym.n_type;
		if (Sflag && ((type & N_TYPE) == N_ABS ||
			(type & N_TYPE) > N_COMM))
		{
			umm_free (cursym.n_name);
			continue;
		}
		if (! (type & N_EXT)) {
			if (! (sflag || xflag ||
                            (Xflag && IS_LOCSYM(&cursym))))
				fputsym (&cursym, soutb);
			umm_free (cursym.n_name);
			continue;
		}
		if (! (sp = *lookup()))
			error (2, "internal error: symbol not found");
		umm_free (cursym.n_name);
		if (cursym.n_type == N_EXT+N_UNDF ||
			cursym.n_type == N_EXT+N_COMM)
		{
			if (lp >= &local [NSYMPR])
				error (2, "local symbol table overflow");
			lp->locindex = symno;
			lp++->locsymbol = sp;
			continue;
		}
		if (cursym.n_type != sp->n_type ||
			cursym.n_value != sp->n_value)
		{
			printf ("%s: ", cursym.n_name);
			error (1, "name redefined");
		}
	}

	count = loc + filhdr.a_text + filhdr.a_data;

	if (trace > 1)
		printf ("-- text --\n");
	f_lseek (text, loc);
	f_lseek (reloc, count);
	relocate (lp, toutb, troutb, filhdr.a_text, torigin);

	if (trace > 1)
		printf ("-- data --\n");
	f_lseek (text, loc + filhdr.a_text);
	f_lseek (reloc, count + filhdr.a_reltext);
	relocate (lp, doutb, droutb, filhdr.a_data, dorigin);

	torigin += filhdr.a_text;
	dorigin += filhdr.a_data;
	borigin += filhdr.a_bss;

        /* Alignment. */
        torigin = (torigin + 3) & ~3;
        dorigin = (dorigin + 3) & ~3;
        borigin = (borigin + 3) & ~3;
}

static
void load2arg (arname)
        register char *arname;
{
	register unsigned *lp;

	if (getfile (arname) == 0) {
		if (trace || verbose)
			printf ("%s:\n", arname);
		mkfsym (arname, 1);
		load2 (0L);
	} else {
		/* scan archive members referenced */
		for (lp = libp; *lp != -1; lp++) {
			f_lseek (text, *lp);
			fgetarhdr (text, &archdr);
			if (trace || verbose)
				printf ("%s(%s):\n", arname, archdr.ar_name);
			mkfsym (archdr.ar_name, 1);
                        umm_free (archdr.ar_name);
			load2 (*lp + ARHDRSZ);
		}
		libp = ++lp;
	}
	fclose (text);
	fclose (reloc);
}

static
void pass2 (argc, argv)
        int argc;
        char **argv;
{
	register int c, i;
	register char *ap, **p;

	p = argv + 1;
	libp = liblist;
	for (c=1; c<argc; c++) {
		ap = *p++;
		if (*ap != '-') {
			load2arg (ap);
			continue;
		}
		for (i=1; ap[i]; i++) {
			switch (ap[i]) {

			case 'u':
			case 'e':
			case 'o':
				++c;
				++p;

			default:
				continue;

			case 'l':
				ap [--i] = '-';
				load2arg (&ap[i]);
				break;

			}
			break;
		}
	}
}

void finishout ()
{
	register struct nlist *p;
        unsigned rtsize = 0, rdsize = 0;
        register unsigned n;

	n = copy_and_close (toutb);
        while (n++ < tsize) {
                /* Align text size. */
                putc (0, outb);
        }
	copy_and_close (doutb);
	if (output_relinfo) {
		rtsize = copy_and_close (troutb);
		while (rtsize % W) {
			putc (0, outb);
			rtsize++;
                }
		rdsize = copy_and_close (droutb);
		while (rdsize % W) {
			putc (0, outb);
			rdsize++;
                }
	}
	if (! sflag) {
		if (! xflag)
                        copy_and_close (soutb);
		for (p=symtab; p<&symtab[symindex]; ++p)
			fputsym (p, outb);
		putc (0, outb);
		while (ssize++ % W)
			putc (0, outb);
	}
	filhdr.a_midmag = output_relinfo ? RMAGIC : OMAGIC;
	filhdr.a_text = tsize;
	filhdr.a_data = dsize;
	filhdr.a_bss = bsize;
	filhdr.a_reltext = rtsize;
	filhdr.a_reldata = rdsize;
	filhdr.a_syms = ALIGN (ssize, W);
	/* printf("%08x\n",entrypt); */
	/* printf("%08x\n",basaddr); */
	if (entrypt) {
		if (entrypt->n_type != N_EXT+N_TEXT &&
		    entrypt->n_type != N_EXT+N_UNDF)
			error (1, "entry out of text");
		else filhdr.a_entry = entrypt->n_value;
	} else
		filhdr.a_entry = basaddr;

	f_lseek (outb, 0);
	fputhdr (&filhdr, outb);
	fclose (outb);
}

extern char *test_umm_heap;
int ldmain (argc, argv,baseaddress)
     char **argv;
{
  basaddr = baseaddress;
  RAM = fframset(ARAM,10);

  if(AVAILABLE_MEM >= sizeof(*g_ld)){
    g_ld = RAM;
    //    test_umm_heap = ;
    if(AVAILABLE_MEM-sizeof(*g_ld) < 4000){
      printf("ram not enough,(%d)",AVAILABLE_MEM-sizeof(*g_ld));
      return -1;
    }
    int i;
    for(i=0;i<AVAILABLE_MEM/sizeof(RAM[0]);i++){
      RAM[i] = 0;
    }
    umm_init((void*)((((unsigned)RAM) + sizeof(*g_ld)+1)&0xFFFFFFFC),(AVAILABLE_MEM-sizeof(*g_ld)-8)&0xFFFFFFC);
  }
    
  else{
    printf("ERROR");
    return -1;
  }
  xsprintf(tfname, "/tmp?");
  ofilename = "l.out";


	if (argc == 1) {
		/* printf ("Usage:\n"); */
		/* printf ("  ld [-sSxXrdt] [-o file] [-lname] [-u name] [-e name] [-T num] file...\n"); */
		/* printf ("Options:\n"); */
                /* printf ("  -o filename     Set output file name, default a.out\n"); */
                /* printf ("  -llibname       Search for library libname\n"); */
                /* printf ("  -u symbol       Start with undefined reference to symbol\n"); */
                /* printf ("  -e symbol       Set start address\n"); */
                /* printf ("  -T address      Set address of .text segment, default %#x\n", basaddr); */
                /* printf ("  -s              Discard all symbols\n"); */
                /* printf ("  -S              Discard all symbols except locals and globals\n"); */
                /* printf ("  -x              Discard local symbols\n"); */
                /* printf ("  -X              Discard locals starting with 'L' or '.'\n"); */
                /* printf ("  -r              Generate relocatable output\n"); */
                /* printf ("  -d              Force common symbols to be defined\n"); */
                /* printf ("  -t              Increase trace verbosity (up to 3)\n"); */
		exit (4);
	}

	/*
	 * First pass: compute lengths of segments, symbol name table
         * and entry address.
	 */
	pass1 (argc, argv);
	filname = 0;

	/*
	 * Compute name table.
	 */
	middle ();

	/*
	 * Create temporary files.
	 */
	setupout ();

	/*
	 * Second pass: relocation.
	 */
	pass2 (argc, argv);

	/*
	 * Flush buffers, write a header.
	 */
	finishout ();

	if (! ofilfnd) {
		unlink ("a.out");
		fclose(outb);
		if (f_rename ("l.out", "a.out") != 0)
		        printf("a.out error");
		ofilename = "a.out";
	}
	delarg = errlev;

	fcloseall();
	for(;tfname[4]!='a'-1;tfname[4]--){
	  if(f_unlink(tfname)){
	    /* printf("cannot remove temporary file.[%s]",tfname); */
	  }
	}
	delexit (0);
	return (0);
}
