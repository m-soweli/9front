#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "hash.h"

char*
pushutf(char *dst, char *e, char *u, int nrune)
{
	Rune r;
	char *p;
	char *d;

	if(dst >= e)
		return dst;

	d = dst;
	p = u;
	while(d < e-1){
		if(isascii(*p)){
			if((*d = *p) == '\0')
				return d;
			p++;
			d++;
		} else {
			p += chartorune(&r, p);
			if(r == Runeerror){
				*d = '\0';
				return d;
			}
			d += runetochar(d, &r);
		}
		if(nrune > 0 && --nrune == 0)
			break;
	}
	if(d > e-1)
		d = e-1;

	*d = '\0';
	return d;
}

typedef struct Str Str;
struct Str {
	char b[128];
	char *p;
};

#define strend(s) ((s)->b + sizeof (s)->b)

void
resetstr(Str *s, ...)
{
	va_list args;
	va_start(args, s);
	do {
		s->p = s->b;
		s->p[0] = '\0';
		s = va_arg(args, Str*);
	} while(s != nil);
	va_end(args);
}

void
popstr(Str *s)
{
	while(s->p > s->b && (*--s->p & 0xC0)==Runesync)
		;

	s->p[0] = '\0';
}

typedef	struct Map Map;
struct Map {
	char	*roma;
	char	*kana;
	char	leadstomore;
};

Hmap*
openmap(char *file)
{
	Biobuf *b;
	char *s;
	Map map;
	Hmap *h;
	char *key, *val;
	Str partial;
	Rune r;

	h = hmapalloc(64, sizeof(Map));
	b = Bopen(file, OREAD);
	if(b == nil)
		return nil;

	while(key = Brdstr(b, '\n', 1)){
		if(key[0] == '\0'){
		Err:
			free(key);
			continue;
		}

		val = strchr(key, '\t');
		if(val == nil || val[1] == '\0')
			goto Err;

		*val = '\0';
		val++;
		resetstr(&partial, nil);
		for(s = key; *s; s += chartorune(&r, s)){
			partial.p = pushutf(partial.p, strend(&partial), s, 1);
			map.leadstomore = 0;
			if(hmapget(h, partial.b, &map) == 0){
				if(map.leadstomore == 1 && s[1] == '\0')
					map.leadstomore = 1;
			}
			if(s[1] == '\0'){
				map.roma = key;
				map.kana = val;
				hmaprepl(&h, strdup(map.roma), &map, nil, 1);
			} else {
				map.roma = strdup(partial.b);
				map.leadstomore = 1;
				map.kana = nil;
				hmaprepl(&h, strdup(partial.b), &map, nil, 1);
			}
		}
	}
	Bterm(b);
	return h;
}

Hmap*
opendict(Hmap *h, char *name)
{
	Biobuf *b;
	char *p;
	char *dot, *rest;
	char *kouho[16];
	int i;

	b = Bopen(name, OREAD);
	if(b == nil)
		return nil;

	if(h == nil)
		h = hmapalloc(8192, sizeof(kouho));
	else
		hmapreset(h, 1);
	while(p = Brdstr(b, '\n', 1)){
		if(p[0] == '\0' || p[0] == ';'){
		Err:
			free(p);
			continue;
		}
		dot = strchr(p, '\t');
		if(dot == nil)
			goto Err;

		*dot = '\0';
		rest = dot+1;
		if(*rest == '\0')
			goto Err;

		memset(kouho, 0, sizeof kouho);
		i = 0;
		while(i < nelem(kouho)-1 && (dot = utfrune(rest, ' '))){
			*dot = '\0';
			kouho[i++] = rest;
			rest = dot+1;
		}
		if(i < nelem(kouho)-1)
			kouho[i] = rest;

		/* key is the base pointer; overwrites clean up for us */
		hmaprepl(&h, p, kouho, nil, 1);
	}
	Bterm(b);
	return h;
}

enum{
	LangEN 	= '',	// ^t
	LangJP	= '', 	// ^n
	LangJPK = '',	// ^k
	LangRU 	= '',	// ^r
	LangEL	= '',	// ^o
	LangKO	= '',	// ^s
	LangZH	= '',	// ^c
};

int deflang;

Hmap *natural;
Hmap *hira, *kata, *jisho;
Hmap *cyril;
Hmap *greek;
Hmap *hangul;
Hmap *hanzi, *zidian;

Hmap **langtab[] = {
	[LangEN]  &natural,
	[LangJP]  &hira,
	[LangJPK] &kata,
	[LangRU]  &cyril,
	[LangEL]  &greek,
	[LangKO]  &hangul,
	[LangZH]  &hanzi,
};

char *langcodetab[] = {
	[LangEN]  "en",
	[LangJP]  "jp",
	[LangJPK] "jpk",
	[LangRU]  "ru",
	[LangEL]  "el",
	[LangKO]  "ko",
	[LangZH]  "zh",
};

int
parselang(char *s)
{
	int i;

	for(i = 0; i < nelem(langcodetab); i++){
		if(langcodetab[i] == nil)
			continue;
		if(strcmp(langcodetab[i], s) == 0)
			return i;
	}

	return -1; 
}

int
checklang(int *dst, int c)
{
	Hmap **p;

	if(c >= nelem(langtab))
		return 0;

	p = langtab[c];
	if(p == nil)
		return 0;

	*dst = c;
	return c;
}

int
maplkup(int lang, char *s, Map *m)
{
	Hmap **h;

	if(lang >= nelem(langtab))
		return -1;

	h = langtab[lang];
	if(h == nil || *h == nil)
		return -1;

	return hmapget(*h, s, m);
}

typedef struct Msg Msg;
struct Msg {
	char code;
	char buf[64];
};
static Channel *dictch;
static Channel *output;
static Channel *input;
static char  backspace[64];

static int
emitutf(Channel *out, char *u, int nrune)
{
	Msg m;
	char *e;

	m.code = 'c';
	e = pushutf(m.buf, m.buf + sizeof m.buf, u, nrune);
	send(out, &m);
	return e - m.buf;
}

static void
dictthread(void*)
{
	Msg m;
	Rune r;
	int n;
	char *p;
	Hmap *dict;
	char *kouho[16];
	Str line;
	Str last;
	Str okuri;
	int selected;

	enum{
		Kanji,
		Okuri,
		Joshi,
	};
	int mode;

	dict = jisho;
	selected = -1;
	kouho[0] = nil;
	mode = Kanji;
	resetstr(&last, &line, &okuri, nil);

	threadsetname("dict");
	while(recv(dictch, &m) != -1){
		for(p = m.buf; *p; p += n){
			n = chartorune(&r, p);
			if(r != ''){
				if(selected >= 0){
					resetstr(&okuri, nil);
					mode = Kanji;
				}
				resetstr(&last, nil);
				selected = -1;
				kouho[0] = nil;
			}
			switch(r){
			case LangJP:
				dict = jisho;
				break;
			case LangZH:
				dict = zidian;
				break;
			case '':
				if(line.b == line.p){
					emitutf(output, "", 1);
					break;
				}
				emitutf(output, backspace, utflen(line.b));
				/* fallthrough */
			case ' ': case ',': case '.':
			case '':
				mode = Kanji;
				resetstr(&line, &okuri, nil);
				break;
			case '\b':
				if(mode != Kanji){
					if(okuri.p == okuri.b){
						mode = Kanji;
						popstr(&line);
					}else
						popstr(&okuri);
					break;
				}
				popstr(&line);
				break;
			case '\n':
				if(line.b == line.p){
					emitutf(output, "\n", 1);
					break;
				}
				/* fallthrough */
			case '':
				selected++;
				if(selected == 0){
					if(hmapget(dict, line.b, kouho) < 0){
						resetstr(&line, &last, nil);
						selected = -1;
						break;
					}
					if(dict == jisho && line.p > line.b && isascii(line.p[-1]))
						line.p[-1] = '\0';
				}
				if(kouho[selected] == nil){
					/* cycled through all matches; bail */
					emitutf(output, backspace, utflen(last.b));
					emitutf(output, line.b, 0);
					resetstr(&line, &last, &okuri, nil);
					selected = -1;
					break;
				}

				if(okuri.p != okuri.b)
					emitutf(output, backspace, utflen(okuri.b));
				if(selected == 0)
					emitutf(output, backspace, utflen(line.b));
				else
					emitutf(output, backspace, utflen(last.b));

				emitutf(output, kouho[selected], 0);
				last.p = pushutf(last.b, strend(&last), kouho[selected], 0);
				emitutf(output, okuri.b, 0);

				resetstr(&line, nil);
				mode = Kanji;
				break;
			default:
				if(dict == zidian){
					line.p = pushutf(line.p, strend(&line), p, 1);
					break;
				}

				if(mode == Joshi){
					okuri.p = pushutf(okuri.p, strend(&okuri), p, 1);
					break;
				}
	
				if(isupper(*p)){
					if(mode == Okuri){
						popstr(&line);
						mode = Joshi;
						okuri.p = pushutf(okuri.p, strend(&okuri), p, 1);
						break;
					}
					mode = Okuri;
					*p = tolower(*p);
					line.p = pushutf(line.p, strend(&line), p, 1);
					okuri.p = pushutf(okuri.b, strend(&okuri), p, 1);
					break;	
				}
				if(mode == Kanji)
					line.p = pushutf(line.p, strend(&line), p, 1);
				else
					okuri.p = pushutf(okuri.p, strend(&okuri), p, 1);
				break;
			}
		}
	}
}

static void
keythread(void*)
{
	int lang;
	Msg m;
	Map lkup;
	char *p;
	int n;
	Rune r;
	char peek[UTFmax+1];
	Str line;
	int mode;

	mode = 0;
	peek[0] = lang = deflang;
	resetstr(&line, nil);
	if(lang == LangJP || lang == LangZH)
		emitutf(dictch, peek, 1);

	threadsetname("keytrans");
	while(recv(input, &m) != -1){
		if(m.code == 'r'){
			emitutf(dictch, "", 1);
			resetstr(&line, nil);
			continue;
		}
		if(m.code != 'c'){
			send(output, &m);
			continue;
		}

		for(p = m.buf; *p; p += n){
			n = chartorune(&r, p);
			if(checklang(&lang, r)){
				emitutf(dictch, "", 1);
				if(lang == LangJP || lang == LangZH)
					emitutf(dictch, p, 1);
				resetstr(&line, nil);
				continue;
			}
			if(lang == LangZH || lang == LangJP){
				emitutf(dictch, p, 1);
				if(utfrune("\n", r) != nil){
					resetstr(&line, nil);
					continue;
				}
				if(lang == LangJP && isupper(*p)){
					*p = tolower(*p);
					mode++;
				} else {
					mode = 0;
				}
			}

			emitutf(output, p, 1);
			if(lang == LangEN || lang == LangZH)
				continue;
			if(r == '\b'){
				popstr(&line);
				continue;
			}

			line.p = pushutf(line.p, strend(&line), p, 1);
			if(maplkup(lang, line.b, &lkup) < 0){
				resetstr(&line, nil);
				pushutf(peek, peek + sizeof peek, p, 1);
				if(maplkup(lang, peek, &lkup) == 0)
					line.p = pushutf(line.p, strend(&line), p, 1);
				continue;
			}
			if(lkup.kana == nil)
				continue;

			if(!lkup.leadstomore)
				resetstr(&line, nil);

			if(lang == LangJP){
				emitutf(dictch, backspace, utflen(lkup.roma));
				emitutf(dictch, lkup.kana, 0);
			}
			emitutf(output, backspace, utflen(lkup.roma));
			emitutf(output, lkup.kana, 0);
		}
	}
}

static int kbdin;
static int kbdout;

void
kbdtap(void*)
{
	Msg msg;
	char buf[128];
	char *p, *e;
	int n;

	threadsetname("kbdtap");
	for(;;){
Drop:
		n = read(kbdin, buf, sizeof buf);
		if(n < 0)
			break;
		for(p = buf; p < buf+n;){
			msg.code = p[0];
			p++;
			switch(msg.code){
			case 'c': case 'k': case 'K':
			case 'r':
				break;
			default:
				goto Drop;
			}
			e = utfecpy(msg.buf, msg.buf + sizeof msg.buf, p);
			p += e - msg.buf;
			p++;
			if(send(input, &msg) == -1)
				return;
		}
	}
}

void
kbdsink(void*)
{
	Msg m;
	char *p;
	Rune rn;

	threadsetname("kbdsink");
	while(recv(output, &m) != -1){
		if(m.code != 'c'){
			fprint(kbdout, "%c%s", m.code, m.buf);
			continue;
		}
		p = m.buf;
		for(;;){
			p += chartorune(&rn, p);
			if(rn == Runeerror || rn == '\0')
				break;
			fprint(kbdout, "c%C", rn);
		}
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [ -l lang ] [ kbdtap ]\n", argv0);
	threadexits("usage");
}

mainstacksize = 8192*2;

void
threadmain(int argc, char *argv[])
{

	char *jishoname, *zidianname;

	deflang = LangEN;
	ARGBEGIN{
	case 'l':
		deflang = parselang(EARGF(usage()));
		if(deflang < 0)
			usage();
		break;
	default:
		usage();
	}ARGEND;
	switch(argc){
	case 0:
		kbdin = 0;
		kbdout = 1;
		break;
	case 1:
		kbdin = kbdout = open(argv[0], ORDWR);
		if(kbdin < 0)
			sysfatal("failed to open kbdtap: %r");
		break;
	default:
		usage();
	}

	memset(backspace, '\b', sizeof backspace-1);
	backspace[sizeof backspace-1] = '\0';

	if((jishoname = getenv("jisho")) == nil)
		jishoname = "/lib/ktrans/kanji.dict";
	jisho = opendict(nil, jishoname);

	if((zidianname = getenv("zidian")) == nil)
		zidianname = "/lib/ktrans/wubi.dict";
	zidian = opendict(nil, zidianname);

	natural = hanzi = nil;
	hira 	= openmap("/lib/ktrans/hira.map");
	kata 	= openmap("/lib/ktrans/kata.map");
	greek 	= openmap("/lib/ktrans/greek.map");
	cyril 	= openmap("/lib/ktrans/cyril.map");
	hangul 	= openmap("/lib/ktrans/hangul.map");

	dictch 	= chancreate(sizeof(Msg), 0);
	input 	= chancreate(sizeof(Msg), 0);
	output 	= chancreate(sizeof(Msg), 0);

	proccreate(kbdtap, nil, mainstacksize);
	proccreate(kbdsink, nil, mainstacksize);
	threadcreate(dictthread, nil, mainstacksize);
	threadcreate(keythread, nil, mainstacksize);

	threadexits(nil);
}