#define checkmmu(a, b)
#define countpagerefs(a, b)

#include "../port/portfns.h"

extern int led(int, int);
extern void ledexit(int);
extern void delay(int);
extern void uartconsinit(void);

#pragma	varargck argpos	_uartprint 1

extern void archreboot(void);
extern void archconfinit(void);
extern void archreset(void);
extern void barriers(void);
extern void cachedinv(void);
extern void cachedinvse(void*, int);
extern void cachedwb(void);
extern void cachedwbinv(void);
extern void cachedwbinvse(void*, int);
extern void cachedwbse(void*, int);
extern void cacheiinv(void);
extern void cacheuwbinv(void);
extern uintptr cankaddr(uintptr pa);
extern void clockshutdown(void);
extern int clz(ulong);
int	cmpswap(long*, long, long);

#define coherence barriers

extern u32int controlget(void);
extern u32int cpctget(void);
extern u32int cpidget(void);
extern char *cputype2name(char *, int);
extern ulong cprd(int cp, int op1, int crn, int crm, int op2);
extern ulong cprdsc(int op1, int crn, int crm, int op2);
extern void cpuidprint(void);
extern void cpwr(int cp, int op1, int crn, int crm, int op2, ulong val);
extern void cpwrsc(int op1, int crn, int crm, int op2, ulong val);
#define cycles(ip) *(ip) = lcycles()
extern u32int dacget(void);
extern void dacput(u32int);
extern u32int farget(void);
extern u32int fsrget(void);
extern int ispow2(uvlong);
extern void l1cachesoff(void);
extern void l1cacheson(void);
extern void l2cachecfgoff(void);
extern void l2cachecfgon(void);
extern void l2cacheon(void);
extern void l2cacheuinv(void);
extern void l2cacheuinvse(void*, int);
extern void l2cacheuwb(void);
extern void l2cacheuwbinv(void);
extern void l2cacheuwbinvse(void*, int);
extern void l2cacheuwbse(void*, int);
extern void lastresortprint(char *buf, long bp);
extern int log2(ulong);
extern void mmuinvalidate(void);		/* 'mmu' or 'tlb'? */
extern void mmuinvalidateaddr(u32int);		/* 'mmu' or 'tlb'? */
extern u32int pidget(void);
extern void pidput(u32int);
void	procrestore(Proc *);
void	procsave(Proc*);
void	procsetup(Proc*);
void	procfork(Proc*);
extern void _reset(void);
extern void setr13(int, u32int*);
extern int tas(void *);
extern u32int ttbget(void);
extern void ttbput(u32int);

extern void intrclear(int sort, int v);
extern void intrenable(int sort, int v, void (*f)(Ureg*, void*), void *a, char *name);
extern void intrdisable(int sort, int v, void (*f)(Ureg*, void*), void* a, char *name);
extern void vectors(void);
extern void vtable(void);

/*
 * Things called in main.
 */
extern void clockinit(void);
extern void i8250console(void);
extern void links(void);
extern void mmuinit(void);
extern void touser(uintptr);
extern void trapinit(void);

extern int fpiarm(Ureg*);
extern int fpudevprocio(Proc*, void*, long, uintptr, int);
extern void fpuinit(void);
extern void fpunoted(void);
extern void fpunotify(Ureg*);
extern void fpuprocrestore(Proc*);
extern void fpuprocsave(Proc*);
extern void fpusysprocsetup(Proc*);
extern int fpuemu(Ureg*);

/*
 * Miscellaneous machine dependent stuff.
 */
char*	getconf(char*);
uintptr mmukmap(uintptr, uintptr, usize);
uintptr mmukunmap(uintptr, uintptr, usize);
extern void* mmuuncache(void*, usize);
extern void* ucalloc(usize);
extern void* ucallocalign(usize size, int align, usize span);
extern void ucfree(void*);

/*
 * Things called from port.
 */
extern void delay(int);
extern int islo(void);
extern void microdelay(int);			/* only edf.c */
extern void evenaddr(uintptr);
extern void idlehands(void);
extern void setkernur(Ureg*, Proc*);		/* only devproc.c */
extern void spldone(void);
extern int splfhi(void);
extern int splflo(void);
extern void sysprocsetup(Proc*);
extern int isaconfig(char*, int, ISAConf*);	/* only devusb.c */

int	cas32(void*, u32int, u32int);
int	tas32(void*);

#define CASU(p, e, n)	cas32((p), (u32int)(e), (u32int)(n))
#define CASV(p, e, n)	cas32((p), (u32int)(e), (u32int)(n))
#define CASW(addr, exp, new)	cas32((addr), (exp), (new))
#define TAS(addr)	tas32(addr)

extern void forkret(void);
extern int userureg(Ureg*);
void*	vmap(uintptr, usize);
void	vunmap(void*, usize);

#define	getpgcolor(a)	0
#define	kmapinval()

/*
 * this low-level printing stuff is ugly,
 * but there appears to be no other way to
 * print until after #t is populated.
 */
#define wave(c) { \
	coherence(); \
	while ((*(ulong *)(PHYSCONS+4*5) & (1<<5)) == 0) /* (x->lsr&LSRthre)==0? */ \
		; \
	*(ulong *)PHYSCONS = (c); \
	coherence(); \
}

/*
 * These are not good enough.
 */
#define KADDR(pa)	((void*)(KZERO|((uintptr)(pa))))
#define PADDR(va)	(((uintptr)(va)) & ~KSEGM)

#define MASK(v)	((1UL << (v)) - 1)	/* mask `v' bits wide */
