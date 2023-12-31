/*
 * Time.
 *
 * HZ should divide 1000 evenly, ideally.
 * 100, 125, 200, 250 and 333 are okay.
 */
#define	HZ		100			/* clock frequency */
#define	MS2HZ		(1000/HZ)		/* millisec per clock tick */
#define	TK2SEC(t)	((t)/HZ)		/* ticks to seconds */

enum {
	Mhz	= 1000 * 1000,
};

/*
 * More accurate time
 */
#define MS2TMR(t)	((ulong)(((uvlong)(t) * m->cpuhz)/1000))
#define US2TMR(t)	((ulong)(((uvlong)(t) * m->cpuhz)/1000000))

typedef struct Conf	Conf;
typedef struct Confmem	Confmem;
typedef struct FPsave	FPsave;
typedef struct PFPU	PFPU;
typedef struct ISAConf	ISAConf;
typedef struct Label	Label;
typedef struct Lock	Lock;
typedef struct Memcache	Memcache;
typedef struct MMMU	MMMU;
typedef struct Mach	Mach;
typedef u32int Mreg;				/* Msr - bloody UART */
typedef struct Page	Page;
typedef struct PhysUart	PhysUart;
typedef struct PMMU	PMMU;
typedef struct Proc	Proc;
typedef u32int		PTE;
typedef struct Uart	Uart;
typedef struct Ureg	Ureg;
typedef uvlong		Tval;

#pragma incomplete Ureg

#define MAXSYSARG	5	/* for mount(fd, mpt, flag, arg, srv) */

/*
 *  parameters for sysproc.c
 */
#define AOUT_MAGIC	(E_MAGIC)

struct Lock
{
	ulong	key;
	u32int	sr;
	uintptr	pc;
	Proc*	p;
	Mach*	m;
	int	isilock;
};

struct Label
{
	uintptr	sp;
	uintptr	pc;
};

/*
 * emulated floating point
 */
enum{
	Nfpctlregs = 16,
};

struct FPsave
{
	ulong	status;
	ulong	control;
	ulong	regs[Nfpctlregs][3];

	int	fpstate;
};

struct PFPU
{
	int	fpstate;
	FPsave	fpsave[1];
};

enum
{
	FPinit,
	FPactive,
	FPinactive,

	FPillegal= 0x100,
};

struct Confmem
{
	uintptr	base;
	ulong	npage;
	uintptr	limit;
	uintptr	kbase;
	uintptr	klimit;
};

struct Conf
{
	ulong	nmach;		/* processors */
	ulong	nproc;		/* processes */
	Confmem	mem[1];		/* physical memory */
	ulong	npage;		/* total physical pages of memory */
	ulong	upages;		/* user page pool */
	ulong	copymode;	/* 0 is copy on write, 1 is copy on reference */
	ulong	ialloc;		/* max interrupt time allocation in bytes */
	ulong	pipeqsize;	/* size in bytes of pipe queues */
	ulong	nimage;		/* number of page cache image headers */
	ulong	nswap;		/* number of swap pages */
	int	nswppo;		/* max # of pageouts per segment pass */
	ulong	hz;		/* processor cycle freq */
	ulong	mhz;
	int	monitor;	/* flag */
};

/*
 *  MMU stuff in Mach.
 */
struct MMMU
{
	PTE*	mmul1;		/* l1 for this processor */
	int	mmul1lo;
	int	mmul1hi;
	int	mmupid;
};

/*
 *  MMU stuff in proc
 */
#define NCOLOR	1		/* 1 level cache, don't worry about VCE's */
struct PMMU
{
	Page*	mmul2;
	Page*	mmul2cache;	/* free mmu pages */
};

#include "../port/portdat.h"

struct Mach
{
	int	machno;			/* physical id of processor */
	uintptr	splpc;			/* pc of last caller to splhi */
	Proc*	proc;			/* current process */
	MMMU;
	/* end of offsets known to asm */

	PMach;

	uvlong	fastclock;		/* last sampled value */
	int	inclockintr;

	int	cputype;
	ulong	delayloop;
	int	cpumhz;
	uvlong	cpuhz;			/* speed of cpu */

	/* save areas for exceptions, hold R0-R4 */
	u32int	sfiq[5];
	u32int	sirq[5];
	u32int	sund[5];
	u32int	sabt[5];
	u32int	smon[5];		/* probably not needed */
	u32int	ssys[5];

	uintptr	stack[1];
};

/*
 * Fake kmap.
 */
typedef void		KMap;
#define	VA(k)		((uintptr)(k))
#define	kmap(p)		(KMap*)((p)->pa|kseg0)
#define	kunmap(k)

struct
{
	char	machs[MAXMACH];		/* active CPUs */
	int	exiting;		/* shutdown */
}active;

extern register Mach* m;			/* R10 */
extern register Proc* up;			/* R9 */
extern uintptr kseg0;
extern Mach* machaddr[MAXMACH];
extern ulong memsize;

/*
 *  a parsed plan9.ini line
 */
#define NISAOPT		8

struct ISAConf {
	char	*type;
	ulong	port;
	int	irq;
	ulong	dma;
	ulong	mem;
	ulong	size;
	ulong	freq;

	int	nopt;
	char	*opt[NISAOPT];
};

#define BUSUNKNOWN -1

#define	MACHP(n)	(machaddr[n])

/*
 * Horrid. But the alternative is 'defined'.
 */
#ifdef _DBGC_
#define DBGFLG		(dbgflg[_DBGC_])
#else
#define DBGFLG		(0)
#endif /* _DBGC_ */

int vflag;
extern char dbgflg[256];

#define dbgprint	print		/* for now */

/*
 *  hardware info about a device
 */
typedef struct {
	ulong	port;
	int	size;
} Devport;

struct DevConf
{
	ulong	intnum;			/* interrupt number */
	char	*type;			/* card type, malloced */
	int	nports;			/* Number of ports */
	Devport	*ports;			/* The ports themselves */
};

enum {
	Dcache,
	Icache,
	Unified,
};

/* characteristics of a given cache level */
struct Memcache {
	uint	level;		/* 1 is nearest processor, 2 further away */
	uint	l1ip;		/* l1 I policy */

	uint	nways;		/* associativity */
	uint	nsets;
	uint	linelen;	/* bytes per cache line */
	uint	setsways;

	uint	log2linelen;
	uint	waysh;		/* shifts for set/way register */
	uint	setsh;
};

enum Dmamode {
	Const,
	Postincr,
	Index,
	Index2,
};
