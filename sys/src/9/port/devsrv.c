#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#define SRVTYPE(x)	(((uint)x)&0x3)
#define SRVPATH(x)	(((uvlong)x)>>2)
#define SRVQID(x, t)	((((uvlong)x)<<2)|((t)&0x3))

typedef struct Link Link;
struct Link
{
	void 	*link;
	char 	*name;
	char	*owner;
	ulong	perm;
	ulong 	path;
};

typedef struct Srv Srv;
struct Srv
{
	Link;
	Chan	*chan;
};

typedef struct Board Board;
struct Board
{
	Link;
	Ref;

	int 	closed;	
	Srv 	*srv;

	/* tree linkage */
	Board 	*parent;
	Board 	*child;

	/* all boards list */
	Board	*prev;
	Board	*next;
};

enum{
	Qsrv,
	Qclone,
	Qlease,
};

static char clone[] = "clone";

static RWlock srvlk;
static ulong srvpath;
static Board srvroot;

static void*
lookup(Link *l, char *name, ulong qidpath)
{
	Link *lp;

	if(qidpath != ~0UL){
		assert(SRVTYPE(qidpath) == Qsrv);
		qidpath = SRVPATH(qidpath);
	}
	for(lp = l; lp != nil; lp = lp->link){
		if(qidpath != ~0UL && lp->path == qidpath)
			return lp;
		if(name != nil && strcmp(lp->name, name) == 0)
			return lp;
	}
	return nil;
}

static void*
remove(Link **l, char *name, ulong qidpath)
{
	Link *lp;
	Link **last;

	if(qidpath != ~0UL){
		assert(SRVTYPE(qidpath) == Qsrv);
		qidpath = SRVPATH(qidpath);
	}
	last = l;
	for(lp = *l; lp != nil; lp = lp->link){
		if(qidpath != ~0UL && lp->path == qidpath)
			break;
		if(name != nil && strcmp(lp->name, name) == 0)
			break;
		last = &lp->link;
	}
	if(lp == nil)
		return nil;

	*last = lp->link;
	lp->link = nil;
	return lp;
}

static void
freelink(Link *l)
{
	free(l->name);
	free(l->owner);
	free(l);
}

/* always called with srvlock wlock'ed */
static void
boardclunk(Board *b)
{
	Board *ch;

	/* srvroot is not ref-counted nor clunkable */
	if(b == &srvroot)
		return;

	if(decref(b))
		return;

	/*
	 * All boards must be walkable from root. So a board
	 * is allowed to sit at zero references as long as it
	 * still has active children. For leaf nodes we then
	 * have to walk up the tree to clear now empty parents.
	 */
	while(b->closed && b->child == nil){
		assert(b->srv == nil);
		assert(b->parent != nil);

		/* unlink from parent board */
		ch = remove((Link**)&b->parent->child, b->name, ~0UL);
		assert(ch == b);

		/* unlink from all boards list */
		ch->prev->next = ch->next;
		ch->next->prev = ch->prev;

		b = ch->parent;
		freelink(ch);
	}
}

static int
srvgen(Chan *c, char *name, Dirtab*, int, int s, Dir *dp)
{
	Srv *sp;
	Board *b, *ch;
	Qid q;

	if(name != nil && strlen(name) >= sizeof(up->genbuf))
		return -1;

	b = c->aux;
	ch = nil;
	mkqid(&q, ~0L, 0, QTFILE);
	rlock(&srvlk);
	if(waserror()){
		runlock(&srvlk);
		nexterror();
	}
	switch(s){
	case -2: /* dot */
		ch = b;
		goto Child;
	case DEVDOTDOT:
		ch = b->parent;
		if(ch == nil)
			ch = &srvroot;
		goto Child;
	}
	if(name != nil){
		if(strcmp(name, clone) == 0)
			goto Clone;

		sp = lookup(b->srv, name, ~0UL);
		if(sp == nil)
			ch = lookup(b->child, name, ~0UL);
	} else {
		if(s == 0)
			goto Clone;
		s--;
		for(sp = b->srv; sp != nil && s > 0; sp = sp->link)
			s--;
		for(ch = b->child; ch != nil && s > 0; ch = ch->link)
			s--;
	}
	if(sp != nil){
		kstrcpy(up->genbuf, sp->name, sizeof up->genbuf);
		q.path = SRVQID(sp->path, Qsrv);
		devdir(c, q, up->genbuf, 0, sp->owner, sp->perm, dp);
	} else if(ch != nil){
Child:
		if(name != nil || s == DEVDOTDOT){
			devpermcheck(ch->owner, ch->perm, OEXEC);
			c->aux = ch;
		}
		kstrcpy(up->genbuf, ch->name, sizeof up->genbuf);
		q.path = SRVQID(ch->path, Qsrv);
		q.type = QTDIR;
		devdir(c, q, up->genbuf, 0, ch->owner, ch->perm|DMDIR, dp);
	} else if(0){
Clone:
		q.path = SRVQID(SRVPATH(c->qid.path), Qclone);
		devdir(c, q, clone, 0, eve, 0444, dp);
	} else {
		runlock(&srvlk);
		poperror();
		return -1;
	}

	runlock(&srvlk);
	poperror();
	return 1;
}

static void
srvinit(void)
{
	srvroot.next = &srvroot;
	srvroot.prev = &srvroot;
	srvroot.path = srvpath++;
	srvroot.name = "#s";
	srvroot.perm = 0777;
	kstrdup(&srvroot.owner, eve);
}

static Chan*
srvattach(char *spec)
{
	Chan *c;

	c = devattach('s', spec);
	c->aux = &srvroot;
	return c;
}

static Walkqid*
srvwalk(Chan *c, Chan *nc, char **name, int nname)
{
	Board *b;
	Walkqid *wq;

	wq = devwalk(c, nc, name, nname, 0, 0, srvgen);
	if(wq == nil || wq->clone == nil)
		return wq;

	b = wq->clone->aux;
	if(b == &srvroot)
		return wq;

	incref(b);
	return wq;
}

static int
srvstat(Chan *c, uchar *db, int n)
{
	Dir d;

	/* devstat cheats for dir stats, we care about our dir perms */
	if(c->qid.type == QTDIR){
		srvgen(c, nil, nil, 0, -2, &d);
		n = convD2M(&d, db, n);
		if(n == 0)
			error(Ebadarg);
		return n;
	}

	return devstat(c, db, n, 0, 0, srvgen);
}

static Chan*
srvopen(Chan *c, int omode)
{
	Board *b, *ch;
	Srv *sp;
	Chan *nc;
	char buf[64];
	int mode;

	if(omode&OTRUNC)
		error(Eexist);
	if(omode&ORCLOSE)
		error(Eperm);
	mode = openmode(omode);

	b = c->aux;
	if(SRVTYPE(c->qid.path) == Qclone){
		wlock(&srvlk);
		if(waserror()){
			wunlock(&srvlk);
			nexterror();
		}
		if(b->closed)
			error(Eshutdown);
		ch = smalloc(sizeof *ch);
		ch->ref = 1;
		ch->perm = 0770;
		do {
			ch->path = srvpath++;
			snprint(buf, sizeof buf, "%ld", ch->path);
		} while(lookup(b->srv, buf, ~0UL) != nil);
		kstrdup(&ch->name, buf);
		kstrdup(&ch->owner, up->user);

		/* link to all boards list */
		ch->next = &srvroot;
		ch->prev = srvroot.prev;
		ch->next->prev = ch;
		ch->prev->next = ch;

		/* link to parent board */
		ch->parent = b;
		ch->link = b->child;
		b->child = ch;

		c->aux = ch;
		c->qid.path = SRVQID(ch->path, Qlease);
		c->mode = mode;
		boardclunk(b);
		wunlock(&srvlk);
		poperror();
		return c;
	}

	rlock(&srvlk);
	if(waserror()){
		runlock(&srvlk);
		nexterror();
	}
	if(c->qid.type == QTDIR){
		if(omode != OREAD)
			error(Eisdir);
		devpermcheck(b->owner, b->perm, omode);
		c->mode = mode;
		c->flag |= COPEN;
		c->offset = 0;
		runlock(&srvlk);
		poperror();
		return c;
	}
	if(b->closed)
		error(Eshutdown);

	sp = lookup(b->srv, nil, c->qid.path);
	if(sp == nil)
		error(Eshutdown);
	nc = sp->chan;
	if(nc == nil)
		error(Eshutdown);
	if(mode != nc->mode && nc->mode != ORDWR)
		error(Eperm);
	devpermcheck(sp->owner, sp->perm, omode);

	incref(nc);

	runlock(&srvlk);
	poperror();

	cclose(c);
	return nc;
}

static Chan*
srvcreate(Chan *c, char *name, int omode, ulong perm)
{
	Board *b;
	Srv *sp;

	if(openmode(omode) != OWRITE)
		error(Eperm);

	if(strlen(name) >= sizeof(up->genbuf))
		error(Etoolong);

	if(strcmp(name, clone) == 0)
		error(Eexist);

	sp = smalloc(sizeof *sp);
	kstrdup(&sp->name, name);
	kstrdup(&sp->owner, up->user);

	b = c->aux;
	wlock(&srvlk);
	if(waserror()){
		wunlock(&srvlk);
		freelink(sp);
		nexterror();
	}
	if(b->closed)
		error(Eshutdown);
	devpermcheck(b->owner, b->perm, OWRITE);
	if(lookup(b->srv, name, ~0UL) != nil)
		error(Eexist);
	if(lookup(b->child, name, ~0UL) != nil)
		error(Eexist);

	sp->perm = perm&0777;
	sp->path = srvpath++;

	c->qid.path = SRVQID(sp->path, Qsrv);
	c->qid.type = QTFILE;

	sp->link = b->srv;
	b->srv = sp;

	wunlock(&srvlk);
	poperror();

	c->flag |= COPEN;
	c->mode = OWRITE;

	return c;
}

static void
srvremove(Chan *c)
{
	Board *b;
	Srv *sp;

	b = c->aux;
	wlock(&srvlk);
	if(waserror()){
		boardclunk(b);
		wunlock(&srvlk);
		nexterror();
	}
	if(c->qid.type == QTDIR)
		error(Eperm);
	switch(SRVTYPE(c->qid.path)){
	case Qlease:
	case Qclone:
		error(Eperm);
	}

	sp = lookup(b->srv, nil, c->qid.path);
	if(sp == nil)
		error(Enonexist);

	if(strcmp(sp->owner, up->user) != 0 && !iseve())
		error(Eperm);

	remove((Link**)&b->srv, nil, c->qid.path);

	boardclunk(b);
	wunlock(&srvlk);
	poperror();

	if(sp->chan != nil)
		cclose(sp->chan);
	freelink(sp);
}

static int
srvwstat(Chan *c, uchar *dp, int n)
{
	Board *b, *s;
	char *strs;
	Dir d;
	Link *lp;

	switch(SRVTYPE(c->qid.path)){
	case Qlease:
	case Qclone:
		error(Eperm);
	}
	if(c->qid.type == QTDIR && c->aux == &srvroot)
		error(Eperm);

	strs = smalloc(n);
	if(waserror()){
		free(strs);
		nexterror();
	}
	n = convM2D(dp, n, &d, strs);
	if(n == 0)
		error(Eshortstat);

	b = c->aux;
	wlock(&srvlk);
	if(waserror()){
		wunlock(&srvlk);
		nexterror();
	}
	if(b->closed)
		error(Eshutdown);

	if(c->qid.type == QTDIR)
		lp = b;
	else
		lp = lookup(b->srv, nil, c->qid.path);
	if(lp == nil)
		error(Enonexist);

	if(strcmp(lp->owner, up->user) != 0 && !iseve())
		error(Eperm);

	if(d.name != nil && *d.name && strcmp(lp->name, d.name) != 0) {
		if(strchr(d.name, '/') != nil)
			error(Ebadchar);
		if(strlen(d.name) >= sizeof(up->genbuf))
			error(Etoolong);

		/* Ensure new name doesn't conflict with old names */
		if(strcmp(d.name, clone) == 0)
			error(Eexist);
		if(c->qid.type == QTDIR)
			s = b->parent;
		else
			s = b;
		if(lookup(s->srv, d.name, ~0UL) != nil)
			error(Eexist);
		if(lookup(s->child, d.name, ~0UL) != nil)
			error(Eexist);
		kstrdup(&lp->name, d.name);
	}
	if(d.uid != nil && *d.uid)
		kstrdup(&lp->owner, d.uid);
	if(d.mode != ~0UL)
		lp->perm = d.mode & 0777;

	wunlock(&srvlk);
	poperror();

	free(strs);
	poperror();

	return n;
}

static void
srvclose(Chan *c)
{
	Srv *sp, *link;
	Board *b;

	if((c->flag & CRCLOSE) != 0 && SRVTYPE(c->qid.path) != Qlease){
		/*
		 * in theory we need to override any changes in removability
		 * since open, but since all that's checked is the owner,
	 	 * which is immutable, all is well.
	 	 */
		if(waserror())
			return;
		srvremove(c);
		poperror();
		return;
	}

	b = c->aux;
	if(b == &srvroot)
		return;

	wlock(&srvlk);
	if(SRVTYPE(c->qid.path) != Qlease){
		boardclunk(b);
		wunlock(&srvlk);
		return;
	}

	/* free later after releasing srvlk */
	sp = b->srv;
	b->srv = nil;
	b->closed++;
	boardclunk(b);
	wunlock(&srvlk);

	for(; sp != nil; sp = link){
		link = sp->link;
		if(sp->chan != nil)
			ccloseq(sp->chan);
		freelink(sp);
	}
}

static long
srvread(Chan *c, void *va, long n, vlong off)
{
	Board *b;

	if(SRVTYPE(c->qid.path) == Qlease){
		b = c->aux;
		rlock(&srvlk);
		if(waserror()){
			runlock(&srvlk);
			nexterror();
		}
		n = readstr((ulong)off, va, n, b->name);
		runlock(&srvlk);
		poperror();
		return n;
	}
	isdir(c);
	return devdirread(c, va, n, 0, 0, srvgen);
}

static long
srvwrite(Chan *c, void *va, long n, vlong)
{
	Board *b;
	Srv *sp;
	Chan *c1;
	int fd;
	char buf[32];

	if(SRVTYPE(c->qid.path) == Qlease)
		error(Eperm);

	if(n >= sizeof buf)
		error(Etoobig);
	memmove(buf, va, n);	/* so we can NUL-terminate */
	buf[n] = 0;
	fd = strtoul(buf, 0, 0);

	c1 = fdtochan(fd, -1, 0, 1);	/* error check and inc ref */

	b = c->aux;
	wlock(&srvlk);
	if(waserror()) {
		wunlock(&srvlk);
		cclose(c1);
		nexterror();
	}
	if(b->closed)
		error(Eshutdown);
	if(c1->qid.type & QTAUTH)
		error("cannot post auth file in srv");
	sp = lookup(b->srv, nil, c->qid.path);
	if(sp == nil)
		error(Enonexist);

	if(sp->chan != nil)
		error(Ebadusefd);

	sp->chan = c1;

	if(c1->srvname == nil)
		kstrdup(&c1->srvname, c->path->s);

	wunlock(&srvlk);
	poperror();
	return n;
}

Dev srvdevtab = {
	's',
	"srv",

	devreset,
	srvinit,	
	devshutdown,
	srvattach,
	srvwalk,
	srvstat,
	srvopen,
	srvcreate,
	srvclose,
	srvread,
	devbread,
	srvwrite,
	devbwrite,
	srvremove,
	srvwstat,
};

void
srvrenameuser(char *old, char *new)
{
	Board *b;
	Srv *sp;

	wlock(&srvlk);
	b = &srvroot;
	do {
		if(strcmp(b->owner, old) == 0)
			kstrdup(&b->owner, new);
		for(sp = b->srv; sp != nil; sp = sp->link)
			if(strcmp(sp->owner, old) == 0)
				kstrdup(&sp->owner, new);
		b = b->next;
	} while(b != &srvroot);
	wunlock(&srvlk);
}
