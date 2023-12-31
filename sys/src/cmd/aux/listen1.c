#include <u.h>
#include <libc.h>
#include <auth.h>

int maxprocs;
int verbose;
int trusted;
int oneshot;
char *nsfile;
int nsopts, ncopts = 1;
char *sopts[16], *copts[16] = { "keepalive", };

void
usage(void)
{
	fprint(2, "usage: listen1 [-1tv]"
		" [-n namespace] [-p maxprocs]"
		" [-O msg] [-o msg]"
		" address cmd args...\n");
	exits("usage");
}

void
becomenone(void)
{
	if(procsetuser("none") < 0)
		sysfatal("can't become none: %r");
	if(newns("none", nsfile) < 0)
		sysfatal("can't build namespace: %r");
}

char*
remoteaddr(char *dir)
{
	static char buf[128];
	char *p;
	int n, fd;

	snprint(buf, sizeof buf, "%s/remote", dir);
	fd = open(buf, OREAD);
	if(fd < 0)
		return "";
	n = read(fd, buf, sizeof(buf));
	close(fd);
	if(n > 0){
		buf[n] = 0;
		p = strchr(buf, '!');
		if(p)
			*p = 0;
		return buf;
	}
	return "";
}

void
main(int argc, char **argv)
{
	char data[60], dir[40], ndir[40], wbuf[64];
	int ctl, nctl, fd, i;
	int wfd, nowait, procs;
	Dir *d;

	ARGBEGIN{
	default:
		usage();
	case '1':
		oneshot = 1;
		break;
	case 't':
		trusted = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	case 'p':
		maxprocs = atoi(EARGF(usage()));
		break;
	case 'n':
		nsfile = EARGF(usage());
		break;
	case 'o':
		if(ncopts >= nelem(copts))
			sysfatal("too many -o options");
		copts[ncopts++] = EARGF(usage());
		break;
	case 'O':
		if(nsopts >= nelem(sopts))
			sysfatal("too many -O options");
		sopts[nsopts++] = EARGF(usage());
		break;
	}ARGEND

	if(argc < 2)
		usage();

	if(!verbose){
		close(1);
		fd = open("/dev/null", OWRITE);
		if(fd != 1){
			dup(fd, 1);
			close(fd);
		}
	}

	if(!trusted)
		becomenone();

	fprint(2, "listen started\n");
	ctl = announce(argv[0], dir);
	if(ctl < 0)
		sysfatal("announce %s: %r", argv[0]);

	for(i = 0; i < nsopts; i++){
		if(write(ctl, sopts[i], strlen(sopts[i])) < 0)
			fprint(2, "%s/ctl: can't write %s: %r\n", dir, sopts[i]);
	}

	wfd = -1;
	nowait = RFNOWAIT;
	if(maxprocs > 0){
		snprint(wbuf, sizeof(wbuf), "/proc/%d/wait", getpid());
		if((wfd = open(wbuf, OREAD)) >= 0)
			nowait = 0;
	}
	procs = 0;
	for(;;){
		if(nowait == 0 && (procs >= maxprocs || (procs % 8) == 0))
			while(procs > 0){
				if(procs < maxprocs){
					d = dirfstat(wfd);
					if(d == nil || d->length == 0){
						free(d);
						break;
					}
					free(d);
				}
				if(read(wfd, wbuf, sizeof(wbuf)) > 0)
					procs--;
			}

		nctl = listen(dir, ndir);
		if(nctl < 0)
			sysfatal("listen %s: %r", argv[0]);

		if(!oneshot)
		switch(rfork(RFFDG|RFPROC|RFMEM|RFENVG|RFNAMEG|RFNOTEG|RFREND|nowait)){
		case 0:
			break;
		case -1:
			reject(nctl, ndir, "host overloaded");
			close(nctl);
			continue;
		default:
			close(nctl);
			procs++;
			continue;
		}

		fd = accept(nctl, ndir);
		if(fd < 0){
			fprint(2, "accept %s: can't open  %s/data: %r\n", argv[0], ndir);
			if(oneshot){
				close(nctl);
				continue;
			}
			exits("accept");
		}

		fprint(2, "incoming call for %s from %s in %s\n", argv[0], remoteaddr(ndir), ndir);

		for(i = 0; i < ncopts; i++)
			write(nctl, copts[i], strlen(copts[i]));

		close(ctl);
		close(nctl);
		if(wfd >= 0)
			close(wfd);
		putenv("net", ndir);
		snprint(data, sizeof data, "%s/data", ndir);
		bind(data, "/dev/cons", MREPL);
		dup(fd, 0);
		dup(fd, 1);
		/* dup(fd, 2); keep stderr */
		if(fd > 2) close(fd);
		exec(argv[1], argv+1);
		if(argv[1][0] != '/')
			exec(smprint("/bin/%s", argv[1]), argv+1);
		fprint(2, "%s: exec: %r\n", argv0);
		exits("exec");
	}
}
