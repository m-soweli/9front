#include <u.h>
#include <libc.h>
#include <ip.h>

int
myetheraddr(uchar *to, char *dev)
{
	int n, fd;
	char buf[256];

	if(*dev == '/' || *dev == '#')
		snprint(buf, sizeof buf, "%s/addr", dev);
	else
		snprint(buf, sizeof buf, "/net/%s/addr", dev);

	fd = open(buf, OREAD|OCEXEC);
	if(fd < 0)
		return -1;

	n = read(fd, buf, sizeof buf -1 );
	close(fd);
	if(n <= 0)
		return -1;
	buf[n] = 0;

	parseether(to, buf);
	return 0;
}
