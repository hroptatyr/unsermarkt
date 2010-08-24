/* example order client */
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "order.h"

#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

#define UM_PORT		(12768)

typedef struct po_clo_s {
	int fd;
} *po_clo_t;

static ssize_t
send_order(int fd, umo_t o)
{
	return write(fd, o, sizeof(*o));
}

static ssize_t
send_lmt_order(int fd, oside_t s, m30_t p, uint32_t q)
{
	struct umo_s o = {
		.agent_id = 1,
		.secu_id = 2,
		.p = p,
		.q = q,
		.side = s,
		.type = OTYPE_LIM,
		.tymod = OTYMOD_GTC,
	};
	return send_order(fd, &o);
}

static ssize_t
send_mkt_order(int fd, oside_t s, uint32_t q)
{
	struct umo_s o = {
		.agent_id = 1,
		.secu_id = 2,
		.q = q,
		.side = s,
		.type = OTYPE_MKT,
		.tymod = OTYMOD_GTC,
	};
	return send_order(fd, &o);
}

static int
proc_order(char *line, void *clo)
{
/* format must be B|S QTY [PRI] */
	char *p = line;
	po_clo_t poclo = clo;
	oside_t side;
	uint32_t qty;
	m30_t pri;

	switch (*p) {
	case 'B':
	case 'b':
		side = OSIDE_BUY;
		break;
	case 'S':
	case 's':
		side = OSIDE_SELL;
		break;
	default:
		fputs("Illegal line format\n", stderr);
		return 0;
	}

	/* zap to next space */
	for (; *p && !(*p == ' ' || *p == '\t'); p++);
	/* zap to next thing beyond that space */
	for (; *p && (*p == ' ' || *p == '\t'); p++);

	/* must be quantity */
	qty = strtoul(p, &p, 10);

	/* zap to next thing beyond that space */
	for (; *p && (*p == ' ' || *p == '\t'); p++);

	if (*p == '\0') {
		/* market order */
		pri.v = 0;
		return send_mkt_order(poclo->fd, side, qty);
	} else {
		/* limit order, p should point to the price now */
		pri = ffff_m30_get_s(&p);
		return send_lmt_order(poclo->fd, side, pri, qty);
	}
}


static void
usage(void)
{
	fputs("\
Usage: umoc HOSTNAME\
\n", stderr);
	return;
}

static void
process_lines(FILE *inf, int(*cb)(char *line, void *clo), void *clo)
{
	char *line;
	size_t lno = 0;

	/* no threads reading this stream */
	__fsetlocking(inf, FSETLOCKING_BYCALLER);

	/* loop over the lines */
	for (line = NULL; !feof_unlocked(inf); lno++) {
		ssize_t n;
		size_t len;

		if ((n = getline(&line, &len, inf)) < 0) {
			break;
		}
		/* terminate the string accordingly */
		line[n - 1] = '\0';
		/* process line, check if it's a comment first */
		if (line[0] == '#' || line[0] == '\0') {
			;
		} else if (cb(line, clo) < 0) {
			;
		}
	}
	/* get rid of resources */
	free(line);
	return;
}

int
main(int argc, char *argv[])
{
	int s = socket(PF_INET6, SOCK_STREAM, 0);
	struct sockaddr_in6 sa[1] = {{0}};
	struct hostent *he;
	struct po_clo_s clo[1];

	if (argc != 2) {
		usage();
		return 1;
	}
	if ((he = gethostbyname2(argv[1], AF_INET6)) != NULL) {
		sa->sin6_family = he->h_addrtype;
		sa->sin6_addr = *(struct in6_addr*)(he->h_addr);
		sa->sin6_port = htons(UM_PORT);

	} else if (inet_pton(AF_INET6, argv[1], sa) == 0) {
		sa->sin6_family = AF_INET6;
		sa->sin6_port = htons(UM_PORT);

	} else {
		fprintf(stderr, "Host not found\n");
		return 1;
	}

	if (connect(s, (struct sockaddr*)sa, sizeof(*sa)) < 0) {
		fprintf(stderr, "Connection failed\n");
		return 1;
	}

	/* now listen on stdin for orders
	 * the specs are: 1 order per line in the format:
	 * B|S QTY [PRI]
	 * where B or S indicate buy or sell orders
	 * QTY is the quantity to obtain
	 * and PRI is the price, if omitted a market order will be sent. */
	clo->fd = s;
	process_lines(stdin, proc_order, clo);

	/* and off we go */
	close(s);
	return 0;
}

/* um-order.c ends here */
