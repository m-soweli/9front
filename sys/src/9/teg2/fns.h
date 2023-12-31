#define checkmmu(a, b)
#define countpagerefs(a, b)

#include "../port/portfns.h"

typedef struct Ether Ether;
struct Ether;

extern int led(int, int);
extern void ledexit(int);
extern void delay(int);
extern void uartconsinit(void);

#pragma	varargck argpos	_uartprint 1

extern void allcacheinfo(Memcache *);
extern void allcacheson(void);
extern int archether(unsigned, Ether *);
extern void archreboot(void);
extern void archreset(void);
extern void cachedinv(void);
extern void cachedinvse(void*, int);
extern void cachedwb(void);
extern void cachedwbinv(void);
extern void cachedwbinvse(void*, int);
extern void cachedwbse(void*, int);
extern void cacheiinv(void);
extern void cacheuwbinv(void);
extern uintptr cankaddr(uintptr pa);
extern void chkmissing(void);
extern void clockprod(Ureg *);
extern void clockshutdown(void);
extern int clz(ulong);
extern int cmpswap(long*, long, long);
extern void coherence(void);
extern void configscreengpio(void);
extern u32int controlget(void);
extern void cortexa9cachecfg(void);
extern u32int cpctget(void);
extern u32int cpidget(void);
extern ulong cprd(int cp, int op1, int crn, int crm, int op2);
extern ulong cprdsc(int op1, int crn, int crm, int op2);
extern void cpuidprint(void);
extern char *cputype2name(char *buf, int size);
extern void cpwr(int cp, int op1, int crn, int crm, int op2, ulong val);
extern void cpwrsc(int op1, int crn, int crm, int op2, ulong val);
#define cycles(vlp) *(vlp) = (ulong)lcycles()
extern u32int dacget(void);
extern void dacput(u32int);
extern void dmainit(void);
extern int dmastart(void *, int, void *, int, uint, Rendez *, int *);
extern void dmatest(void);
extern void dump(void *vaddr, int words);
extern u32int farget(void);
extern void fpclear(void);
extern void fpoff(void);
extern void fpon(void);
extern ulong fprd(int fpreg);
extern void fprestreg(int fpreg, uvlong val);
extern void fpsave(FPsave *);
extern ulong fpsavereg(int fpreg, uvlong *fpp);
extern void fpwr(int fpreg, ulong val);
extern u32int fsrget(void);
extern ulong getauxctl(void);
extern ulong getclvlid(void);
extern ulong getcyc(void);
extern int getncpus(void);
extern u32int getpsr(void);
extern u32int getscr(void);
extern ulong getwayssets(void);
extern void intcmask(uint);
extern void intcunmask(uint);
extern void intrcpu(int);
extern void intrcpushutdown(void);
extern void intrshutdown(void);
extern void intrsoff(void);
extern int isaconfig(char*, int, ISAConf*);
extern int isdmadone(int);
extern int ispow2(uvlong);
extern void l1diag(void);
extern void l2pl310init(void);
extern int log2(ulong);
extern void machoff(uint cpu);
extern void machon(uint cpu);
extern void memdiag(ulong *);
extern void mmuidmap(uintptr phys, int mbs);
extern void mmuinvalidate(void);		/* 'mmu' or 'tlb'? */
extern void mmuinvalidateaddr(u32int);		/* 'mmu' or 'tlb'? */
extern void mousectl(Cmdbuf *cb);
extern int pcicfgrw8(int, int, int, int);
extern int pcicfgrw16(int, int, int, int);
extern int pcicfgrw32(int, int, int, int);
extern void pcieintrdone(void);
extern u32int pidget(void);
extern void pidput(u32int);
extern void prcachecfg(void);
extern vlong probeaddr(uintptr);
extern void procrestore(Proc *);
extern void procsave(Proc*);
extern void procfork(Proc*);
extern void procsetup(Proc*);
extern void putauxctl(ulong);
extern void _reset(void);
extern void screenclockson(void);
extern void screeninit(void);
extern void setcachelvl(int);
extern void setsp(uintptr);
extern void setr13(int, u32int*);
extern ulong smpon(void);
extern int startcpu(uint);
extern void stopcpu(uint);
extern int tas(void *);
extern void tegclock0init(void);
extern void tegclockinit(void);
extern void tegclockintr(void);
extern void tegclockshutdown(void);
extern void tegwdogintr(Ureg *, void *);
extern u32int ttbget(void);
extern void ttbput(u32int);
extern void _vrst(void);
extern void wakewfi(void);
extern void watchdoginit(void);
extern void wfi(void);

extern int irqenable(uint, void (*)(Ureg*, void*), void*, char*);
extern int irqdisable(uint, void (*)(Ureg*, void*), void*, char*);
#define intrenable(i, f, a, b, n)	irqenable((i), (f), (a), (n))
#define intrdisable(i, f, a, b, n)	irqdisable((i), (f), (a), (n))
extern void vectors(void);
extern void vtable(void);

/*
 * Things called in main.
 */
extern void archconfinit(void);
extern void clockinit(void);
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
extern void fpuprocfork(Proc*);
extern int fpuemu(Ureg*);

/*
 * Miscellaneous machine dependent stuff.
 */
extern int cas(int *, int, int);
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
extern void* sysexecregs(uintptr, ulong, int);
extern void sysprocsetup(Proc*);

/* libc */
long labs(long);

/*
 * PCI stuff.
 */

extern void forkret(void);
extern int userureg(Ureg*);
void*	vmap(uintptr, usize);
void vunmap(void*, usize);

#define	getpgcolor(a)	0
#define	kmapinval()

#define KADDR(pa)	((void*)(KZERO | ((uintptr)(pa) & ~KSEGM)))
#define PADDR(va)	(PHYSDRAM | ((uintptr)(va) & ~KSEGM))

#define MASK(v)	((1UL << (v)) - 1)	/* mask `v' bits wide */
