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


/* XML mumbo jumbo */
#include <mxml.h>

static void
bla(const char *buf, size_t bsz)
{
	mxml_node_t *bla;

	if ((bla = mxmlLoadString(NULL, buf, MXML_NO_CALLBACK)) == NULL) {
		return;
	}
	mxmlSaveFd(bla, STDERR_FILENO, MXML_NO_CALLBACK);
	mxmlDelete(bla);
	return;
}


/* the supply process:
 * - donors must pause for 12 weeks
 * - typical donation is 1 unit
 * - 38% of the population is eligible => 31.16M for de
 * - makes at most 31.16M units of blood per 12 weeks = 519k units per bday
 * - typically 5% of eligible donors actually donate blood
 *   => roughly 25966 units/bday for de */

/* the demand process:
 * - 38000 units per day for 260M people ~ 146 units per 1M people
 *   = at least 11985 units for de
 * - car accident: 100 units per victim
 * - 10% of hospital inhabitants need 1 unit of blood per day */

/* pricing: for 1 unit on average 123 */

typedef enum {
	BG_UNK,
	BG_AB,
	BG_0,
	BG_A,
	BG_B,
	NBGS
} bgrp_t;

typedef double bgdist_t[NBGS];
typedef double bgdmnd_t[NBGS];
typedef double bgsupp_t[NBGS];
typedef struct fund_state_s *fund_state_t;

struct fund_state_s {
	/* demand */
	bgdmnd_t d;
	/* supply */
	bgsupp_t s;
	/* distribution */
	bgdist_t v;
};

static void
setup_dist(bgdist_t d)
{
/* germany's distro 0, A, B, AB
 * Rh+ 35% 37% 9% 4%, Rh- 6% 6% 2% 1%
 * */
	d[BG_AB] = 5.0;
	d[BG_0] = 41.0;
	d[BG_A] = 43.0;
	d[BG_B] = 11.0;
	return;
}

static void
setup_supp(bgsupp_t s)
{
/* units/day */
	s[BG_AB] = 18547.0 * 5.0;
	s[BG_0] = 18547.0 * 41.0;
	s[BG_A] = 18547.0 * 43.0;
	s[BG_B] = 18547.0 * 11.0;
	return;
}

static void
setup_dmnd(bgdmnd_t d)
{
/* units/day,
 * actually 0 donors can be used everywhere */
	d[BG_AB] = 11985.0 * 5.0;
	d[BG_0] = 11985.0 * 41.0;
	d[BG_A] = 11985.0 * 43.0;
	d[BG_B] = 11985.0 * 11.0;
	return;
}

static void
setup_state(fund_state_t s)
{
	setup_supp(s->s);
	setup_dmnd(s->d);
	setup_dist(s->v);
	return;
}


#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

#define UM_PORT		(8787)

#define BAB	(2)
#define B0	(3)
#define BA	(4)
#define BB	(5)

static const char *isym[NBGS] = {
	[BG_AB] = "BAB",
	[BG_0] = "B0",
	[BG_A] = "BA",
	[BG_B] = "BB"
};
static const char isid[4] = {[OSIDE_BUY] = 'B', [OSIDE_SELL] = 'S'};

static char buf[4096];

static void
send_cancel(int fd, uint64_t oid)
{
	char *p;
	ssize_t bsz;

	bsz = snprintf(buf, sizeof(buf), "CANCEL %lu\n", oid);
	write(fd, buf, bsz);
	/* do we need the confirmation? */
	if ((bsz = read(fd, buf, bsz)) < 0) {
		return;
	}
	if (p = strstr(buf, "<?xml")) {
		bla(p, bsz - (p - buf));
	}
	return;
}

static uint64_t
send_order(int fd, umo_t o)
{
	ssize_t bsz;
	uint64_t res;
	char *p;

	ffff_m30_s(buf + 4096 - 32, o->p);
	bsz = snprintf(
		buf, sizeof(buf), "ORDER %s %c %u %s\n",
		isym[o->instr_id - 1], isid[o->side],
		o->q, buf + 4096 - 32);
	write(fd, buf, bsz);
	/* there should be a reply now */
	if ((bsz = read(fd, buf, sizeof(buf))) <= 0) {
		return 0;
	}
	res = strtoul(buf, &p, 10);
	bla(p, bsz - (p - buf));
	return res;
}

static void
make_market(int fd, fund_state_t st)
{
	struct umo_s o[NBGS][2];
	uint64_t oid[NBGS][2];

	for (int i = BG_UNK; i < NBGS; i++) {
		o[i][0].instr_id = i + 1;
		o[i][1].instr_id = i + 1;
		o[i][0].side = OSIDE_BUY;
		o[i][1].side = OSIDE_SELL;
	}

	/* initial price finding */
	for (int i = BG_UNK + 1; i < NBGS; i++) {
		o[i][0].p = ffff_m30_get_d(123.80);
		o[i][0].q = (uint32_t)(st->d[i] / 100.0);

		o[i][1].p = ffff_m30_get_d(132.20);
		o[i][1].q = (uint32_t)(st->s[i] / 100.0);
	}

	sleep(3);

	for (int i = BG_UNK + 1; i < NBGS; i++) {
		oid[i][0] = send_order(fd, &o[i][0]);
		oid[i][1] = send_order(fd, &o[i][1]);
	}

	sleep(3);

	for (int i = BG_UNK + 1; i < NBGS; i++) {
		send_cancel(fd, oid[i][0]);
		send_cancel(fd, oid[i][1]);
	}
	sleep(3);
	return;
}


static void
usage(void)
{
	fputs("\
Usage: blood-mm HOSTNAME BTYPE\
\n", stderr);
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

	hints->ai_family = AF_UNSPEC;
	if (getaddrinfo(host, NULL, hints, &ai) < 0) {
		fprintf(stderr, "Host not found\n");
		return -1;
	}

	addr = &((struct sockaddr_in6*)(ai->ai_addr))->sin6_addr;
	if ((firsthop->sin6_family = ai->ai_family) == AF_INET) {
		((struct sockaddr_in*)firsthop)->sin_addr =
			((struct sockaddr_in*)ai->ai_addr)->sin_addr;
	} else if (ipv6_addr_any(&firsthop->sin6_addr)) {
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
	res = socket(firsthop->sin6_family, SOCK_STREAM, 0);

	firsthop->sin6_port = htons(UM_PORT);
	if (connect(res, (struct sockaddr*)firsthop, sizeof(*firsthop)) < 0) {
		fprintf(stderr, "Connection failed\n");
		close(res);
		return -1;
	}
	return res;
}

static agtid_t
bmm_ehlo(int fd)
{
	static const char ehlo[] = "EHLO BAB MM";
	char buf[32];

	write(fd, ehlo, sizeof(ehlo));
	if (read(fd, buf, sizeof(buf)) <= 0) {
		return 0;
	} else if (memcmp(buf, "EHLO", 4) != 0) {
		return 0;
	}
	return strtoul(buf + 4, NULL, 10);
}


int
main(int argc, char *argv[])
{
	int s;
	struct fund_state_s st[1];
	agtid_t aid;

	if (argc != 2) {
		usage();
		return 1;
	}

	if ((s = umoc_connect(argv[1])) < 0) {
		return 1;
	}

	/* initial setup */
	setup_state(st);
	/* say hello */
	if ((aid = bmm_ehlo(s)) > 0) {
		/* start the market making :) */
		fprintf(stderr, "our agent id is %u\n", aid);
		make_market(s, st);
	}
	/* and off we go */
	close(s);
	return 0;
}

/* blood-mm.c ends here */
