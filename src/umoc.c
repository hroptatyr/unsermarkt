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
		.instr_id = 2,
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
		.instr_id = 2,
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

static struct in6_addr in6_anyaddr = IN6ADDR_ANY_INIT;
static inline int
ipv6_addr_any(struct in6_addr *addr)
{
	return (memcmp(addr, &in6_anyaddr, 16) == 0);
}

static int
umoc_connect(const char *host)
{
	struct addrinfo hints[1];
	struct addrinfo *ai;
	struct in6_addr *addr;
	struct sockaddr_in6 firsthop[1];
	uint32_t scope_id = 0;
	int res = -1;

	memset(hints, 0, sizeof(*hints));
	memset(firsthop, 0, sizeof(*firsthop));
	firsthop->sin6_family = AF_INET6;

	hints->ai_family = AF_INET6;
	if (getaddrinfo(host, NULL, hints, &ai) < 0) {
		fprintf(stderr, "Host not found\n");
		return -1;
	}

	addr = &((struct sockaddr_in6*)(ai->ai_addr))->sin6_addr;

	if (ipv6_addr_any(&firsthop->sin6_addr)) {
		memcpy(&firsthop->sin6_addr, addr, 16);

		firsthop->sin6_scope_id =
			((struct sockaddr_in6*)(ai->ai_addr))->sin6_scope_id;
		/* verify scope_id is the same as previous nodes */
		if (firsthop->sin6_scope_id && scope_id &&
		    firsthop->sin6_scope_id != scope_id) {
			fprintf(stderr, "Scope discrepancy among the nodes\n");
			return -1;
		} else if (!scope_id) {
			scope_id = firsthop->sin6_scope_id;
		}
	}
	freeaddrinfo(ai);

	/* get a socket */
	res = socket(PF_INET6, SOCK_STREAM, 0);

	firsthop->sin6_port = htons(UM_PORT);
	if (connect(res, (struct sockaddr*)firsthop, sizeof(*firsthop)) < 0) {
		fprintf(stderr, "Connection failed\n");
		close(res);
		return -1;
	}
	return res;
}

int
main(int argc, char *argv[])
{
	int s;
	struct po_clo_s clo[1];

	if (argc != 2) {
		usage();
		return 1;
	}

	if ((s = umoc_connect(argv[1])) < 0) {
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
