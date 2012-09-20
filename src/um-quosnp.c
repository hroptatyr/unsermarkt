/*** um-quosnp.c -- unsermarkt quote snapper
 *
 * Copyright (C) 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of unsermarkt.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
/** Brief:
 * This basically does the same job as um-quodmp but without actually
 * writing the quotes to the disk, instead keep the last state in a
 * memory mapped file and respond to url queries.
 * Eventually this stuff will go back to um-quodmp. */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
/* for gettimeofday() */
#include <sys/time.h>

#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <netdb.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

#define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "nifty.h"
#include "ud-sock.h"
#include "gq.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

/* maximum allowed age for clients (in seconds) */
#if defined DEBUG_FLAG
# define MAX_CLI_AGE	(300)
# define PRUNE_INTV	(10.0)
#else  /* !DEBUG_FLAG */
# define MAX_CLI_AGE	(1800)
# define PRUNE_INTV	(60.0)
#endif	/* DEBUG_FLAG */

static FILE *logerr;
#if defined DEBUG_FLAG
# define UMQS_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define UMQS_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef size_t cli_t;
typedef intptr_t hx_t;

typedef struct urifq_s *urifq_t;
typedef struct urifi_s *urifi_t;

typedef struct ev_io_q_s *ev_io_q_t;
typedef struct ev_io_i_s *ev_io_i_t;

struct key_s {
	ud_sockaddr_t sa;
	uint16_t id;
};

/* the client */
struct cli_s {
	union ud_sockaddr_u sa __attribute__((aligned(16)));
	uint16_t id;
	uint16_t tgtid;

	volatile uint32_t last_seen;

	char sym[64];
};

/* uri fetch queue */
struct urifq_s {
	struct gq_s q[1];
	struct gq_ll_s fetchq[1];
};

struct urifi_s {
	struct gq_item_s i;
	char uri[256];
	uint16_t idx;
};

/* ev io object queue */
struct ev_io_q_s {
	struct gq_s q[1];
};

struct ev_io_i_s {
	struct gq_item_s i;
	ev_io w[1];
	uint16_t idx;
	/* reply buffer, pointer and size */
	char *rpl;
	size_t rsz;
};

/* children need access to beef resources */
static ev_io *beef = NULL;
static size_t nbeef = 0;


/* date and time funs, could use libdut from dateutils */
static int
__leapp(unsigned int y)
{
	return !(y % 4);
}

static void
ffff_gmtime(struct tm *tm, const time_t t)
{
#define UTC_SECS_PER_DAY	(86400)
	static uint16_t __mon_yday[] = {
		/* cumulative, first element is a bit set of leap days to add */
		0xfff8, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};
	register int days;
	register int secs;
	register unsigned int yy;
	const uint16_t *ip;


	/* just go to day computation */
	days = (int)(t / UTC_SECS_PER_DAY);
	/* time stuff */
	secs = (int)(t % UTC_SECS_PER_DAY);

	/* gotta do the date now */
	yy = 1970;
	/* stolen from libc */
#define DIV(a, b)		((a) / (b))
/* we only care about 1901 to 2099 and there are no bullshit leap years */
#define LEAPS_TILL(y)		(DIV(y, 4))
	while (days < 0 || days >= (!__leapp(yy) ? 365 : 366)) {
		/* Guess a corrected year, assuming 365 days per year. */
		register unsigned int yg = yy + days / 365 - (days % 365 < 0);

		/* Adjust DAYS and Y to match the guessed year.  */
		days -= (yg - yy) * 365 +
			LEAPS_TILL(yg - 1) - LEAPS_TILL(yy - 1);
		yy = yg;
	}
	/* set the year */
	tm->tm_year = (int)yy - 1900;

	ip = __mon_yday;
	/* unrolled */
	yy = 13;
	if (days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy]) {
		yy = 1;
	}
	/* set the rest of the tm structure */
	tm->tm_mday = days - ip[yy] + 1;
	tm->tm_yday = days;
	tm->tm_mon = (int)yy - 1;

	tm->tm_sec = secs % 60;
	secs /= 60;
	tm->tm_min = secs % 60;
	secs /= 60;
	tm->tm_hour = secs;

	/* fix up leap years */
	if (UNLIKELY(__leapp(tm->tm_year))) {
		if ((ip[0] >> (yy)) & 1) {
			if (UNLIKELY(tm->tm_yday == 59)) {
				tm->tm_mon = 1;
				tm->tm_mday = 29;
			} else if (UNLIKELY(tm->tm_yday == ip[yy])) {
				tm->tm_mday = tm->tm_yday - ip[tm->tm_mon--];
			} else {
				tm->tm_mday--;
			}
		}
	}
	return;
}

static void
ffff_strfdtu(char *restrict buf, size_t bsz, time_t sec, unsigned int usec)
{
	struct tm tm[1];

	ffff_gmtime(tm, sec);
	/* libdut? */
	strftime(buf, bsz, "%FT%T", tm);
	buf[19] = '.';
	snprintf(buf + 20, bsz - 20, "%06u+0000", usec);
	return;
}


/* uri fetch queue */
static struct urifq_s urifq = {0};

static urifi_t
make_uri(void)
{
	urifi_t res;

	if (urifq.q->free->i1st == NULL) {
		size_t nitems = urifq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(urifq.q->free->ilst == NULL);
		UMQS_DEBUG("URIFQ RESIZE -> %zu\n", nitems + 16);
		df = init_gq(urifq.q, sizeof(*res), nitems + 16);
		gq_rbld_ll(urifq.fetchq, df);
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(urifq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static void
free_uri(urifi_t uri)
{
	gq_push_tail(urifq.q->free, (gq_item_t)uri);
	return;
}

static void
push_uri(urifi_t uri)
{
	gq_push_tail(urifq.fetchq, (gq_item_t)uri);
	return;
}

static urifi_t
pop_uri(void)
{
	void *res = gq_pop_head(urifq.fetchq);
	return res;
}


/* ev io object queue */
static struct ev_io_q_s ioq = {0};

static ev_io_i_t
make_io(void)
{
	ev_io_i_t res;

	if (ioq.q->free->i1st == NULL) {
		size_t nitems = ioq.q->nitems / sizeof(*res);

		assert(ioq.q->free->ilst == NULL);
		UMQS_DEBUG("IOQ RESIZE -> %zu\n", nitems + 16);
		init_gq(ioq.q, sizeof(*res), nitems + 16);
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(ioq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static void
free_io(ev_io_i_t io)
{
	gq_push_tail(ioq.q->free, (gq_item_t)io);
	return;
}


#if !defined HAVE_UTE_FREE
/* for the moment we provide compatibility with uterus v0.2.2 */
struct utectx_s {
	/** file descriptor we're banging on about */
	int fd;
};

static void
ute_free(utectx_t ctx)
{
	struct utectx_s *p = ctx;
	close(p->fd);
	p->fd = -1;
	free(ctx);
	return;
}
#endif	/* HAVE_UTE_FREE */


/* we support a maximum of 64 order books atm */
static struct cli_s *cli = NULL;
static hx_t *chx = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)

static void
init_cli(void)
{
	/* and our client list */
	cli = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	chx = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	alloc_cli = 4096;
	return;
}

static void
fini_cli(void)
{
	/* and the list of clients */
	munmap(cli, alloc_cli);
	munmap(chx, alloc_cli);
	return;
}

static void
resz_cli(size_t nu)
{
	cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
	chx = mremap(chx, alloc_cli, nu, MREMAP_MAYMOVE);
	alloc_cli = nu;
	return;
}

static int
sa_eq_p(ud_const_sockaddr_t sa1, ud_const_sockaddr_t sa2)
{
#define s6a8(x)		(x)->sa6.sin6_addr.s6_addr8
#define s6a16(x)	(x)->sa6.sin6_addr.s6_addr16
#define s6a32(x)	(x)->sa6.sin6_addr.s6_addr32
#if SIZEOF_INT == 4
# define s6a		s6a32
#elif SIZEOF_INT == 2
# define s6a		s6a16
#else
# error sockaddr comparison will fail
#endif	/* sizeof(long int) */
	return sa1->sa6.sin6_family == sa2->sa6.sin6_family &&
		sa1->sa6.sin6_port == sa2->sa6.sin6_port &&
#if SIZEOF_INT <= 4
		s6a(sa1)[0] == s6a(sa2)[0] &&
		s6a(sa1)[1] == s6a(sa2)[1] &&
		s6a(sa1)[2] == s6a(sa2)[2] &&
		s6a(sa1)[3] == s6a(sa2)[3] &&
#endif
#if SIZEOF_INT <= 2
		s6a(sa1)[4] == s6a(sa2)[4] &&
		s6a(sa1)[5] == s6a(sa2)[5] &&
		s6a(sa1)[6] == s6a(sa2)[6] &&
		s6a(sa1)[7] == s6a(sa2)[7] &&
#endif
		1;
}

static hx_t __attribute__((pure, const))
compute_hx(struct key_s k)
{
	/* we need collision stats from a production run */
	return k.sa->sa6.sin6_family ^ k.sa->sa6.sin6_port ^
		s6a32(k.sa)[0] ^
		s6a32(k.sa)[1] ^
		s6a32(k.sa)[2] ^
		s6a32(k.sa)[3] ^
		k.id;
}

static cli_t
find_cli(struct key_s k)
{
	hx_t khx = compute_hx(k);

	for (size_t i = 0; i < ncli; i++) {
		if (chx[i] == khx &&
		    cli[i].id == k.id &&
		    sa_eq_p(&cli[i].sa, k.sa)) {
			return i + 1;
		}
	}
	return 0;
}

static cli_t
add_cli(struct key_s k)
{
	cli_t idx = ncli++;

	if (ncli * sizeof(*cli) > alloc_cli) {
		resz_cli(alloc_cli + 4096);
	}

	cli[idx].sa = *k.sa;
	cli[idx].id = k.id;
	cli[idx].tgtid = 0;
	cli[idx].last_seen = 0;
	chx[idx] = compute_hx(k);
	return idx + 1;
}

static __attribute__((noinline)) void
prune_cli(cli_t c)
{
	/* wipe it all */
	memset(CLI(c), 0, sizeof(struct cli_s));
	return;
}

static bool
cli_pruned_p(cli_t c)
{
	return CLI(c)->id == 0 && CLI(c)->tgtid == 0;
}

static __attribute__((noinline)) void
prune_clis(void)
{
	struct timeval tv[1];
	size_t nu_ncli = ncli;

	/* what's the time? */
	gettimeofday(tv, NULL);

	/* prune clis */
	for (cli_t i = 1; i <= ncli; i++) {
		if (CLI(i)->last_seen + MAX_CLI_AGE < tv->tv_sec) {
			UMQS_DEBUG("pruning %zu\n", i);
			prune_cli(i);
		}
	}

	/* condense the cli array a bit */
	for (cli_t i = 1; i <= ncli; i++) {
		size_t consec;

		for (consec = 0; i <= ncli && cli_pruned_p(i); i++) {
			consec++;
		}
		assert(consec <= ncli);
		assert(consec <= i);
		if (consec && i <= ncli) {
			/* shrink */
			size_t nmv = ncli - i;

			UMQS_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			memcpy(CLI(i - consec), CLI(i), nmv * sizeof(*cli));
			nu_ncli -= consec;
		} else if (consec) {
			UMQS_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			nu_ncli -= consec;
		}
	}

	/* let everyone know how many clis we've got */
	ncli = nu_ncli;
	return;
}


/* networking */
static int
make_dccp(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_DCCP, IPPROTO_DCCP)) < 0) {
		return s;
	}
	/* mark the address as reusable */
	setsock_reuseaddr(s);
	/* impose a sockopt service, we just use 1 for now */
	set_dccp_service(s, 1);
	/* make a timeout for the accept call below */
	setsock_rcvtimeo(s, 1000);
	/* make socket non blocking */
	setsock_nonblock(s);
	/* turn off nagle'ing of data */
	setsock_nodelay(s);
	return s;
}

static int
make_tcp(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return s;
	}
	/* reuse addr in case we quickly need to turn the server off and on */
	setsock_reuseaddr(s);
	/* turn lingering on */
	setsock_linger(s, 1);
	return s;
}

static int
sock_listener(int s, ud_sockaddr_t sa)
{
	if (s < 0) {
		return s;
	}

	if (bind(s, &sa->sa, sizeof(*sa)) < 0) {
		return -1;
	}

	return listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
}

static int
rslv(struct addrinfo **res, const char *host, short unsigned int port)
{
	char strport[32];
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	/* Convert the port number into a string. */
	snprintf(strport, sizeof(strport), "%hu", port);

	return getaddrinfo(host, strport, &hints, res);
}

static int
conn(struct addrinfo *ais)
{
	int s = -1;

	for (struct addrinfo *aip = ais; aip; aip = aip->ai_next) {
		if ((s = socket(aip->ai_family, aip->ai_socktype, 0)) < 0) {
			/* great way to start the day */
			;
		} else if (connect(s, aip->ai_addr, aip->ai_addrlen) < 0) {
			/* bugger */
			close(s);
		} else {
			/* yippie */
			break;
		}
	}
	freeaddrinfo(ais);
	return s;
}


static utectx_t u = NULL;
static struct {
	struct sl1t_s bid[1];
	struct sl1t_s ask[1];
	const char *sd;
	size_t sdsz;
} *cache = NULL;
static size_t ncache = 0;

static char *secdefs = NULL;
static size_t secdefs_alsz = 0UL;

static const size_t pgsz = 65536UL;

#if !defined AS_CONST_SL1T
# define AS_CONST_SL1T(x)	((const_sl1t_t)(x))
#endif	/* !AS_CONST_SL1T */

static void
clean_up_secdefs(void)
{
	UMQS_DEBUG("cleaning up secdefs\n");
	munmap(secdefs, secdefs_alsz);
	return;
}

static void
clean_up_cache(void)
{
	size_t nx64k = (ncache * sizeof(*cache) + pgsz) & ~(pgsz - 1);

	UMQS_DEBUG("cleaning up cache\n");
	munmap(cache, nx64k);
	return;
}

static void
check_cache(unsigned int tgtid)
{
	if (UNLIKELY(tgtid > ncache)) {
		/* resize */
		size_t nx64k = (tgtid * sizeof(*cache) + pgsz) & ~(pgsz - 1);

		if (UNLIKELY(cache == NULL)) {
			cache = mmap(NULL, nx64k, PROT_MEM, MAP_MEM, -1, 0);
			atexit(clean_up_cache);
		} else {
			size_t olsz = ncache * sizeof(*cache);
			cache = mremap(cache, olsz, nx64k, MREMAP_MAYMOVE);
		}

		ncache = nx64k / sizeof(*cache);
	}
	return;
}

static ssize_t
massage_fetch_uri_rpl(const char *buf, size_t bsz, uint16_t idx)
{
/* if successful returns 0, -1 if an error occurred and
 * a positive value if the whole contents couldn't fit in BUF. */
	static char hdr_cont_len[] = "Content-Length:";
	static char delim[] = "\r\n\r\n";
	static size_t secdefs_sz = 0UL;
	const char *p;
	ssize_t sz;

	if ((p = strcasestr(buf, hdr_cont_len)) == NULL) {
		return -1;
	} else if ((sz = strtol(p += sizeof(hdr_cont_len) - 1, NULL, 10)) < 0) {
		/* too weird */
		return -1;
	} else if ((p = strstr(p, delim)) == NULL) {
		/* can't find the content in this packet */
		return sz;
	} else if ((size_t)((p += 4) - buf + sz) > bsz) {
		/* pity, the packet is too large to fit */
		return sz;
	}

	/* SZ is the size, p points to the content
	 * AND the whole shebang is in this packet,
	 * time for fireworks innit? */
	if (secdefs_sz + sz + 1 > secdefs_alsz) {
		size_t nx64k = (secdefs_sz + sz + 1 + pgsz) & ~(pgsz - 1);

		if (UNLIKELY(secdefs == NULL)) {
			secdefs = mmap(NULL, nx64k, PROT_MEM, MAP_MEM, -1, 0);
			atexit(clean_up_secdefs);
		} else {
			secdefs = mremap(
				secdefs, secdefs_alsz, nx64k, MREMAP_MAYMOVE);
		}

		secdefs_alsz = nx64k;
	}

	/* also make sure we can bang our stuff into the cache array */
	check_cache(idx);

	memcpy(secdefs + secdefs_sz, p, sz);
	secdefs[secdefs_sz + sz] = '\0';

	/* let our cache know */
	cache[idx - 1].sd = secdefs + secdefs_sz;
	cache[idx - 1].sdsz = sz;

	/* advance the pointer */
	secdefs_sz += sz + 1;
	return 0;
}

static void
bang(unsigned int tgtid, scom_t sp)
{
	if (UNLIKELY(!tgtid)) {
		return;
	}
	/* make sure there's enough room */
	check_cache(tgtid);

	switch (scom_thdr_ttf(sp)) {
	case SL1T_TTF_BID:
		*cache[tgtid - 1].bid = *AS_CONST_SL1T(sp);
		break;
	case SL1T_TTF_ASK:
		*cache[tgtid - 1].ask = *AS_CONST_SL1T(sp);
		break;
	default:
		break;
	}
	return;
}

static void
snarf_meta(job_t j)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct udpc_seria_s ser[1];
	struct key_s k = {
		.sa = &j->sa,
	};
	uint8_t tag;
	struct timeval tv[1];

	/* just to make sure that we're not late for dinner tonight */
	gettimeofday(tv, NULL);

	udpc_seria_init(ser, pbuf, plen);
	while (ser->msgoff < plen && (tag = udpc_seria_tag(ser))) {
		cli_t c;
		uint16_t id;
		int peruse_uri = 0;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		k.id = udpc_seria_des_ui16(ser);
		/* and the cli, if any */
		if ((c = find_cli(k)) == 0) {
			c = add_cli(k);
		}
		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (UNLIKELY(tag != UDPC_TYPE_STR || c == 0)) {
			break;
		}
		/* fuck error checking */
		udpc_seria_des_str_into(CLI(c)->sym, sizeof(CLI(c)->sym), ser);

		/* check if we know about the symbol */
		if ((id = ute_sym2idx(u, CLI(c)->sym)) == CLI(c)->tgtid) {
			/* yep, known and it's the same id, brilliant */
			;
		} else if (CLI(c)->tgtid) {
			/* known but the wrong id */
			UMQS_DEBUG("reass %hu -> %hu\n", CLI(c)->tgtid, id);
			CLI(c)->tgtid = id;
			peruse_uri = 1;
		} else /*if (CLI(c)->tgtid == 0)*/ {
			/* unknown */
			UMQS_DEBUG("ass'ing %s -> %hu\n", CLI(c)->sym, id);
			CLI(c)->tgtid = id;
			ute_bang_symidx(u, CLI(c)->sym, CLI(c)->tgtid);
			peruse_uri = 1;
		}

		/* leave a last_seen note */
		CLI(c)->last_seen = tv->tv_sec;

		/* next up is brag uri, possibly */
		tag = udpc_seria_tag(ser);
		if (LIKELY(tag == UDPC_TYPE_STR)) {
			char uri[256];
			size_t nuri;

			nuri = udpc_seria_des_str_into(uri, sizeof(uri), ser);

			if (peruse_uri) {
				urifi_t fi = make_uri();

				UMQS_DEBUG("checking out %s ...\n", uri);
				memcpy(fi->uri, uri, nuri + 1);
				fi->idx = id;
				push_uri(fi);
			}
		}
	}
	return;
}

static void
snarf_data(job_t j)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct key_s k = {
		.sa = &j->sa,
	};
	struct timeval tv[1];

	if (UNLIKELY(plen == 0)) {
		return;
	}

	/* lest we miss our flight */
	gettimeofday(tv, NULL);

	for (scom_thdr_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		cli_t c;

		k.id = scom_thdr_tblidx(sp);
		if ((c = find_cli(k)) == 0) {
			c = add_cli(k);
		}
		if (CLI(c)->tgtid == 0) {
			continue;
		}

		/* update our state table */
		bang(CLI(c)->tgtid, sp);

		/* leave a last_seen note */
		CLI(c)->last_seen = tv->tv_sec;
	}
	return;
}

static void
rotate_outfile(EV_P)
{
	struct tm tm[1];
	time_t now;

	fprintf(logerr, "rotate...\n");

	/* get a recent time stamp */
	now = time(NULL);
	gmtime_r(&now, tm);
	fprintf(logerr, "%zu syms\n", ute_nsyms(u));
	return;
}


/* web services */
typedef enum {
	WEBSVC_F_UNK,
	WEBSVC_F_SECDEF,
	WEBSVC_F_QUOTREQ,
} websvc_f_t;

struct websvc_s {
	websvc_f_t ty;

	union {
		struct {
			uint16_t idx;
		} secdef;

		struct {
			uint16_t idx;
		} quotreq;
	};
};

/* looks like dccp://host:port/secdef?idx=00000 */
static char brag_uri[INET6_ADDRSTRLEN] = "dccp://";
/* offset into brag_uris idx= field */
static size_t brag_uri_offset = 0;

#define MASS_QUOT	(0xffff)

static int
make_brag_uri(ud_sockaddr_t sa, socklen_t UNUSED(sa_len))
{
	struct utsname uts[1];
	char dnsdom[64];
	const size_t uri_host_offs = sizeof("dccp://");
	char *curs = brag_uri + uri_host_offs - 1;
	size_t rest = sizeof(brag_uri) - uri_host_offs;
	int len;

	if (uname(uts) < 0) {
		return -1;
	} else if (getdomainname(dnsdom, sizeof(dnsdom)) < 0) {
		return -1;
	}

	len = snprintf(
		curs, rest, "%s.%s:%hu/",
		uts->nodename, dnsdom, ntohs(sa->sa6.sin6_port));

	if (len > 0) {
		brag_uri_offset = uri_host_offs + len - 1;
	}

	UMQS_DEBUG("adv_name: %s\n", brag_uri);
	return 0;
}

static uint16_t
__find_idx(const char *str)
{
	if ((str = strstr(str, "idx="))) {
		long int idx = strtol(str + 4, NULL, 10);
		if (idx > 0 && idx < 65536) {
			return (uint16_t)idx;
		}
		return 0;
	}
	return MASS_QUOT;
}

static websvc_f_t
websvc_from_request(struct websvc_s *tgt, const char *req, size_t UNUSED(len))
{
	static const char get_slash[] = "GET /";
	websvc_f_t res = WEBSVC_F_UNK;
	const char *p;

	if ((p = strstr(req, get_slash))) {
		p += sizeof(get_slash) - 1;

#define TAG_MATCHES_P(p, x)				\
		(strncmp(p, x, sizeof(x) - 1) == 0)

#define SECDEF_TAG	"secdef"
		if (TAG_MATCHES_P(p, SECDEF_TAG)) {
			UMQS_DEBUG("secdef query\n");
			res = WEBSVC_F_SECDEF;
			tgt->secdef.idx =
				__find_idx(p + sizeof(SECDEF_TAG) - 1);

#define QUOTREQ_TAG	"quotreq"
		} else if (TAG_MATCHES_P(p, QUOTREQ_TAG)) {
			UMQS_DEBUG("quotreq query\n");
			res = WEBSVC_F_QUOTREQ;
			tgt->quotreq.idx =
				__find_idx(p + sizeof(QUOTREQ_TAG) - 1);
		}
	}
	return tgt->ty = res;
}

static size_t
websvc_secdef(char *restrict tgt, size_t tsz, struct websvc_s sd)
{
	ssize_t res = 0;
	uint16_t idx = sd.secdef.idx;

	UMQS_DEBUG("printing secdef idx %hu\n", sd.secdef.idx);
	if (cache[idx - 1].sd && tsz) {
		res = cache[idx - 1].sdsz;

		if (tsz < (size_t)res) {
			res = tsz - 1;
		}
		memcpy(tgt, cache[idx - 1].sd, res);
		tgt[res] = '\0';
	}
	return res;
}

static size_t
__quotreq1(char *restrict tgt, size_t tsz, uint16_t idx, struct timeval now)
{
	static size_t qid = 0;
	static char bp[16], ap[16], bq[16], aq[16];
	static char vtm[32];
	static char txn[32];
	static struct timeval now_cch;
	const char *sym = ute_idx2sym(u, idx);
	const_sl1t_t b = cache[idx - 1].bid;
	const_sl1t_t a = cache[idx - 1].ask;

	/* find the more recent quote out of bid and ask */
	{
		time_t bs = sl1t_stmp_sec(b);
		unsigned int bms = sl1t_stmp_msec(b);
		time_t as = sl1t_stmp_sec(a);
		unsigned int ams = sl1t_stmp_msec(a);

		if (bs <= as) {
			bs = as;
			bms = ams;
		}
		if (UNLIKELY(bs == 0)) {
			return 0;
		}

		ffff_strfdtu(txn, sizeof(txn), bs, bms * 1000);
	}

	ffff_m30_s(bp, (m30_t)b->pri);
	ffff_m30_s(bq, (m30_t)b->qty);
	ffff_m30_s(ap, (m30_t)a->pri);
	ffff_m30_s(aq, (m30_t)a->qty);

	if (now_cch.tv_sec != now.tv_sec) {
		ffff_strfdtu(vtm, sizeof(vtm), now.tv_sec, now.tv_usec);
		now_cch = now;
	}

	return snprintf(
		tgt, tsz, "\
  <Quot QID=\"%zu\" \
BidPx=\"%s\" OfrPx=\"%s\" BidSz=\"%s\" OfrSz=\"%s\" \
TxnTm=\"%s\" ValidUntilTm=\"%s\">\n\
    <Instrmt ID=\"%hu\" Sym=\"%s\"/>\n\
  </Quot>\n",
		++qid, bp, ap, bq, aq, txn, vtm, idx, sym);
}

static size_t
websvc_quotreq(char *restrict tgt, size_t tsz, struct websvc_s sd)
{
	static const char pre[] = "\
<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\
<FIXML>\n\
";
	static const char post[] = "\
</FIXML>\n\
";
	size_t idx = 0;
	size_t nsy = ute_nsyms(u);
	struct timeval now[1];

	UMQS_DEBUG("printing quotreq idx %hu\n", sd.quotreq.idx);

	if (!sd.quotreq.idx) {
		return 0;
	}

	/* get current time */
	gettimeofday(now, NULL);

	/* copy pre */
	memcpy(tgt + idx, pre, sizeof(pre));
	idx += sizeof(pre) - 1;

	if (sd.quotreq.idx < nsy) {
		idx += __quotreq1(tgt + idx, tsz - idx, sd.quotreq.idx, *now);
	} else if (sd.quotreq.idx == MASS_QUOT) {
		static const char batch_pre[] = "<Batch>\n";
		static const char batch_post[] = "</Batch>\n";

		memcpy(tgt + idx, batch_pre, sizeof(batch_pre));
		idx += sizeof(batch_pre) - 1;
		/* loop over instruments */
		for (size_t i = 1; i <= nsy; i++) {
			idx += __quotreq1(tgt + idx, tsz - idx, i, *now);
		}
		memcpy(tgt + idx, batch_post, sizeof(batch_post));
		idx += sizeof(batch_post) - 1;
	}

	/* copy post */
	memcpy(tgt + idx, post, sizeof(post));
	idx += sizeof(post) - 1;
	return idx;
}

static size_t
websvc_unk(char *restrict tgt, size_t tsz, struct websvc_s UNUSED(sd))
{
	static const char rsp[] = "\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <title>um-quosnp overview</title>\n\
  </head>\n\
\n\
  <body>\n\
    <section>\n\
      <header>\n\
        <h1>Services</h1>\n\
      </header>\n\
\n\
      <footer>\n\
        <hr/>\n\
        <address>\n\
          <a href=\"https://github.com/hroptatyr/unsermarkt/\">unsermarkt</a>\n\
        </address>\n\
      </footer>\n\
    </section>\n\
  </body>\n\
</html>\n\
";
	if (tsz < sizeof(rsp)) {
		return 0;
	}
	memcpy(tgt, rsp, sizeof(rsp));
	return sizeof(rsp) - 1;
}


static int
snarf_uri(char *restrict uri)
{
	struct addrinfo *ais = NULL;
	const char *host;
	char *path;
	char *p;
	uint16_t port;
	int s;

	/* snarf host and path from uri */
	if ((host = strstr(uri, "://")) == NULL) {
		fprintf(logerr, "cannot snarf host part off %s\n", uri);
		return -1;
	} else if ((p = strchr(host += 3, '/')) == NULL) {
		fprintf(logerr, "no path in URI %s\n", uri);
		return -1;
	}
	/* fiddle with the string a bit */
	*p = '\0';
	path = p + 1;

	/* try and find a port number */
	if ((p = strchr(host, ':'))) {
		*p = '\0';
		port = strtoul(p + 1, NULL, 10);
	} else {
		port = 80U;
	}

	/* connection guts */
	if (rslv(&ais, host, port) < 0) {
		fprintf(logerr, "cannot resolve %s %hu\n", host, port);
		return -1;
	} else if ((s = conn(ais)) < 0) {
		fprintf(logerr, "cannot connect to %s %hu\n", host, port);
		return -1;
	}
	/* yay */
	UMQS_DEBUG("d/l'ing /%s off %s %hu\n", path, host, port);
	{
		static char req[256] = "GET /";
		size_t plen = strlen(path);

		memcpy(req + 5, path, plen);
		memcpy(req + 5 + plen, "\r\n\r\n", 5);
		send(s, req, plen + 10, 0);
	}
	return s;
}

static void
check_urifq(EV_P)
{
	static void fetch_data_cb();

	for (urifi_t fi; (fi = pop_uri()); free_uri(fi)) {
		ev_io_i_t qio;
		int s;

		if ((s = snarf_uri(fi->uri)) < 0) {
			continue;
		}

		/* retrieve the result through our queue */
		qio = make_io();
		ev_io_init(qio->w, fetch_data_cb, s, EV_READ);
		qio->idx = fi->idx;
		qio->w->data = qio;
		ev_io_start(EV_A_ qio->w);
	}
	return;
}


#define UTE_LE		(0x7574)
#define UTE_BE		(0x5554)
#define QMETA		(0x7572)
#define QMETA_RPL	(UDPC_PKT_RPL(QMETA))
#if defined WORDS_BIGENDIAN
# define UTE		UTE_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE		UTE_LE
#endif	/* WORDS_BIGENDIAN */
#define UTE_RPL		(UDPC_PKT_RPL(UTE))

/* the actual worker function */
static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nrd;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	j->sock = w->fd;
	nrd = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nrd < 0)) {
		goto out_revok;
	} else if (nrd == 0) {
		/* no need to bother */
		goto out_revok;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, j->buf})) {
		goto out_revok;
	}

	/* preapre a job */
	j->blen = nrd;

	/* intercept special channels */
	switch (udpc_pkt_cmd(JOB_PACKET(j))) {
	case QMETA_RPL:
		snarf_meta(j);
		break;

	case UTE:
	case UTE_RPL:
		snarf_data(j);
		break;
	default:
		break;
	}

out_revok:
	return;
}

/* fdfs */
static void
ev_io_shut(EV_P_ ev_io *w)
{
	int fd = w->fd;

	ev_io_stop(EV_A_ w);
	shutdown(fd, SHUT_RDWR);
	close(fd);
	w->fd = -1;
	return;
}

static void
ev_qio_shut(EV_P_ ev_io *w)
{
/* attention, W *must* come from the ev io queue */
	ev_io_i_t qio = w->data;

	ev_io_shut(EV_A_ w);
	free_io(qio);
	return;
}

static void
fetch_data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	static char rpl[4096];
	ev_io_i_t qio;
	ssize_t nrd;

	if (UNLIKELY((qio = w->data) == NULL)) {
		goto clo;
	} else if ((nrd = read(w->fd, rpl, sizeof(rpl))) < 0) {
		goto clo;
	} else if ((size_t)nrd < sizeof(rpl)) {
		rpl[nrd] = '\0';
	} else {
		/* uh oh, mega request, wtf? */
		rpl[sizeof(rpl) - 1] = '\0';
	}

	/* quick parsing */
	massage_fetch_uri_rpl(rpl, nrd, qio->idx);

clo:
	ev_qio_shut(EV_A_ w);
	return;
}

static void
paste_clen(char *restrict buf, size_t bsz, size_t len)
{
/* print ascii repr of LEN at BUF. */
	buf[0] = ' ';
	buf[1] = ' ';
	buf[2] = ' ';
	buf[3] = ' ';

	if (len > bsz) {
		len = 0;
	}

	buf[4] = (len % 10U) + '0';
	if ((len /= 10U)) {
		buf[3] = (len % 10U) + '0';
		if ((len /= 10U)) {
			buf[2] = (len % 10U) + '0';
			if ((len /= 10U)) {
				buf[1] = (len % 10U) + '0';
				if ((len /= 10U)) {
					buf[0] = (len % 10U) + '0';
				}
			}
		}
	}
	return;
}

static void
dccp_data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	/* the final \n will be subst'd later on */
#define HDR		"\
HTTP/1.1 200 OK\r\n\
Server: um-quosnp\r\n\
Content-Length: "
#define CLEN_SPEC	"% 5zu"
#define BUF_INIT	HDR CLEN_SPEC "\r\n\r\n"
	/* hdr is a format string and hdr_len is as wide as the result printed
	 * later on */
	static char buf[65536] = BUF_INIT;
	char *rsp = buf + sizeof(BUF_INIT) - 1;
	const size_t rsp_len = sizeof(buf) - (sizeof(BUF_INIT) - 1);
	ssize_t nrd;
	size_t cont_len;
	struct websvc_s voodoo;

	if ((nrd = read(w->fd, rsp, rsp_len)) < 0) {
		goto clo;
	} else if ((size_t)nrd < rsp_len) {
		rsp[nrd] = '\0';
	} else {
		/* uh oh, mega request, wtf? */
		buf[sizeof(buf) - 1] = '\0';
	}

	switch (websvc_from_request(&voodoo, rsp, nrd)) {
	default:
	case WEBSVC_F_UNK:
		cont_len = websvc_unk(rsp, rsp_len, voodoo);
		break;

	case WEBSVC_F_SECDEF:
		cont_len = websvc_secdef(rsp, rsp_len, voodoo);
		break;
	case WEBSVC_F_QUOTREQ:
		cont_len = websvc_quotreq(rsp, rsp_len, voodoo);
		break;
	}

	/* prepare the header */
	paste_clen(buf + sizeof(HDR) - 1, sizeof(buf), cont_len);

	/* and append the actual contents */
	send(w->fd, buf, sizeof(BUF_INIT) - 1 + cont_len, 0);

clo:
	ev_qio_shut(EV_A_ w);
	return;
}

static void
dccp_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	union ud_sockaddr_u sa;
	socklen_t sasz = sizeof(sa);
	ev_io_i_t qio;
	int s;

	UMQS_DEBUG("interesting activity on %d\n", w->fd);

	if ((s = accept(w->fd, &sa.sa, &sasz)) < 0) {
		return;
	}

	qio = make_io();
	ev_io_init(qio->w, dccp_data_cb, s, EV_READ);
	qio->w->data = qio;
	ev_io_start(EV_A_ qio->w);
	return;
}

static void
prep_cb(EV_P_ ev_prepare *UNUSED(w), int UNUSED(revents))
{
	/* check the uri fetch queue */
	check_urifq(EV_A);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	rotate_outfile(EV_A);
	return;
}

static void
midnight_cb(EV_P_ ev_periodic *UNUSED(w), int UNUSED(r))
{
	rotate_outfile(EV_A);
	return;
}

static void
prune_cb(EV_P_ ev_timer *w, int UNUSED(r))
{
	prune_clis();
	ev_timer_again(EV_A_ w);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "um-quosnp-clo.h"
#include "um-quosnp-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

static pid_t
detach(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		/* i am the parent */
		UMQS_DEBUG("daemonisation successful %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return -1;
	}
	/* close standard tty descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	/* reattach them to /dev/null */
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
	}
#if defined DEBUG_FLAG
	logerr = fopen("/tmp/um-quosnp.log", "w");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* args */
	struct umqd_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_periodic midnight[1];
	ev_prepare prp[1];
	ev_timer prune[1];
	ev_io dccp[2];
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (umqd_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sigint_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* the midnight tick for file rotation, also upon sighup */
	ev_periodic_init(midnight, midnight_cb, MIDNIGHT, ONE_DAY, NULL);
	ev_periodic_start(EV_A_ midnight);

	/* prune timer, check occasionally for old unused clients */
	ev_timer_init(prune, prune_cb, PRUNE_INTV, PRUNE_INTV);
	ev_timer_start(EV_A_ prune);

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	{
		int s = ud_mcast_init(UD_NETWORK_SERVICE);
		ev_io_init(beef, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* make a channel for http/dccp requests */
	{
		union ud_sockaddr_u sa = {
			.sa6 = {
				.sin6_family = AF_INET6,
				.sin6_addr = in6addr_any,
				.sin6_port = 0,
			},
		};
		socklen_t sa_len = sizeof(sa);
		int s;

		if ((s = make_dccp()) < 0) {
			/* just to indicate we have no socket */
			dccp[0].fd = -1;
		} else if (sock_listener(s, &sa) < 0) {
			/* grrr, whats wrong now */
			close(s);
			dccp[0].fd = -1;
		} else {
			/* everything's brilliant */
			ev_io_init(dccp, dccp_cb, s, EV_READ);
			ev_io_start(EV_A_ dccp);

			getsockname(s, &sa.sa, &sa_len);
		}


		if (countof(dccp) < 2) {
			;
		} else if ((s = make_tcp()) < 0) {
			/* just to indicate we have no socket */
			dccp[1].fd = -1;
		} else if (sock_listener(s, &sa) < 0) {
			/* bugger */
			close(s);
			dccp[1].fd = -1;
		} else {
			/* yay */
			ev_io_init(dccp + 1, dccp_cb, s, EV_READ);
			ev_io_start(EV_A_ dccp + 1);

			getsockname(s, &sa.sa, &sa_len);
		}

		if (s >= 0) {
			make_brag_uri(&sa, sa_len);

			fwrite(brag_uri, 1, brag_uri_offset, stdout);
			fputc('\n', stdout);
		}
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		int s = ud_mcast_init(argi->beef_arg[i]);
		ev_io_init(beef + i + 1, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	/* init cli space */
	init_cli();

	/* init helper ute, we're not storing tick data in this file */
	if ((u = ute_mktemp(UO_ANON | UO_RDWR)) == NULL) {
		res = 1;
		goto past_loop;
	}

	if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto past_loop;
	}

	/* pre and post poll hooks */
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
		ud_mcast_fini(s);
	}
	/* free beef resources */
	free(beef);

	/* finish cli space */
	fini_cli();

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	umqd_parser_free(argi);

	/* unloop was called, so exit */
	return res;
}

/* um-quosnp.c ends here */
