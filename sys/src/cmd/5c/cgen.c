#include "gc.h"

static void genasop(int, Node*, Node*, Node*);

void
cgen(Node *n, Node *nn)
{
	cgenrel(n, nn, 0);
}

void
cgenrel(Node *n, Node *nn, int inrel)
{
	Node *l, *r;
	Prog *p1;
	Node nod, nod1, nod2, nod3, nod4;
	int o, t;
	long v, curs;

	if(debug['g']) {
		prtree(nn, "cgen lhs");
		prtree(n, "cgen");
	}
	if(n == Z || n->type == T)
		return;
	if(typesuv[n->type->etype]) {
		sugen(n, nn, n->type->width);
		return;
	}
	l = n->left;
	r = n->right;
	o = n->op;
	if(n->addable >= INDEXED) {
		if(nn == Z) {
			switch(o) {
			default:
				nullwarn(Z, Z);
				break;
			case OINDEX:
				nullwarn(l, r);
				break;
			}
			return;
		}
		gmove(n, nn);
		return;
	}
	curs = cursafe;

	if(n->complex >= FNX)
	if(l->complex >= FNX)
	if(r != Z && r->complex >= FNX)
	switch(o) {
	default:
		regret(&nod, r);
		cgen(r, &nod);

		regsalloc(&nod1, r);
		gmove(&nod, &nod1);

		regfree(&nod);
		nod = *n;
		nod.right = &nod1;
		cgen(&nod, nn);
		return;

	case OFUNC:
	case OCOMMA:
	case OANDAND:
	case OOROR:
	case OCOND:
	case ODOT:
		break;
	}

	switch(o) {
	default:
		diag(n, "unknown op in cgen: %O", o);
		break;

	case OAS:
		if(l->op == OBIT)
			goto bitas;
		if(l->addable >= INDEXED && l->complex < FNX) {
			if(nn != Z || r->addable < INDEXED) {
				if(r->complex >= FNX && nn == Z)
					regret(&nod, r);
				else
					regalloc(&nod, r, nn);
				cgen(r, &nod);
				gmove(&nod, l);
				if(nn != Z)
					gmove(&nod, nn);
				regfree(&nod);
			} else
				gmove(r, l);
			break;
		}
		if(l->complex >= r->complex) {
			reglcgen(&nod1, l, Z);
			if(r->addable >= INDEXED) {
				gmove(r, &nod1);
				if(nn != Z)
					gmove(r, nn);
				regfree(&nod1);
				break;
			}
			regalloc(&nod, r, nn);
			cgen(r, &nod);
		} else {
			regalloc(&nod, r, nn);
			cgen(r, &nod);
			reglcgen(&nod1, l, Z);
		}
		gmove(&nod, &nod1);
		if(nn != Z)
			gmove(&nod, nn);
		regfree(&nod);
		regfree(&nod1);
		break;

	bitas:
		n = l->left;
		regalloc(&nod, r, nn);
		if(l->complex >= r->complex) {
			reglcgen(&nod1, n, Z);
			cgen(r, &nod);
		} else {
			cgen(r, &nod);
			reglcgen(&nod1, n, Z);
		}
		regalloc(&nod2, n, Z);
		gopcode(OAS, &nod1, Z, &nod2);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;

	case OBIT:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		bitload(n, &nod, Z, Z, nn);
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		break;

	case ODIV:
	case OMOD:
		if(nn != Z)
		if((t = vlog(r)) >= 0) {
			/* signed div/mod by constant power of 2 */
			cgen(l, nn);
			gopcode(OGE, nodconst(0), nn, Z);
			p1 = p;
			if(o == ODIV) {
				gopcode(OADD, nodconst((1<<t)-1), Z, nn);
				patch(p1, pc);
				gopcode(OASHR, nodconst(t), Z, nn);
			} else {
				gopcode(OSUB, nn, nodconst(0), nn);
				gopcode(OAND, nodconst((1<<t)-1), Z, nn);
				gopcode(OSUB, nn, nodconst(0), nn);
				gbranch(OGOTO);
				patch(p1, pc);
				p1 = p;
				gopcode(OAND, nodconst((1<<t)-1), Z, nn);
				patch(p1, pc);
			}
			break;
		}
		goto muldiv;

	case OSUB:
		if(nn != Z)
		if(l->op == OCONST)
		if(!typefd[n->type->etype]) {
			cgen(r, nn);
			gopcode(o, Z, l, nn);
			break;
		}
	case OADD:
	case OAND:
	case OOR:
	case OXOR:
	case OROL:
	case OLSHR:
	case OASHL:
	case OASHR:
		/*
		 * immediate operands
		 */
		if(nn != Z)
		if(r->op == OCONST)
		if(!typefd[n->type->etype]) {
			cgen(l, nn);
			if(r->vconst == 0)
			if(o != OAND)
				break;
			if(nn != Z)
				gopcode(o, r, Z, nn);
			break;
		}

	case OLMUL:
	case OLDIV:
	case OLMOD:
	case OMUL:
	muldiv:
		if(nn == Z) {
			nullwarn(l, r);
			break;
		}
		if(o == OMUL || o == OLMUL) {
			if(mulcon(n, nn))
				break;
		}
		if(l->complex >= r->complex) {
			regalloc(&nod, l, nn);
			cgen(l, &nod);
			regalloc(&nod1, r, Z);
			cgen(r, &nod1);
			gopcode(o, &nod1, Z, &nod);
		} else {
			regalloc(&nod, r, nn);
			cgen(r, &nod);
			regalloc(&nod1, l, Z);
			cgen(l, &nod1);
			gopcode(o, &nod, &nod1, &nod);
		}
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		regfree(&nod1);
		break;

	case OASLSHR:
	case OASASHL:
	case OASASHR:
	case OASAND:
	case OASADD:
	case OASSUB:
	case OASXOR:
	case OASOR:
		if(l->op == OBIT)
			goto asbitop;
		if(r->op == OCONST)
		if(!typefd[r->type->etype])
		if(!typefd[n->type->etype]) {
			if(l->addable < INDEXED)
				reglcgen(&nod2, l, Z);
			else
				nod2 = *l;
			regalloc(&nod, l, nn);
			gopcode(OAS, &nod2, Z, &nod);
			gopcode(o, r, Z, &nod);
			gopcode(OAS, &nod, Z, &nod2);
			if(nn != Z)
				gmove(&nod, nn);
			regfree(&nod);
			if(l->addable < INDEXED)
				regfree(&nod2);
			break;
		}
		genasop(o, l, r, nn);
		break;

	case OASLMUL:
	case OASLDIV:
	case OASLMOD:
	case OASMUL:
	case OASDIV:
	case OASMOD:
		if(l->op == OBIT)
			goto asbitop;
		genasop(o, l, r, nn);
		break;

	asbitop:
		regalloc(&nod4, n, nn);
		if(l->complex >= r->complex) {
			bitload(l, &nod, &nod1, &nod2, &nod4);
			regalloc(&nod3, r, Z);
			cgen(r, &nod3);
		} else {
			regalloc(&nod3, r, Z);
			cgen(r, &nod3);
			bitload(l, &nod, &nod1, &nod2, &nod4);
		}
		gmove(&nod, &nod4);
		gopcode(o, &nod3, Z, &nod4);
		regfree(&nod3);
		gmove(&nod4, &nod);
		regfree(&nod4);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;

	case OADDR:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		lcgen(l, nn);
		break;

	case OFUNC:
		l = uncomma(l);
		if(l->complex >= FNX) {
			if(l->op != OIND)
				diag(n, "bad function call");

			regret(&nod, l->left);
			cgen(l->left, &nod);
			regsalloc(&nod1, l->left);
			gopcode(OAS, &nod, Z, &nod1);
			regfree(&nod);

			nod = *n;
			nod.left = &nod2;
			nod2 = *l;
			nod2.left = &nod1;
			nod2.complex = 1;
			cgen(&nod, nn);

			return;
		}
		if(REGARG >= 0)
			o = reg[REGARG];
		gargs(r, &nod, &nod1);
		if(l->addable < INDEXED) {
			reglcgen(&nod, l, Z);
			gopcode(OFUNC, Z, Z, &nod);
			regfree(&nod);
		} else
			gopcode(OFUNC, Z, Z, l);
		if(REGARG >= 0)
			if(o != reg[REGARG])
				reg[REGARG]--;
		if(nn != Z) {
			regret(&nod, n);
			gopcode(OAS, &nod, Z, nn);
			regfree(&nod);
		}
		break;

	case OIND:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		regialloc(&nod, n, nn);
		r = l;
		while(r->op == OADD)
			r = r->right;
		if(sconst(r) && (v = r->vconst+nod.xoffset) > -4096 && v < 4096) {
			v = r->vconst;
			r->vconst = 0;
			cgen(l, &nod);
			nod.xoffset += v;
			r->vconst = v;
		} else
			cgen(l, &nod);
		regind(&nod, n);
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		break;

	case OEQ:
	case ONE:
	case OLE:
	case OLT:
	case OGE:
	case OGT:
	case OLO:
	case OLS:
	case OHI:
	case OHS:
		if(nn == Z) {
			nullwarn(l, r);
			break;
		}
		boolgen(n, 1, nn);
		break;

	case OANDAND:
	case OOROR:
		boolgen(n, 1, nn);
		if(nn == Z)
			patch(p, pc);
		break;

	case ONOT:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		boolgen(n, 1, nn);
		break;

	case OCOMMA:
		cgen(l, Z);
		cgen(r, nn);
		break;

	case OCAST:
		if(nn == Z) {
			cgen(l, Z);
			break;
		}
		/*
		 * convert from types l->n->nn
		 */
		if(nocast(l->type, n->type)) {
			if(nocast(n->type, nn->type)) {
				cgen(l, nn);
				break;
			}
		}
		if(typev[l->type->etype]) {
			cgen64(n, nn);
			break;
		}
		regalloc(&nod, l, nn);
		cgen(l, &nod);
		regalloc(&nod1, n, &nod);
		if(inrel)
			gmover(&nod, &nod1);
		else
			gopcode(OAS, &nod, Z, &nod1);
		gopcode(OAS, &nod1, Z, nn);
		regfree(&nod1);
		regfree(&nod);
		break;

	case ODOT:
		sugen(l, nodrat, l->type->width);
		if(nn != Z) {
			warn(n, "non-interruptable temporary");
			nod = *nodrat;
			if(!r || r->op != OCONST) {
				diag(n, "DOT and no offset");
				break;
			}
			nod.xoffset += (long)r->vconst;
			nod.type = n->type;
			cgen(&nod, nn);
		}
		break;

	case OCOND:
		bcgen(l, 1);
		p1 = p;
		cgen(r->left, nn);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		cgen(r->right, nn);
		patch(p1, pc);
		break;

	case OPOSTINC:
	case OPOSTDEC:
		v = 1;
		if(l->type->etype == TIND)
			v = l->type->link->width;
		if(o == OPOSTDEC)
			v = -v;
		if(l->op == OBIT)
			goto bitinc;
		if(nn == Z)
			goto pre;

		if(l->addable < INDEXED)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;

		regalloc(&nod, l, nn);
		gopcode(OAS, &nod2, Z, &nod);
		if(nn != Z)
			gmove(&nod, nn);
		regalloc(&nod1, l, Z);
		if(typefd[l->type->etype]) {
			regalloc(&nod3, l, Z);
			if(v < 0) {
				gopcode(OAS, nodfconst(-v), Z, &nod3);
				gopcode(OSUB, &nod3, &nod, &nod1);
			} else {
				gopcode(OAS, nodfconst(v), Z, &nod3);
				gopcode(OADD, &nod3, &nod, &nod1);
			}
			regfree(&nod3);
		} else
			gopcode(OADD, nodconst(v), &nod, &nod1);
		gopcode(OAS, &nod1, Z, &nod2);

		regfree(&nod);
		regfree(&nod1);
		if(l->addable < INDEXED)
			regfree(&nod2);
		break;

	case OPREINC:
	case OPREDEC:
		v = 1;
		if(l->type->etype == TIND)
			v = l->type->link->width;
		if(o == OPREDEC)
			v = -v;
		if(l->op == OBIT)
			goto bitinc;

	pre:
		if(l->addable < INDEXED)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;

		regalloc(&nod, l, nn);
		gopcode(OAS, &nod2, Z, &nod);
		if(typefd[l->type->etype]) {
			regalloc(&nod3, l, Z);
			if(v < 0) {
				gopcode(OAS, nodfconst(-v), Z, &nod3);
				gopcode(OSUB, &nod3, Z, &nod);
			} else {
				gopcode(OAS, nodfconst(v), Z, &nod3);
				gopcode(OADD, &nod3, Z, &nod);
			}
			regfree(&nod3);
		} else
			gopcode(OADD, nodconst(v), Z, &nod);
		gopcode(OAS, &nod, Z, &nod2);
		if(nn != Z){
			gmove(&nod, nn);
			if(l->op == ONAME)	/* in x=++i, emit USED(i) */
				gins(ANOP, l, Z);
		}
		regfree(&nod);
		if(l->addable < INDEXED)
			regfree(&nod2);
		break;

	bitinc:
		if(nn != Z && (o == OPOSTINC || o == OPOSTDEC)) {
			bitload(l, &nod, &nod1, &nod2, Z);
			gopcode(OAS, &nod, Z, nn);
			gopcode(OADD, nodconst(v), Z, &nod);
			bitstore(l, &nod, &nod1, &nod2, Z);
			break;
		}
		bitload(l, &nod, &nod1, &nod2, nn);
		gopcode(OADD, nodconst(v), Z, &nod);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;
	}
	cursafe = curs;
	return;
}

static void
genasop(int o, Node *l, Node *r, Node *nn)
{
	Node nod, nod1, nod2;
	int hardleft;

	hardleft = l->addable < INDEXED || l->complex >= FNX;
	if(l->complex >= r->complex) {
		if(hardleft)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;
		regalloc(&nod1, r, Z);
		cgen(r, &nod1);
	} else {
		regalloc(&nod1, r, Z);
		cgen(r, &nod1);
		if(hardleft)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;
	}
	if(nod1.type == nod2.type || !typefd[nod1.type->etype])
		regalloc(&nod, &nod2, nn);
	else
		regalloc(&nod, &nod1, Z);
	gmove(&nod2, &nod);
	gopcode(o, &nod1, Z, &nod);
	gmove(&nod, &nod2);
	if(nn != Z)
		gmove(&nod2, nn);
	regfree(&nod);
	regfree(&nod1);
	if(hardleft)
		regfree(&nod2);
}

void
reglcgen(Node *t, Node *n, Node *nn)
{
	Node *r;
	long v;

	regialloc(t, n, nn);
	if(n->op == OIND) {
		r = n->left;
		while(r->op == OADD)
			r = r->right;
		if(sconst(r) && (v = r->vconst+t->xoffset) > -4096 && v < 4096) {
			v = r->vconst;
			r->vconst = 0;
			lcgen(n, t);
			t->xoffset += v;
			r->vconst = v;
			regind(t, n);
			return;
		}
	} else if(n->op == OINDREG) {
		if((v = n->xoffset) > -4096 && v < 4096) {
			n->op = OREGISTER;
			cgen(n, t);
			t->xoffset += v;
			n->op = OINDREG;
			regind(t, n);
			return;
		}
	}
	lcgen(n, t);
	regind(t, n);
}

void
reglpcgen(Node *n, Node *nn, int f)
{
	Type *t;

	t = nn->type;
	nn->type = types[TLONG];
	if(f)
		reglcgen(n, nn, Z);
	else {
		regialloc(n, nn, Z);
		lcgen(nn, n);
		regind(n, nn);
	}
	nn->type = t;
}

void
lcgen(Node *n, Node *nn)
{
	Prog *p1;
	Node nod;

	if(debug['g']) {
		prtree(nn, "lcgen lhs");
		prtree(n, "lcgen");
	}
	if(n == Z || n->type == T)
		return;
	if(nn == Z) {
		nn = &nod;
		regalloc(&nod, n, Z);
	}
	switch(n->op) {
	default:
		if(n->addable < INDEXED) {
			diag(n, "unknown op in lcgen: %O", n->op);
			break;
		}
		nod = *n;
		nod.op = OADDR;
		nod.left = n;
		nod.right = Z;
		nod.type = types[TIND];
		gopcode(OAS, &nod, Z, nn);
		break;

	case OCOMMA:
		cgen(n->left, n->left);
		lcgen(n->right, nn);
		break;

	case OIND:
		cgen(n->left, nn);
		break;

	case OCOND:
		bcgen(n->left, 1);
		p1 = p;
		lcgen(n->right->left, nn);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		lcgen(n->right->right, nn);
		patch(p1, pc);
		break;
	}
}

void
bcgen(Node *n, int true)
{

	if(n->type == T)
		gbranch(OGOTO);
	else
		boolgen(n, true, Z);
}

void
boolgen(Node *n, int true, Node *nn)
{
	int o;
	Prog *p1, *p2;
	Node *l, *r, nod, nod1;
	long curs;

	if(debug['g']) {
		prtree(nn, "boolgen lhs");
		prtree(n, "boolgen");
	}
	curs = cursafe;
	l = n->left;
	r = n->right;
	switch(n->op) {

	default:
		regalloc(&nod, n, nn);
		cgen(n, &nod);
		o = ONE;
		if(true)
			o = comrel[relindex(o)];
		if(typefd[n->type->etype]){
			regalloc(&nod1, n, Z);
			gmove(nodfconst(0.0), &nod1);
			gopcode(true ? o | BTRUE : o, &nod1, &nod, Z);
			regfree(&nod1);
		}else
			gopcode(o, nodconst(0), &nod, Z);
		regfree(&nod);
		goto com;

	case OCONST:
		o = vconst(n);
		if(!true)
			o = !o;
		gbranch(OGOTO);
		if(o) {
			p1 = p;
			gbranch(OGOTO);
			patch(p1, pc);
		}
		goto com;

	case OCOMMA:
		cgen(l, Z);
		boolgen(r, true, nn);
		break;

	case ONOT:
		boolgen(l, !true, nn);
		break;

	case OCOND:
		bcgen(l, 1);
		p1 = p;
		bcgen(r->left, true);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		bcgen(r->right, !true);
		patch(p2, pc);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		patch(p2, pc);
		goto com;

	case OANDAND:
		if(!true)
			goto caseor;

	caseand:
		bcgen(l, true);
		p1 = p;
		bcgen(r, !true);
		p2 = p;
		patch(p1, pc);
		gbranch(OGOTO);
		patch(p2, pc);
		goto com;

	case OOROR:
		if(!true)
			goto caseand;

	caseor:
		bcgen(l, !true);
		p1 = p;
		bcgen(r, !true);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		patch(p2, pc);
		goto com;

	case OEQ:
	case ONE:
	case OLE:
	case OLT:
	case OGE:
	case OGT:
	case OHI:
	case OHS:
	case OLO:
	case OLS:
		o = n->op;
		if(true)
			o = comrel[relindex(o)];
		if(l->complex >= FNX && r->complex >= FNX) {
			regret(&nod, r);
			cgenrel(r, &nod, 1);
			regsalloc(&nod1, r);
			gopcode(OAS, &nod, Z, &nod1);
			regfree(&nod);
			nod = *n;
			nod.right = &nod1;
			boolgen(&nod, true, nn);
			break;
		}
		if(sconst(l)) {
			regalloc(&nod, r, nn);
			cgenrel(r, &nod, 1);
			o = invrel[relindex(o)];
			gopcode(true ? o | BTRUE : o, l, &nod, Z);
			regfree(&nod);
			goto com;
		}
		if(sconst(r)) {
			regalloc(&nod, l, nn);
			cgenrel(l, &nod, 1);
			gopcode(true ? o | BTRUE : o, r, &nod, Z);
			regfree(&nod);
			goto com;
		}
		if(l->complex >= r->complex) {
			regalloc(&nod1, l, nn);
			cgenrel(l, &nod1, 1);
			regalloc(&nod, r, Z);
			cgenrel(r, &nod, 1);
		} else {
			regalloc(&nod, r, nn);
			cgenrel(r, &nod, 1);
			regalloc(&nod1, l, Z);
			cgenrel(l, &nod1, 1);
		}
		gopcode(true ? o | BTRUE : o, &nod, &nod1, Z);
		regfree(&nod);
		regfree(&nod1);

	com:
		if(nn != Z) {
			p1 = p;
			gopcode(OAS, nodconst(1), Z, nn);
			gbranch(OGOTO);
			p2 = p;
			patch(p1, pc);
			gopcode(OAS, nodconst(0), Z, nn);
			patch(p2, pc);
		}
		break;
	}
	cursafe = curs;
}

static void
freepair(Node *n)
{
	n->left->xoffset = reg[n->left->reg];
	reg[n->left->reg] = 0;
	n->right->xoffset = reg[n->right->reg];
	reg[n->right->reg] = 0;
}
static void
unfreepair(Node *n)
{
	reg[n->left->reg] = n->left->xoffset;
	n->left->xoffset = 0;
	reg[n->right->reg] = n->right->xoffset;
	n->right->xoffset = 0;
}

int
cgen64(Node *n, Node *nn)
{
	Node nod0, nod1, nod2, nod3, *l, *r;
	int o, a, ml, mr, nnsaved;
	long curs;

	if(!machcap(n))
		return 0;

	if(debug['g']){
		prtree(nn, "cgen64 nn");
		prtree(n, "cgen64 n");
	}

	if(nn != Z && nn->op != OREGPAIR && typev[n->type->etype]){
		if(nn->complex > n->complex){
			reglpcgen(&nod0, nn, 1);
			nod0.type = n->type;
			regalloc(&nod1, n, Z);
			cgen(n, &nod1);
			cgen(&nod1, &nod0);
			regfree(&nod0);
			regfree(&nod1);
		} else {
			regalloc(&nod1, n, Z);
			cgen(n, &nod1);
			cgen(&nod1, nn);
			regfree(&nod1);
		}
		return 1;
	}

	nnsaved = 0;
	curs = cursafe;
	o = n->op;
	l = n->left;
	r = n->right;
	switch(o){
	default:
		return 0;

	case OCAST:
		if(nn == Z){
			cgen(l, Z);
			goto Out;
		}
		if(typeilp[n->type->etype] && typev[l->type->etype]){
			if(l->op == ONAME || l->op == OINDREG)
				nod0 = *l;
			else if((l->op == OLSHR || l->op == OASHR)
			&& (l->right->op == OCONST && l->right->vconst == 32)
			&& (l->left->op == ONAME || l->left->op == OINDREG)){
				nod0 = *l->left;
				nod0.xoffset += SZ_LONG;
			} else {
				if(nn->complex > l->complex){
					reglpcgen(&nod0, nn, 1);
					regalloc(&nod1, l, Z);
					cgen(l, &nod1);
					cgen(nod1.left, &nod0);
					regfree(&nod0);
					regfree(&nod1);
				} else {
					regalloc(&nod0, l, Z);
					cgen(l, &nod0);
					cgen(nod0.left, nn);
					regfree(&nod0);
				}
				goto Out;
			}
			nod0.type = n->type;
			cgen(&nod0, nn);
			goto Out;
		}
		if(typev[n->type->etype] && typeilp[l->type->etype]){
			regalloc(&nod1, l, nn->left);
			a = reg[nn->right->reg];
			reg[nn->right->reg] = 0;
			cgen(l, &nod1);
			reg[nn->right->reg] = a;
			if(typeu[l->type->etype])
				gmove(nodconst(0), nn->right);
			else
				gopcode(OASHR, nodconst(31), &nod1, nn->right);
			regfree(&nod1);
			goto Out;
		}
		return 0;

	case OASASHL:	o = OASHL;	goto asop;
	case OASASHR:	o = OASHR;	goto asop;
	case OASLSHR:	o = OLSHR;	goto asop;

	case OASADD:	o = OADD;	goto asop;
	case OASSUB:	o = OSUB;	goto asop;
	case OASAND:	o = OAND;	goto asop;
	case OASXOR:	o = OXOR;	goto asop;
	case OASOR:	o = OOR;	goto asop;
	asop:	
		nod0 = *n;
		nod0.op = o;
		nod0.left = &nod1;
		nod1 = *l;
		if(side(l)){
			nod1.op = OIND;
			nod1.left = &nod3;
			nod1.right = Z;
			nod1.complex = 1;

			nod1.type = typ(TIND, l->type);
			regsalloc(&nod3, &nod1);
			nod1.type = l->type;

			regalloc(&nod2, &nod3, nn != Z ? nn->left : Z);
			lcgen(l, &nod2);
			gmove(&nod2, &nod3);
			regfree(&nod2);
		}
		if(nn == Z)
			cgen(&nod0, &nod1);
		else {
			cgen(&nod0, nn);
			cgen(nn, &nod1);
		}
		goto Out;

	case OASHL:
		if(nn == Z){
			nullwarn(l, Z);
			goto Out;
		}
		cgen(l, nn);
		assert(r->op == OCONST);
		a = r->vconst & 63;
		if(a == 0)
			goto Out;
		if(a == 1){
			gins(AADD, nn->left, nn->left);
			p->scond |= C_SBIT;
			gins(AADC, nn->right, nn->right);
			goto Out;
		}
		if(a < 32){
			gopcode(OASHL, nodconst(a), Z, nn->right);
			gopcode(OOR, nn->left, Z, nn->right);
			p->from.offset = nn->left->reg | (32-a)<<7 | 1<<5;
			p->from.reg = NREG;
			p->from.type = D_SHIFT;
			gopcode(OASHL, nodconst(a), Z, nn->left);
			goto Out;
		}
		if(a == 32)
			gmove(nn->left, nn->right);
		else
			gopcode(o, nodconst(a-32), nn->left, nn->right);
		gmove(nodconst(0), nn->left);
		goto Out;

	case OLSHR:
	case OASHR:
		if(nn == Z){
			nullwarn(l, Z);
			goto Out;
		}
		cgen(l, nn);
		assert(r->op == OCONST);
		a = r->vconst & 63;
		if(a == 0)
			goto Out;
		if(a < 32){
			gopcode(OLSHR, nodconst(a), Z, nn->left);
			gopcode(OOR, nn->right, Z, nn->left);
			p->from.offset = nn->right->reg | (32-a)<<7;
			p->from.reg = NREG;
			p->from.type = D_SHIFT;
			gopcode(o, nodconst(a), Z, nn->right);
			goto Out;
		}
		if(a == 32)
			gmove(nn->right, nn->left);
		else
			gopcode(o, nodconst(a-32), nn->right, nn->left);
		if(o == OASHR)
			gopcode(o, nodconst(31), Z, nn->right);
		else
			gmove(nodconst(0), nn->right);
		goto Out;

	case OADD:
	case OSUB:
	case OAND:
	case OXOR:
	case OOR:
		if(nn == Z){
			nullwarn(l, r);
			goto Out;
		}
		ml = o == OADD && l->op == OLMUL && machcap(l);
		mr = o == OADD && r->op == OLMUL && machcap(r);
		if(ml && !mr){
			cgen(r, nn);
			n = l;
		} else if(mr && !ml){
			cgen(l, nn);
			n = r;
		} else {
			if(r->complex > l->complex){
				cgen(r, nn);
				n = l;
			} else {
				cgen(l, nn);
				n = r;
			}
		}
		if(n->complex >= FNX){
			regsalloc(&nod0, nn);
			gmove(nn, &nod0);
			nnsaved = 1;
		}
		if(ml || mr){
			l = n->left;
			r = n->right;
			a = AMULALU;
			break;
		}
		regalloc(&nod1, n, Z);
		if(nnsaved) freepair(nn);
		cgen(n, &nod1);
		if(nnsaved){
			unfreepair(nn);
			gmove(&nod0, nn);
		}

		switch(o){
		case OADD:
			gins(AADD, nod1.left, nn->left);
			p->scond |= C_SBIT;
			gins(AADC, nod1.right, nn->right);
			break;
		case OSUB:
			if(n == r){
				gins(ASUB, nod1.left, nn->left);
				p->scond |= C_SBIT;
				gins(ASBC, nod1.right, nn->right);
			} else {
				gins(ASUB, nn->left, nn->left);
				p->reg = nod1.left->reg;
				p->scond |= C_SBIT;
				gins(ASBC, nn->right, nn->right);
				p->reg = nod1.right->reg;
			}
			break;
		default:
			gopcode(o, nod1.left, Z, nn->left);
			gopcode(o, nod1.right, Z, nn->right);
		}
		regfree(&nod1);
		goto Out;

	case OMUL:
		a = AMULL;
		break;

	case OLMUL:
		a = AMULLU;
		break;
	}

	if(nn == Z){
		nullwarn(l, r);
		goto Out;
	}
	if(r->complex > l->complex) {
		l = r;
		r = n->left;
	}

	regalloc(&nod1, l, Z);
	if(nnsaved) freepair(nn);
	cgen(l, &nod1);
	if(nnsaved) unfreepair(nn);
	if(r->complex >= FNX) {
		regsalloc(&nod3, &nod1);
		gmove(&nod1, &nod3);
		if(nnsaved) freepair(nn);
		cgen(r, &nod1);
		if(nnsaved) unfreepair(nn);
		regalloc(&nod2, &nod3, Z);
		gmove(&nod3, &nod2);
	} else {
		regalloc(&nod2, r, Z);
		if(nnsaved) freepair(nn);
		cgen(r, &nod2);
		if(nnsaved) unfreepair(nn);
	}
	if(nnsaved)
		gmove(&nod0, nn);

	gins(a, &nod1, nn->right);
	p->reg = nod2.reg;
	p->to.type = D_REGREG;
	p->to.offset = nn->left->reg;

	regfree(&nod1);
	regfree(&nod2);

Out:
	cursafe = curs;
	return 1;
}

void
sugen(Node *n, Node *nn, long w)
{
	Prog *p1;
	Node nod0, nod1, nod2, nod3, nod4, *l, *r, *d;
	Type *t;
	long pc1;
	int i, m, c;

	if(n == Z || n->type == T)
		return;
	if(debug['g']) {
		prtree(nn, "sugen lhs");
		prtree(n, "sugen");
	}
	if(nn == nodrat)
		if(w > nrathole)
			nrathole = w;
	switch(n->op) {
	case OIND:
		if(nn == Z) {
			nullwarn(n->left, Z);
			break;
		}

	default:
		goto copy;

	case OCONST:
		if(n->type && typev[n->type->etype]) {
			if(nn == Z) {
				nullwarn(n->left, Z);
				break;
			}

			if(nn->op == OREGPAIR){
				gopcode(OAS, nod32const(n->vconst), Z, nn->left);
				gopcode(OAS, nod32const(n->vconst>>32), Z, nn->right);
				break;
			}

			reglpcgen(&nod1, nn, 1);
			if(align(0, types[TCHAR], Aarg1))	/* isbigendian */
				gopcode(OAS, nod32const(n->vconst>>32), Z, &nod1);
			else
				gopcode(OAS, nod32const(n->vconst), Z, &nod1);
			nod1.xoffset += SZ_LONG;
			if(align(0, types[TCHAR], Aarg1))	/* isbigendian */
				gopcode(OAS, nod32const(n->vconst), Z, &nod1);
			else
				gopcode(OAS, nod32const(n->vconst>>32), Z, &nod1);
			regfree(&nod1);
			break;
		}
		goto copy;

	case ODOT:
		l = n->left;
		sugen(l, nodrat, l->type->width);
		if(nn != Z) {
			warn(n, "non-interruptable temporary");
			nod1 = *nodrat;
			r = n->right;
			if(!r || r->op != OCONST) {
				diag(n, "DOT and no offset");
				break;
			}
			nod1.xoffset += (long)r->vconst;
			nod1.type = n->type;
			sugen(&nod1, nn, w);
		}
		break;

	case OSTRUCT:
		/*
		 * rewrite so lhs has no side effects
		 */
		if(nn != Z && side(nn)) {
			nod1 = *n;
			nod1.type = typ(TIND, n->type);
			regalloc(&nod2, &nod1, Z);
			lcgen(nn, &nod2);
			regsalloc(&nod0, &nod1);
			gopcode(OAS, &nod2, Z, &nod0);
			regfree(&nod2);

			nod1 = *n;
			nod1.op = OIND;
			nod1.left = &nod0;
			nod1.right = Z;
			nod1.complex = 1;

			sugen(n, &nod1, w);
			return;
		}

		r = n->left;
		for(t = n->type->link; t != T; t = t->down) {
			l = r;
			if(r->op == OLIST) {
				l = r->left;
				r = r->right;
			}
			if(nn == Z) {
				cgen(l, nn);
				continue;
			}
			/*
			 * hand craft *(&nn + o) = l
			 */
			nod0 = znode;
			nod0.op = OAS;
			nod0.type = t;
			nod0.left = &nod1;
			nod0.right = l;

			nod1 = znode;
			nod1.op = OIND;
			nod1.type = t;
			nod1.left = &nod2;

			nod2 = znode;
			nod2.op = OADD;
			nod2.type = typ(TIND, t);
			nod2.left = &nod3;
			nod2.right = &nod4;

			nod3 = znode;
			nod3.op = OADDR;
			nod3.type = nod2.type;
			nod3.left = nn;

			nod4 = znode;
			nod4.op = OCONST;
			nod4.type = nod2.type;
			nod4.vconst = t->offset;

			ccom(&nod0);
			acom(&nod0);
			xcom(&nod0);
			nod0.addable = 0;

			cgen(&nod0, Z);
		}
		break;

	case OAS:
		if(nn == Z) {
			if(n->addable < INDEXED)
				sugen(n->right, n->left, w);
			break;
		}
		sugen(n->right, nodrat, w);
		warn(n, "non-interruptable temporary");
		sugen(nodrat, n->left, w);
		sugen(nodrat, nn, w);
		break;

	case OFUNC:
		if(nn == Z) {
			sugen(n, nodrat, w);
			break;
		}
		if(nn->op != OIND) {
			if(nn->op == OREGPAIR) {
				regsalloc(&nod1, nn);
				d = &nod1;
			}else
				d = nn;
			d = new1(OADDR, d, Z);
			d->type = types[TIND];
			d->addable = 0;
		} else
			d = nn->left;
		n = new(OFUNC, n->left, new(OLIST, d, n->right));
		n->complex = FNX;
		n->type = types[TVOID];
		n->left->type = types[TVOID];
		if(nn->op == OREGPAIR){
			freepair(nn);
			cgen(n, Z);
			unfreepair(nn);
			gmove(&nod1, nn);
		} else
			cgen(n, Z);
		break;

	case OCOND:
		bcgen(n->left, 1);
		p1 = p;
		sugen(n->right->left, nn, w);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		sugen(n->right->right, nn, w);
		patch(p1, pc);
		break;

	case OCOMMA:
		cgen(n->left, Z);
		sugen(n->right, nn, w);
		break;
	}
	return;

copy:
	if(nn != Z)
	if(n->complex >= FNX && nn->complex >= FNX) {
		t = nn->type;
		nn->type = types[TLONG];
		regialloc(&nod1, nn, Z);
		lcgen(nn, &nod1);
		regsalloc(&nod2, &nod1);
		nn->type = t;

		gopcode(OAS, &nod1, Z, &nod2);
		regfree(&nod1);

		nod2.type = typ(TIND, t);

		nod1 = nod2;
		nod1.op = OIND;
		nod1.left = &nod2;
		nod1.right = Z;
		nod1.complex = 1;
		nod1.type = t;

		sugen(n, &nod1, w);
		return;
	}

	w /= SZ_LONG;
	if(w == 2 && cgen64(n, nn))
		return;

	if(nn == Z)
		return;

	if(w == 2) {
		if(n->complex > nn->complex) {
			if(n->op != OREGPAIR && n->op != ONAME && n->op != OINDREG)
				reglpcgen(&nod1, n, 1);
			else
				nod1 = *n;
			if(nn->op != OREGPAIR && nn->op != ONAME && nn->op != OINDREG)
				reglpcgen(&nod2, nn, 1);
			else
				nod2 = *nn;
		} else {
			if(nn->op != OREGPAIR && nn->op != ONAME && nn->op != OINDREG)
				reglpcgen(&nod2, nn, 1);
			else
				nod2 = *nn;
			if(n->op != OREGPAIR && n->op != ONAME && n->op != OINDREG)
				reglpcgen(&nod1, n, 1);
			else
				nod1 = *n;
		}
		nod1.type = types[TVLONG];
		nod2.type = types[TVLONG];
		gmove(&nod1, &nod2);
		if(n->op != OREGPAIR && n->op != ONAME && n->op != OINDREG)
			regfree(&nod1);
		if(nn->op != OREGPAIR && nn->op != ONAME && nn->op != OINDREG)
			regfree(&nod2);
		return;
	}

	if(n->complex > nn->complex) {
		reglpcgen(&nod1, n, 0);
		reglpcgen(&nod2, nn, 0);
	} else {
		reglpcgen(&nod2, nn, 0);
		reglpcgen(&nod1, n, 0);
	}

	m = 0;
	for(c = 0; c < w && c < 4; c++) {
		i = tmpreg();
		if (i == 0)
			break;
		reg[i]++;
		m |= 1<<i;
	}
	nod4 = *(nodconst(m));
	if(w < 3*c) {
		for (; w>c; w-=c) {
			gmovm(&nod1, &nod4, 1);
			gmovm(&nod4, &nod2, 1);
		}
		goto out;
	}

	regalloc(&nod3, &regnode, Z);
	gopcode(OAS, nodconst(w/c), Z, &nod3);
	w %= c;
	
	pc1 = pc;
	gmovm(&nod1, &nod4, 1);
	gmovm(&nod4, &nod2, 1);

	gopcode(OSUB, nodconst(1), Z, &nod3);
	gopcode(OEQ, nodconst(0), &nod3, Z);
	p->as = ABGT;
	patch(p, pc1);
	regfree(&nod3);

out:
	if (w) {
		i = 0;
		while (c>w) {
			while ((m&(1<<i)) == 0)
				i++;
			m &= ~(1<<i);
			reg[i] = 0;
			c--;
			i++;
		}
		nod4.vconst = m;
		gmovm(&nod1, &nod4, 0);
		gmovm(&nod4, &nod2, 0);
	}
	i = 0;
	do {
		while ((m&(1<<i)) == 0)
			i++;
		reg[i] = 0;
		c--;
		i++;
	} while (c>0);
	regfree(&nod1);
	regfree(&nod2);
}
