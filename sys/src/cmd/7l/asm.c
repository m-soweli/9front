#include	"l.h"

long	OFFSET;

#define PADDR(a)	((a) & ~0xfffffffff0000000ull)

vlong
entryvalue(void)
{
	char *a;
	Sym *s;

	a = INITENTRY;
	if(*a >= '0' && *a <= '9')
		return atolwhex(a);
	s = lookup(a, 0);
	if(s->type == 0)
		return INITTEXT;
	switch(s->type) {
	case STEXT:
	case SLEAF:
		break;
	case SDATA:
		if(dlm)
			return s->value+INITDAT;
	default:
		diag("entry not text: %s", s->name);
	}
	return s->value;
}

void
cflush(void)
{
	int n;

	n = sizeof(buf.cbuf) - cbc;
	if(n)
		write(cout, buf.cbuf, n);
	cbp = buf.cbuf;
	cbc = sizeof(buf.cbuf);
}

void
asmb(void)
{
	Prog *p;
	long magic;
	vlong vl, etext;
	Optab *o;
	uchar *sbuf, *dbuf;

	if(debug['v'])
		Bprint(&bso, "%5.2f asm\n", cputime());
	Bflush(&bso);
	OFFSET = HEADR;
	seek(cout, OFFSET, 0);
	pc = INITTEXT;
	for(p = firstp; p != P; p = p->link) {
		if(p->as == ATEXT) {
			curtext = p;
			autosize = p->to.offset + PCSZ;
		}else if(p->as == ADWORD && (pc & 7) != 0) {
			lputl(0);
			pc += 4;
		}
		if(p->pc != pc) {
			diag("phase error %llux sb %llux",
				p->pc, pc);
			if(!debug['a'])
				prasm(curp);
			pc = p->pc;
		}
		curp = p;
		o = oplook(p);	/* could probably avoid this call */
		asmout(p, o);
		pc += o->size;
	}

	if(debug['a'])
		Bprint(&bso, "\n");
	Bflush(&bso);
	cflush();

	/* output strings in text segment */
	etext = INITTEXT + textsize;
	sbuf = calloc(1, etext-pc);
	dbuf = calloc(1, datsize);
	datfill(sbuf, pc, dbuf, 0);
	write(cout, sbuf, etext-pc);
	free(sbuf);

	curtext = P;
	switch(HEADTYPE) {
	case 0:
	case 2:
	case 7:
		OFFSET = HEADR+textsize;
		seek(cout, OFFSET, 0);
		break;
	case 6:	/* no header, padded segments */
		OFFSET = rnd(HEADR+textsize, INITRND);
		seek(cout, OFFSET, 0);
		break;
	}
	if(dlm){
		char buf[8];

		write(cout, buf, INITDAT-textsize);
		textsize = INITDAT;
	}
	write(cout, dbuf, datsize);
	free(dbuf);

	symsize = 0;
	lcsize = 0;
	if(!debug['s']) {
		if(debug['v'])
			Bprint(&bso, "%5.2f sym\n", cputime());
		Bflush(&bso);
		switch(HEADTYPE) {
		case 0:
			debug['s'] = 1;
			break;
		case 2:
			OFFSET = HEADR+textsize+datsize;
			seek(cout, OFFSET, 0);
			break;
		case 6:	/* no header, padded segments */
			OFFSET += rnd(datsize, INITRND);
			seek(cout, OFFSET, 0);
			break;
		case 7:
			break;
		}
		if(!debug['s'])
			asmsym();
		if(debug['v'])
			Bprint(&bso, "%5.2f pc\n", cputime());
		Bflush(&bso);
		if(!debug['s'])
			asmlc();
		if(dlm)
			asmdyn();
		cflush();
	}
	else if(dlm){
		seek(cout, HEADR+textsize+datsize, 0);
		asmdyn();
		cflush();
	}

	if(debug['v'])
		Bprint(&bso, "%5.2f header\n", cputime());
	Bflush(&bso);
	OFFSET = 0;
	seek(cout, OFFSET, 0);
	switch(HEADTYPE) {
	case 0:	/* no header */
	case 6:	/* no header, padded segments */
		break;
	case 2:	/* plan 9 */
		magic = 4*28*28+7;
		magic |= 0x00008000;		/* fat header */
		if(dlm)
			magic |= 0x80000000;	/* dlm */
		lput(magic);			/* magic */
		lput(textsize);			/* sizes */
		lput(datsize);
		lput(bsssize);
		lput(symsize);			/* nsyms */
		vl = entryvalue();
		lput(PADDR(vl));		/* va of entry */
		lput(0L);
		lput(lcsize);
		llput(vl);			/* va of entry */
		break;
	}
	cflush();
}

void
cput(int c)
{
	cbp[0] = c;
	cbp++;
	cbc--;
	if(cbc <= 0)
		cflush();
}

void
wput(long l)
{

	cbp[0] = l>>8;
	cbp[1] = l;
	cbp += 2;
	cbc -= 2;
	if(cbc <= 0)
		cflush();
}

void
wputl(long l)
{

	cbp[0] = l;
	cbp[1] = l>>8;
	cbp += 2;
	cbc -= 2;
	if(cbc <= 0)
		cflush();
}

void
lput(long l)
{

	cbp[0] = l>>24;
	cbp[1] = l>>16;
	cbp[2] = l>>8;
	cbp[3] = l;
	cbp += 4;
	cbc -= 4;
	if(cbc <= 0)
		cflush();
}

void
lputl(long l)
{

	cbp[3] = l>>24;
	cbp[2] = l>>16;
	cbp[1] = l>>8;
	cbp[0] = l;
	cbp += 4;
	cbc -= 4;
	if(cbc <= 0)
		cflush();
}

void
llput(vlong v)
{
	lput(v>>32);
	lput(v);
}

void
llputl(vlong v)
{
	lputl(v);
	lputl(v>>32);
}

void
asmsym(void)
{
	Prog *p;
	Auto *a;
	Sym *s;
	int h;

	s = lookup("etext", 0);
	if(s->type == STEXT)
		putsymb(s->name, 'T', s->value, s->version);

	for(h=0; h<NHASH; h++)
		for(s=hash[h]; s!=S; s=s->link)
			switch(s->type) {
			case SCONST:
				putsymb(s->name, 'D', s->value, s->version);
				continue;

			case SDATA:
				putsymb(s->name, 'D', s->value+INITDAT, s->version);
				continue;

			case SBSS:
				putsymb(s->name, 'B', s->value+INITDAT, s->version);
				continue;

			case SSTRING:
				putsymb(s->name, 'T', s->value, s->version);
				continue;

			case SFILE:
				putsymb(s->name, 'f', s->value, s->version);
				continue;
			}

	for(p=textp; p!=P; p=p->cond) {
		s = p->from.sym;
		if(s->type != STEXT && s->type != SLEAF)
			continue;

		/* filenames first */
		for(a=p->to.autom; a; a=a->link)
			if(a->type == D_FILE)
				putsymb(a->asym->name, 'z', a->aoffset, 0);
			else
			if(a->type == D_FILE1)
				putsymb(a->asym->name, 'Z', a->aoffset, 0);

		if(s->type == STEXT)
			putsymb(s->name, 'T', s->value, s->version);
		else
			putsymb(s->name, 'L', s->value, s->version);

		/* frame, auto and param after */
		putsymb(".frame", 'm', p->to.offset+PCSZ, 0);
		for(a=p->to.autom; a; a=a->link)
			if(a->type == D_AUTO)
				putsymb(a->asym->name, 'a', -a->aoffset, 0);
			else
			if(a->type == D_PARAM)
				putsymb(a->asym->name, 'p', a->aoffset, 0);
	}
	if(debug['v'] || debug['n'])
		Bprint(&bso, "symsize = %lud\n", symsize);
	Bflush(&bso);
}

void
putsymb(char *s, int t, vlong v, int ver)
{
	int i, f, l;

	if(t == 'f')
		s++;
	l = 4;
	switch(HEADTYPE){
	default:
		break;
	case 2:
		lput(v>>32);
		l = 8;
		break;
	}
	lput(v);
	if(ver)
		t += 'a' - 'A';
	cput(t+0x80);			/* 0x80 is variable length */

	if(t == 'Z' || t == 'z') {
		cput(s[0]);
		for(i=1; s[i] != 0 || s[i+1] != 0; i += 2) {
			cput(s[i]);
			cput(s[i+1]);
		}
		cput(0);
		cput(0);
		i++;
	}
	else {
		for(i=0; s[i]; i++)
			cput(s[i]);
		cput(0);
	}
	symsize += l + 1 + i + 1;

	if(debug['n']) {
		if(t == 'z' || t == 'Z') {
			Bprint(&bso, "%c %.8llux ", t, v);
			for(i=1; s[i] != 0 || s[i+1] != 0; i+=2) {
				f = ((s[i]&0xff) << 8) | (s[i+1]&0xff);
				Bprint(&bso, "/%x", f);
			}
			Bprint(&bso, "\n");
			return;
		}
		if(ver)
			Bprint(&bso, "%c %.8llux %s<%d>\n", t, v, s, ver);
		else
			Bprint(&bso, "%c %.8llux %s\n", t, v, s);
	}
}

#define	MINLC	4
void
asmlc(void)
{
	vlong oldpc;
	Prog *p;
	long v, s, oldlc;

	oldpc = INITTEXT;
	oldlc = 0;
	for(p = firstp; p != P; p = p->link) {
		if(p->line == oldlc || p->as == ATEXT || p->as == ANOP) {
			if(p->as == ATEXT)
				curtext = p;
			if(debug['V'])
				Bprint(&bso, "%6llux %P\n",
					p->pc, p);
			continue;
		}
		if(debug['V'])
			Bprint(&bso, "\t\t%6ld", lcsize);
		v = (p->pc - oldpc) / MINLC;
		while(v) {
			s = 127;
			if(v < 127)
				s = v;
			cput(s+128);	/* 129-255 +pc */
			if(debug['V'])
				Bprint(&bso, " pc+%ld*%d(%ld)", s, MINLC, s+128);
			v -= s;
			lcsize++;
		}
		s = p->line - oldlc;
		oldlc = p->line;
		oldpc = p->pc + MINLC;
		if(s > 64 || s < -64) {
			cput(0);	/* 0 vv +lc */
			cput(s>>24);
			cput(s>>16);
			cput(s>>8);
			cput(s);
			if(debug['V']) {
				if(s > 0)
					Bprint(&bso, " lc+%ld(%d,%ld)\n",
						s, 0, s);
				else
					Bprint(&bso, " lc%ld(%d,%ld)\n",
						s, 0, s);
				Bprint(&bso, "%6llux %P\n",
					p->pc, p);
			}
			lcsize += 5;
			continue;
		}
		if(s > 0) {
			cput(0+s);	/* 1-64 +lc */
			if(debug['V']) {
				Bprint(&bso, " lc+%ld(%ld)\n", s, 0+s);
				Bprint(&bso, "%6llux %P\n",
					p->pc, p);
			}
		} else {
			cput(64-s);	/* 65-128 -lc */
			if(debug['V']) {
				Bprint(&bso, " lc%ld(%ld)\n", s, 64-s);
				Bprint(&bso, "%6llux %P\n",
					p->pc, p);
			}
		}
		lcsize++;
	}
	while(lcsize & 1) {
		s = 129;
		cput(s);
		lcsize++;
	}
	if(debug['v'] || debug['V'])
		Bprint(&bso, "lcsize = %ld\n", lcsize);
	Bflush(&bso);
}

void
datfill(uchar *sbuf, long soff, uchar *dbuf, long doff)
{
	Sym *v;
	Prog *p;
	uchar *cast, *b, *nuxi;
	long a, l, fl, j;
	vlong d;
	int i, c;

	for(p = datap; p != P; p = p->link) {
		curp = p;
		a = p->from.sym->value + p->from.offset;
		l = a;
		if(p->from.sym->type == SSTRING){
			b = sbuf;
			l -= soff;
		}else{
			b = dbuf;
			l -= doff;
		}

		c = p->reg;
		i = 0;
		if(p->as != AINIT && p->as != ADYNT && !p->from.sym->dupok) {
			for(j=l+(c-i)-1; j>=l; j--)
				if(b[j]) {
					print("%P\n", p);
					diag("multiple initialization");
					break;
				}
		}
		cast = nuxi = nil;
		switch(p->to.type) {
		default:
			diag("unknown mode in initialization%P", p);
			break;

		case D_SCONST:
			while(i < c)
				b[l++] = p->to.sval[i++];
			break;

		case D_FCONST:
			switch(c) {
			default:
			case 4:
				fl = ieeedtof(p->to.ieee);
				cast = (uchar*)&fl;
				nuxi = fnuxi4;
				break;
			case 8:
				cast = (uchar*)p->to.ieee;
				nuxi = fnuxi8;
				break;
			}
			break;

		case D_CONST:
			d = p->to.offset;
			v = p->to.sym;
			if(v) {
				switch(v->type) {
				case SUNDEF:
					ckoff(v, d);
				case STEXT:
				case SLEAF:
				case SSTRING:
					d += p->to.sym->value;
					break;
				case SDATA:
				case SBSS:
					d += p->to.sym->value + INITDAT;
				}
				if(dlm)
					dynreloc(v, a+INITDAT, 1);
			}
			cast = (uchar*)&d;
			nuxi = inuxi[c];
			break;
		}
		if(cast)
			while(i < c)
				b[l++] = cast[nuxi[i++]];
	}
}

int
chipfloat(Ieee *e)
{
	int n, Bbbbbbbbb;

	if(e->l != 0 || (e->h & 0xffffU) != 0)
		return -1;
	n = e->h >> 16;
	Bbbbbbbbb = (n>>6) & 0x1ff;
	if(Bbbbbbbbb != 0x100 && Bbbbbbbbb != 0xff)
		return -1;
	n = (n & 0x8000) >> 8 | (n & 0x7f);
	return n;
}
