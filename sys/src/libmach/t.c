/*
 * thumb definition
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

#pragma pack on
#include "/arm/include/ureg.h"
#pragma pack off

#define	REGOFF(x)	(uintptr)(&((struct Ureg *) 0)->x)

#define SP		REGOFF(r13)
#define PC		REGOFF(pc)

#define	REGSIZE		sizeof(struct Ureg)

Reglist thumbreglist[] =
{
	{"LINK",	REGOFF(link),		RINT|RRDONLY, 'X'},
	{"TYPE",	REGOFF(type),		RINT|RRDONLY, 'X'},
	{"PSR",		REGOFF(psr),		RINT|RRDONLY, 'X'},
	{"PC",		PC,			RINT, 'X'},
	{"SP",		SP,			RINT, 'X'},
	{"R15",		PC,			RINT, 'X'},
	{"R14",		REGOFF(r14),		RINT, 'X'},
	{"R13",		REGOFF(r13),		RINT, 'X'},
	{"R12",		REGOFF(r12),		RINT, 'X'},
	{"R11",		REGOFF(r11),		RINT, 'X'},
	{"R10",		REGOFF(r10),		RINT, 'X'},
	{"R9",		REGOFF(r9),		RINT, 'X'},
	{"R8",		REGOFF(r8),		RINT, 'X'},
	{"R7",		REGOFF(r7),		RINT, 'X'},
	{"R6",		REGOFF(r6),		RINT, 'X'},
	{"R5",		REGOFF(r5),		RINT, 'X'},
	{"R4",		REGOFF(r4),		RINT, 'X'},
	{"R3",		REGOFF(r3),		RINT, 'X'},
	{"R2",		REGOFF(r2),		RINT, 'X'},
	{"R1",		REGOFF(r1),		RINT, 'X'},
	{"R0",		REGOFF(r0),		RINT, 'X'},
	{  0 }
};

	/* the machine description */
Mach mthumb =
{
	"thumb",
	MARM,		/* machine type */
	thumbreglist,	/* register set */
	REGSIZE,	/* register set size */
	0,		/* fp register set size */
	"PC",		/* name of PC */
	"SP",		/* name of SP */
	"R15",		/* name of link register */
	"setR12",	/* static base register name */
	0,		/* static base register value */
	0x1000,		/* page size */
	0x80000000,	/* kernel base */
	0x88000000,	/* kernel text mask */
	0x7FFFFFFF,	/* stack top */
	2,		/* quantization of pc */
	4,		/* szaddr */
	4,		/* szreg */
	4,		/* szfloat */
	8,		/* szdouble */
};

typedef struct pcentry pcentry;

struct pcentry{
	long start;
	long stop;
};

static pcentry *pctab;
static int npctab = 0;

void
thumbpctab(int fd, Fhdr *fp)
{
	long n;
	uchar c[8];
	pcentry *tab;

	n = seek(fd, 0, 2)-(fp->lnpcoff+fp->lnpcsz);
	if(n == 0)
		return;
	pctab = malloc(n);
	if(pctab == nil)
		sysfatal("could not alloc thumbpctab");
	tab = pctab;
	seek(fd, fp->lnpcoff+fp->lnpcsz, 0);
	while(readn(fd, c, sizeof(c)) == sizeof(c)){
		tab->start = fp->txtaddr + (long)((c[0]<<24)|(c[1]<<16)|(c[2]<<8)|c[3]);
		tab->stop = fp->txtaddr + (long)((c[4]<<24)|(c[5]<<16)|(c[6]<<8)|c[7]);
		tab++;
	}
	npctab = n/sizeof(c);
}

int
thumbpclookup(uvlong pc)
{
	uvlong l, u, m;
	pcentry *tab = pctab;

	if(npctab == 0)
		return 0;

	l = 0;
	u = npctab-1;
	while(l < u){
		m = (l+u)/2;
		if(pc < tab[m].start){
			if(m == 0)
				return 0;
			u = m-1;
		}
		else if(pc > tab[m].stop)
			l = m+1;
		else
			l = u = m;
	}
	if(l == u && u < npctab && tab[u].start <= pc && pc <= tab[u].stop)
		return 1;	// thumb
	return 0;	// arm
}
