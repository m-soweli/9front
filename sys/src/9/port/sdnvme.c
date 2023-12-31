#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/pci.h"
#include "ureg.h"
#include "../port/error.h"

#include "../port/sd.h"

typedef struct WS WS;
typedef struct CQ CQ;
typedef struct SQ SQ;
typedef struct Ctlr Ctlr;

struct WS
{
	u32int	cdw0;
	ushort	status;
	Rendez	*sleep;
	WS	**link;
	SQ	*queue;
};

struct CQ
{
	u32int	head;
	u32int	mask;
	u32int	shift;
	u32int	*base;
	Ctlr	*ctlr;
};

struct SQ
{
	u32int	tail;
	u32int	mask;
	u32int	shift;
	u32int	*base;
	WS	**wait;
	Ctlr	*ctlr;
	Lock;
};

struct Ctlr
{
	QLock;

	Lock	intr;
	u32int	ints;
	u32int	irqc[2];

	Pcidev	*pci;
	u32int	*reg;

	u64int	cap;
	uchar	*ident;
	u32int	*nsid;
	int	nnsid;

	u32int	mps;		/* mps = 1<<mpsshift */
	u32int	mpsshift;
	u32int	dstrd;

	u32int	nsq;

	CQ	cq[1+1];
	SQ	sq[1+MAXMACH];

	Ctlr	*next;
};

/* controller registers */
enum {
	Cap0,
	Cap1,
	Ver,
	IntMs,
	IntMc,
	CCfg,

	CSts = 0x1C/4,
	Nssr,
	AQAttr,
	ASQBase0,
	ASQBase1,
	ACQBase0,
	ACQBase1,

	DBell = 0x1000/4,
};

static u32int*
qcmd(WS *ws, Ctlr *ctlr, int adm, u32int opc, u32int nsid, void *data, ulong len)
{
	u32int cid, *e;
	u64int pa;
	SQ *sq;

	if(!adm){
	Retry:
		splhi();
		sq = &ctlr->sq[1+(m->machno % ctlr->nsq)];
		if(conf.nmach > ctlr->nsq)
			lock(sq);
	} else {
		qlock(ctlr);
		sq = &ctlr->sq[0];
	}
	ws->sleep = &up->sleep;
	ws->queue = sq;
	ws->link = &sq->wait[sq->tail & sq->mask];
	while(*ws->link != nil){
		/* should be very rare */
		if(!adm){
			if(conf.nmach > ctlr->nsq)
				unlock(sq);
			sched();
			goto Retry;
		}
		sched();
	}
	*ws->link = ws;

	e = &sq->base[((cid = sq->tail++) & sq->mask)<<4];
	e[0] = opc | cid<<16;
	e[1] = nsid;
	e[2] = 0;
	e[3] = 0;
	e[4] = 0;
	e[5] = 0;
	if(len > 0){
		dmaflush(1, data, len);
		pa = PCIWADDR(data);
		e[6] = pa;
		e[7] = pa>>32;
		if(len > ctlr->mps - (pa & ctlr->mps-1))
			pa += ctlr->mps - (pa & ctlr->mps-1);
		else
			pa = 0;
	} else {
		e[6] = 0;
		e[7] = 0;
		pa = 0;
	}
	e[8] = pa;
	e[9] = pa>>32;
	return e;
}

static void
nvmeintr(Ureg *, void *arg)
{
	u32int phaseshift, *e;
	WS *ws, **wp;
	Ctlr *ctlr;
	SQ *sq;
	CQ *cq;

	ctlr = arg;
	if(ctlr->ints == 0)
		return;

	ilock(&ctlr->intr);
	ctlr->reg[IntMs] = ctlr->ints;
	for(cq = &ctlr->cq[nelem(ctlr->cq)-1]; cq >= ctlr->cq; cq--){
		if(cq->base == nil)
			continue;
		phaseshift = 16 - cq->shift;
		for(;;){
			e = &cq->base[(cq->head & cq->mask)<<2];
			dmaflush(0, e, 32);
			if(((e[3] ^ (cq->head << phaseshift)) & 0x10000) == 0)
				break;

			if(0) iprint("nvmeintr: cq%d [%.4ux] %.8ux %.8ux %.8ux %.8ux\n",
				(int)(cq - ctlr->cq), cq->head & cq->mask,
				e[0], e[1], e[2], e[3]);

			sq = &ctlr->sq[e[2] >> 16];
			wp = &sq->wait[e[3] & sq->mask];
			if((ws = *wp) != nil && ws->link == wp){
				Rendez *z = ws->sleep;
				ws->cdw0 = e[0];
				ws->status = e[3]>>17;
				*wp = nil;
				wakeup(z);
			}
			ctlr->reg[DBell + ((cq-ctlr->cq)*2+1 << ctlr->dstrd)] = ++cq->head & cq->mask;
		}
	}
	ctlr->reg[IntMc] = ctlr->ints;
	iunlock(&ctlr->intr);
}

static int
wdone(void *arg)
{
	WS *ws = arg;
	return *ws->link != ws;
}

static u32int
wcmd(WS *ws, u32int *e)
{
	SQ *sq = ws->queue;
	Ctlr *ctlr = sq->ctlr;

	if(e != nil){
		dmaflush(1, e, 64);
	}
	coherence();
	ctlr->reg[DBell + ((sq-ctlr->sq)*2+0 << ctlr->dstrd)] = sq->tail & sq->mask;
	if(sq > ctlr->sq) {
		assert(sq == &ctlr->sq[1+(m->machno % ctlr->nsq)]);
		if(conf.nmach > ctlr->nsq)
			unlock(sq);
		spllo();
	} else
		qunlock(sq->ctlr);
	while(waserror())
		;
	tsleep(ws->sleep, wdone, ws, 5);
	while(!wdone(ws)){
		nvmeintr(nil, ctlr);
		tsleep(ws->sleep, wdone, ws, 10);
	}
	poperror();
	return ws->status;
}

void
checkstatus(u32int status, char *info)
{
	if(status == 0)
		return;
	snprint(up->genbuf, sizeof(up->genbuf), "%s: status %ux", info, status);
	error(up->genbuf);
}

static long
nvmebio(SDunit *u, int lun, int write, void *a, long count, uvlong lba)
{
	u32int nsid, s, n, m, *e;
	Ctlr *ctlr;
	uchar *p;
	WS ws;

	USED(lun);

	ctlr = u->dev->ctlr;
	nsid = ctlr->nsid[u->subno];
	s = u->secsize;
	p = a;
	while(count > 0){
		m = (2*ctlr->mps - ((uintptr)p & ctlr->mps-1)) / s;
		if((n = count) > m)
			n = m;
		e = qcmd(&ws, ctlr, 0, write ? 0x01 : 0x02, nsid, p, n*s);
		e[10] = lba;
		e[11] = lba>>32;
		e[12] = n-1;
		e[13] = (count>n)<<6;	/* sequential request */
		e[14] = 0;
		e[15] = 0;
		checkstatus(wcmd(&ws, e), write ? "write" : "read");
		p += n*s;
		count -= n;
		lba += n;
	}
	if(!write){
		dmaflush(0, a, p - (uchar*)a);
	}
	return p - (uchar*)a;
}

static int
nvmerio(SDreq *r)
{
	int i, count, rw;
	uvlong lba;
	SDunit *u;

	u = r->unit;
	if(r->cmd[0] == 0x35 || r->cmd[0] == 0x91)
		return sdsetsense(r, SDok, 0, 0, 0);
	if((i = sdfakescsi(r)) != SDnostatus)
		return r->status = i;
	if((i = sdfakescsirw(r, &lba, &count, &rw)) != SDnostatus)
		return i;
	r->rlen = nvmebio(u, r->lun, rw == SDwrite, r->data, count, lba);
	return r->status = SDok;
}

static u64int
get64(uchar *p)
{
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24
		| (u64int)p[4]<<32
		| (u64int)p[5]<<40
		| (u64int)p[6]<<48
		| (u64int)p[7]<<56;
}

static long
readsmart(SDunit *u, Chan *, void *a, long n, vlong off)
{
	Ctlr *ctlr = u->dev->ctlr;
	char *buf, *p, *e;
	uchar *info;
	u32int nsid, *q;
	WS ws;

	buf = smalloc(READSTR);
	if(waserror()){
		free(buf);
		nexterror();
	}
	p = buf;
	e = buf + READSTR;

	info = mallocalign(0x1000, ctlr->mps, 0, 0);
	if(info == nil)
		error(Enomem);
	if(waserror()){
		free(info);
		nexterror();
	}

	/*
	 * Log Page Attributes (LPA) Bit0: If set to '1' then te controller
	 * supports SMART / Health information log page on a per namespace basis.
	 */
	nsid = (ctlr->ident[261] & 1) != 0 ? ctlr->nsid[u->subno] : 0xffffffff;

	q = qcmd(&ws, ctlr, 1, 0x02, nsid, info, 0x1000);
	q[10] = (512/4)<<16 | 0x2;
	q[11] = 0;
	q[12] = 0;
	q[13] = 0;
	q[14] = 0;
	checkstatus(wcmd(&ws, q), "read SMART/health info");
	dmaflush(0, info, 0x1000);

	p = seprint(p, e, "Critical Warning:\t");
	if(info[0]&(1<<0))
		p = seprint(p, e, "Available Spare,");
	if(info[0]&(1<<1))
		p = seprint(p, e, "Temperature Exceeded,");
	if(info[0]&(1<<2))
		p = seprint(p, e, "Reliability Degraded,");
	if(info[0]&(1<<3))
		p = seprint(p, e, "Read only mode,");
	if(info[0]&(1<<4))
		p = seprint(p, e, "Backup failed,");
	p[-1] = '\n';

	p = seprint(p, e, "Temperature:\t%d\n", (info[2]<<8 | info[1]) - 273);

	p = seprint(p, e, "Available Spare:\t%d%%\n", info[3]);
	p = seprint(p, e, "Available Spare Threshold:\t%d%%\n", info[4]);

	p = seprint(p, e, "Percentage Used:\t%d%%\n", info[5]);

	p = seprint(p, e, "Data Units Read:\t%llud\n", get64(info+32));
	p = seprint(p, e, "Data Units Written:\t%llud\n", get64(info+48));
	p = seprint(p, e, "Host Read Commands:\t%llud\n", get64(info+64));
	p = seprint(p, e, "Host Write Commands:\t%llud\n", get64(info+80));
	p = seprint(p, e, "Controller Busy Time:\t%llud:%.2d\n", get64(info+96)/60, (int)(get64(info+96)%60));
	p = seprint(p, e, "Power Cycles:\t%llud\n", get64(info+112));
	p = seprint(p, e, "Power On Hours:\t%llud\n", get64(info+128));
	p = seprint(p, e, "Unsafe Shutdowns:\t%llud\n", get64(info+144));
	p = seprint(p, e, "Media Errors:\t%llud\n", get64(info+160));
	USED(p);

	free(info);
	poperror();

	n = readstr(off, a, n, buf);
	free(buf);
	poperror();

	return n;
}

static int
nvmeverify(SDunit *u)
{
	Ctlr *ctlr = u->dev->ctlr;

	if(u->subno >= ctlr->nnsid)
		return 0;
	sdaddfile(u, "smart", 0440, eve, readsmart, nil);
	return 1;
}

static int
nvmeonline(SDunit *u)
{
	u32int *e, lbaf;
	uchar *info, *p;
	Ctlr *ctlr;
	WS ws;

	if(u->sectors != 0)
		return 1;

	ctlr = u->dev->ctlr;
	if((info = mallocalign(0x1000, ctlr->mps, 0, 0)) == nil)
		return 0;

	e = qcmd(&ws, ctlr, 1, 0x06, ctlr->nsid[u->subno], info, 0x1000);
	e[10] = 0; // identify namespace
	if(wcmd(&ws, e) != 0){
		free(info);
		return 0;
	}
	dmaflush(0, info, 0x1000);
	u->sectors = get64(info);
	p = &info[128 + 4*(info[26]&15)];
	lbaf = p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
	u->secsize = 1<<((lbaf>>16)&0xFF);
	free(info);

	memset(u->inquiry, 0, sizeof u->inquiry);
	u->inquiry[2] = 2;
	u->inquiry[3] = 2;
	u->inquiry[4] = sizeof u->inquiry - 4;
	memmove(u->inquiry+8, ctlr->ident+24, 20);

	return 2;
}

static char*
nvmerctl(SDunit *u, char *p, char *e)
{
	Ctlr *ctlr;

	if((ctlr = u->dev->ctlr) == nil || ctlr->ident == nil)
		return p;

	p = seprint(p, e, "model\t%.40s\n", (char*)ctlr->ident+24);
	p = seprint(p, e, "serial\t%.20s\n", (char*)ctlr->ident+4);
	p = seprint(p, e, "firm\t%.8s\n", (char*)ctlr->ident+64);
	p = seprint(p, e, "geometry %llud %lud\n", u->sectors, u->secsize);

	return p;
}

static void*
cqalloc(Ctlr *ctlr, CQ *cq, u32int lgsize)
{
	cq->ctlr = ctlr;
	cq->head = 0;
	cq->shift = lgsize-4;
	cq->mask = (1<<cq->shift)-1;
	if((cq->base = mallocalign(1<<lgsize, ctlr->mps, 0, 0)) == nil)
		error(Enomem);
	memset(cq->base, 0, 1<<lgsize);
	return cq->base;
}

static void*
sqalloc(Ctlr *ctlr, SQ *sq, u32int lgsize)
{
	sq->ctlr = ctlr;
	sq->tail = 0;
	sq->shift = lgsize-6;
	sq->mask = (1<<sq->shift)-1;
	if((sq->base = mallocalign(1<<lgsize, ctlr->mps, 0, 0)) == nil)
		error(Enomem);
	if((sq->wait = mallocz(sizeof(WS*)*(sq->mask+1), 1)) == nil)
		error(Enomem);
	memset(sq->base, 0, 1<<lgsize);
	return sq->base;
}

static void
setupqueues(Ctlr *ctlr)
{
	u32int mqes, lgcqsize, lgsqsize, nsq, st, *e;
	CQ *cq;
	SQ *sq;
	WS ws;
	int i;

	mqes = 1 + (ctlr->cap & 0xFFFF);
	if(mqes < 2)
		mqes = 2;
	for(lgsqsize = 0; 1<<lgsqsize < mqes; lgsqsize++)
		;
	if(lgsqsize > 12-6)
		lgsqsize = 12-6;
	nsq = conf.nmach;
	while((lgcqsize = lgsqsize) > 0){
		while(nsq >= 1<<lgcqsize)
			nsq >>= 1;
		while(1<<lgcqsize < nsq<<lgsqsize)
			lgcqsize++;
		if(1<<lgcqsize <= mqes)
			break;
		lgsqsize--;
	}
	lgsqsize += 6;
	lgcqsize += 4;

	/* CQID1: shared completion queue */
	cq = &ctlr->cq[1];
	cqalloc(ctlr, cq, lgcqsize);
	e = qcmd(&ws, ctlr, 1, 0x05, 0, cq->base, 1<<lgcqsize);
	e[10] = (cq - ctlr->cq) | cq->mask<<16;
	e[11] = 3; /* IEN | PC */
	checkstatus(wcmd(&ws, e), "create completion queue");

	st = 0;

	/* SQID[1..nmach]: submission queue per cpu */
	for(i=1; i<=nsq; i++){
		sq = &ctlr->sq[i];
		sqalloc(ctlr, sq, lgsqsize);
		e = qcmd(&ws, ctlr, 1, 0x01, 0, sq->base, 1<<lgsqsize);
		e[10] = i | sq->mask<<16;
		e[11] = (cq - ctlr->cq)<<16 | 1;	/* CQID<<16 | PC */
		st = wcmd(&ws, e);
		if(st != 0){
			free(sq->base);
			free(sq->wait);
			memset(sq, 0, sizeof(*sq));
			break;
		}
	}
	
	ctlr->nsq = i - 1;
	if(ctlr->nsq < 1)
		checkstatus(st, "create submission queues");

	ilock(&ctlr->intr);
	ctlr->ints |= 1<<(cq - ctlr->cq);
	ctlr->reg[IntMc] = ctlr->ints;
	iunlock(&ctlr->intr);
}

static void
identify(Ctlr *ctlr)
{
	u32int *e;
	WS ws;
	
	if(ctlr->ident == nil)
		if((ctlr->ident = mallocalign(0x1000, ctlr->mps, 0, 0)) == nil)
			error(Enomem);
	if(ctlr->nsid == nil)
		if((ctlr->nsid = mallocalign(0x1000, ctlr->mps, 0, 0)) == nil)
			error(Enomem);

	e = qcmd(&ws, ctlr, 1, 0x06, 0, ctlr->ident, 0x1000);
	e[10] = 1; // identify controller
	checkstatus(wcmd(&ws, e), "identify controller");
	dmaflush(0, ctlr->ident, 0x1000);

	e = qcmd(&ws, ctlr, 1, 0x06, 0, ctlr->nsid, 0x1000);
	e[10] = 2; // namespace list 
	if(wcmd(&ws, e) == 0) {
		dmaflush(0, ctlr->nsid, 0x1000);
	} else
		ctlr->nsid[0] = 1;	/* assume namespace #1 */

	ctlr->nnsid = 0;
	while(ctlr->nnsid < 1024 && ctlr->nsid[ctlr->nnsid] != 0)
		ctlr->nnsid++;
}

static int
nvmedisable(SDev *sd)
{
	char name[32];
	Ctlr *ctlr;
	int i;

	ctlr = sd->ctlr;

	/* mask interrupts */
	ilock(&ctlr->intr);
	ctlr->ints = 0;
	ctlr->reg[IntMs] = ~ctlr->ints;
	iunlock(&ctlr->intr);

	/* notify normal power off */
	ctlr->reg[CCfg] = (ctlr->reg[CCfg] & ~(3<<14)) | 1<<14;
	for(i = 0; i < 3000; i++){
		if((ctlr->reg[CSts] & 0xc) == 0x8)
			break;
		delay(1);
	}

	/* disable controller */
	ctlr->reg[CCfg] = 0;
	for(i = 0; i < 1000; i++){
		if((ctlr->reg[CSts] & 1) == 0)
			break;
		delay(1);
	}

	snprint(name, sizeof(name), "%s (%s)", sd->name, sd->ifc->name);
	intrdisable(ctlr->pci->intl, nvmeintr, ctlr, ctlr->pci->tbdf, name);

	pciclrbme(ctlr->pci);	/* dma disable */

	for(i=0; i<nelem(ctlr->sq); i++){
		free(ctlr->sq[i].base);
		free(ctlr->sq[i].wait);
	}
	for(i=0; i<nelem(ctlr->cq); i++)
		free(ctlr->cq[i].base);

	memset(ctlr->sq, 0, sizeof(ctlr->sq));
	memset(ctlr->cq, 0, sizeof(ctlr->cq));

	free(ctlr->ident);
	ctlr->ident = nil;
	free(ctlr->nsid);
	ctlr->nsid = nil;
	ctlr->nnsid = 0;

	return 1;
}

static int
nvmeenable(SDev *sd)
{
	char name[32];
	Ctlr *ctlr;
	u64int pa;
	int to;

	ctlr = sd->ctlr;

	snprint(name, sizeof(name), "%s (%s)", sd->name, sd->ifc->name);
	intrenable(ctlr->pci->intl, nvmeintr, ctlr, ctlr->pci->tbdf, name);

	if(waserror()){
		print("%s: %s\n", name, up->errstr);
		nvmedisable(sd);
		sd->nunit = 0;	/* hack: prevent further probing */
		return 0;
	}
	
	pa = PCIWADDR(cqalloc(ctlr, &ctlr->cq[0], ctlr->mpsshift));
	dmaflush(1, ctlr->cq[0].base, 1<<ctlr->mpsshift);
	ctlr->reg[ACQBase0] = pa;
	ctlr->reg[ACQBase1] = pa>>32;

	pa = PCIWADDR(sqalloc(ctlr, &ctlr->sq[0], ctlr->mpsshift));
	dmaflush(1, ctlr->sq[0].base, 1<<ctlr->mpsshift);
	ctlr->reg[ASQBase0] = pa;
	ctlr->reg[ASQBase1] = pa>>32;

	ctlr->reg[AQAttr] = ctlr->sq[0].mask | ctlr->cq[0].mask<<16;

	/* dma enable */
	pcisetbme(ctlr->pci);

	/* enable interrupt */
	ilock(&ctlr->intr);
	ctlr->ints = 1;
	ctlr->reg[IntMc] = ctlr->ints;
	iunlock(&ctlr->intr);

	/* enable controller */
	ctlr->reg[CCfg] = 1 | (ctlr->mpsshift-12)<<7 | 6<<16 | 4<<20;

	for(to = (ctlr->cap>>24) & 255; to >= 0; to--){
		tsleep(&up->sleep, return0, nil, 500);
		if((ctlr->reg[CSts] & 3) == 1)
			goto Ready;
	}
	if(ctlr->reg[CSts] & 2)
		error("fatal controller status during initialization");
	error("controller initialization timeout");
Ready:
	identify(ctlr);
	setupqueues(ctlr);
	print("%s: using %d submit queues\n", name, ctlr->nsq);
	poperror();

	return 1;
}

static Ctlr*
nvmepnpctlrs(void)
{
	Ctlr *ctlr, *h, *t;
	Pcidev *p;
	int i;

	h = t = nil;
	for(p = nil; p = pcimatch(p, 0, 0);){
		if(p->ccrb != 1 || p->ccru != 8 || p->ccrp != 2)
			continue;
		if(p->mem[0].size == 0 || (p->mem[0].bar & 1) != 0)
			continue;
		if((ctlr = malloc(sizeof(*ctlr))) == nil){
			print("nvme: no memory for Ctlr\n");
			break;
		}
		pcienable(p);
		ctlr->pci = p;
		ctlr->reg = vmap(p->mem[0].bar & ~0xF, p->mem[0].size);
		if(ctlr->reg == nil){
			print("nvme: can't vmap bar0\n");
		Bad:
			if(ctlr->reg != nil)
				vunmap(ctlr->reg, p->mem[0].size);
			pcidisable(p);
			free(ctlr);
			continue;
		}
		ctlr->cap = ctlr->reg[Cap0];
		ctlr->cap |= (u64int)ctlr->reg[Cap1]<<32;

		/* mask interrupts */
		ctlr->ints = 0;
		ctlr->reg[IntMs] = ~ctlr->ints;

		/* disable controller */
		ctlr->reg[CCfg] = 0;

		if((ctlr->cap&(1ULL<<37)) == 0){
			print("nvme: doesnt support NVM commactlr set: %ux\n",
				(u32int)(ctlr->cap>>37) & 0xFF);
			goto Bad;
		}

		/* use 64K page size when possible */
		ctlr->dstrd = (ctlr->cap >> 32) & 15;
		for(i = (ctlr->cap >> 48) & 15; i < ((ctlr->cap >> 52) & 15); i++){
			if(i >= 16-12)	/* 64K */
				break;
		}
		ctlr->mpsshift = i+12;
		ctlr->mps = 1 << ctlr->mpsshift;

		if(h == nil)
			h = ctlr;
		else
			t->next = ctlr;
		t = ctlr;
	}

	return h;
}

SDifc sdnvmeifc;

static SDev*
nvmepnp(void)
{
	SDev *s, *h, *t;
	Ctlr *ctlr;
	int id;

	h = t = nil;

	id = 'N';
	for(ctlr = nvmepnpctlrs(); ctlr != nil; ctlr = ctlr->next){
		if((s = malloc(sizeof(*s))) == nil)
			break;
		s->ctlr = ctlr;
		s->idno = id++;
		s->ifc = &sdnvmeifc;
		s->nunit = 1024;
		if(h)
			t->next = s;
		else
			h = s;
		t = s;
	}

	return h;
}

SDifc sdnvmeifc = {
	"nvme",				/* name */

	nvmepnp,			/* pnp */
	nvmeenable,			/* enable */
	nvmedisable,			/* disable */

	nvmeverify,			/* verify */
	nvmeonline,			/* online */
	nvmerio,			/* rio */
	nvmerctl,			/* rctl */
	nil,				/* wctl */

	nvmebio,			/* bio */
	nil,				/* probe */
	nil,				/* clear */
	nil,				/* rtopctl */
	nil,				/* wtopctl */
};
