/*** um-quod.c -- unsermarkt quote daemon
 *
 * Copyright (C) 2012-2013 Sebastian Freundt
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
#endif	/* HAVE_SYS_SOCKET_H */
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif	/* HAVE_ARPA_INET_H */
#if defined HAVE_NETDB_H
# include <netdb.h>
#endif	/* HAVE_NETDB_H */
#if defined HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif	/* HAVE_SYS_UTSNAME_H */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif	/* HAVE_ERRNO_H */
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#if defined HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif	/* HAVE_SYS_MMAN_H */

#include <unserding/unserding.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#if defined HAVE_LIBFIXC_FIX_H
# include <libfixc/fix.h>
# include <libfixc/fixml-msg.h>
# include <libfixc/fixml-comp.h>
# include <libfixc/fixml-attr.h>
#endif	/* HAVE_LIBFIXC_FIX_H */

#include "um-quod.h"
#include "svc-uterus.h"
#include "nifty.h"
#include "ud-sock.h"
#include "gq.h"
#include "web.h"
#include "quod-cache.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

/* maximum allowed age for clients (in seconds) */
#if defined DEBUG_FLAG
# define MAX_CLI_AGE	(60)
# define PRUNE_INTV	(10.0)
#else  /* !DEBUG_FLAG */
# define MAX_CLI_AGE	(1800)
# define PRUNE_INTV	(60.0)
#endif	/* DEBUG_FLAG */

/* exposed to sub systems (like web.c) */
void *logerr;
#if defined DEBUG_FLAG
# define UMQD_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define UMQD_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef size_t cli_t;
typedef intptr_t hx_t;

typedef const struct sockaddr_in6 *my_sockaddr_t;

typedef struct urifq_s *urifq_t;
typedef struct urifi_s *urifi_t;

typedef struct ev_io_q_s *ev_io_q_t;
typedef struct ev_io_i_s *ev_io_i_t;

struct key_s {
	my_sockaddr_t sa;
	uint16_t id;
};

/* the client */
struct cli_s {
	struct sockaddr_storage sa __attribute__((aligned(16)));
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

/* default ute open mode */
#if defined UO_STREAM
static const int ute_oflags = UO_RDWR | UO_CREAT | UO_STREAM;
#else  /* !UO_STREAM */
static const int ute_oflags = UO_RDWR | UO_CREAT;
#endif	/* UO_STREAM */


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
sa_eq_p(my_sockaddr_t sa1, my_sockaddr_t sa2)
{
#define s6a8(x)		(x)->sin6_addr.s6_addr8
#define s6a16(x)	(x)->sin6_addr.s6_addr16
#define s6a32(x)	(x)->sin6_addr.s6_addr32
#if SIZEOF_INT == 4
# define s6a		s6a32
#elif SIZEOF_INT == 2
# define s6a		s6a16
#else
# error sockaddr comparison will fail
#endif	/* sizeof(long int) */
	return sa1->sin6_family == sa2->sin6_family &&
		sa1->sin6_port == sa2->sin6_port &&
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
	return k.sa->sin6_family ^ k.sa->sin6_port ^
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

	if (UNLIKELY(k.sa->sin6_family != AF_INET6)) {
		return 0U;
	}
	for (size_t i = 0; i < ncli; i++) {
		if (chx[i] == khx &&
		    cli[i].id == k.id &&
		    sa_eq_p((my_sockaddr_t)&cli[i].sa, k.sa)) {
			return i + 1;
		}
	}
	return 0U;
}

static cli_t
add_cli(struct key_s k)
{
	cli_t idx = ncli++;

	if (ncli * sizeof(*cli) > alloc_cli) {
		resz_cli(alloc_cli + 4096);
	}

	cli[idx].sa = *(const struct sockaddr_storage*)k.sa;
	cli[idx].id = k.id;
	cli[idx].tgtid = 0;
	cli[idx].last_seen = 0;
	chx[idx] = compute_hx(k);
	return idx + 1;
}

static void
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

static void
prune_clis(void)
{
	struct timeval tv[1];
	size_t nu_ncli = ncli;

	/* what's the time? */
	gettimeofday(tv, NULL);

	/* prune clis */
	for (cli_t i = 1; i <= ncli; i++) {
		if (CLI(i)->last_seen + MAX_CLI_AGE < tv->tv_sec) {
			UMQD_DEBUG("pruning %zu\n", i);
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

			UMQD_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			memcpy(CLI(i - consec), CLI(i), nmv * sizeof(*cli));
			nu_ncli -= consec;
		} else if (consec) {
			UMQD_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
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
sock_listener(int s, my_sockaddr_t sa)
{
	if (s < 0) {
		return s;
	}

	if (bind(s, (const struct sockaddr*)sa, sizeof(*sa)) < 0) {
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


/* uri fetch queue */
static struct urifq_s urifq = {0};

static urifi_t
make_uri(void)
{
	urifi_t res;

	if (urifq.q->free->i1st == NULL) {
		assert(urifq.q->free->ilst == NULL);
		UMQD_DEBUG("URIFQ RESIZE +%u\n", 16U);
		init_gq(urifq.q, 16, sizeof(*res));
		UMQD_DEBUG("URIFQ RESIZE ->%zu\n",
			   urifq.q->nitems / sizeof(*res));
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
		assert(ioq.q->free->ilst == NULL);
		UMQD_DEBUG("IOQ RESIZE +%u\n", 16U);
		init_gq(ioq.q, 16U, sizeof(*res));
		UMQD_DEBUG("IOQ RESIZE ->%zu\n", ioq.q->nitems / sizeof(*res));
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


utectx_t uctx = NULL;
static const char *u_fn = NULL;
static size_t u_nt = 0;
/* number of ticks ignored due to missing symbols */
static size_t ign = 0;

/* value cache */
quod_cache_t quod_cache = NULL;
static size_t cache_alsz = 0UL;
#define CACHE(x)	(quod_cache[x])

static const size_t pgsz = 65536UL;

#if !defined HAVE_LIBFIXC_FIX_H
static char *secdefs = NULL;
static size_t secdefs_alsz = 0UL;

static void
clean_up_secdefs(void)
{
	UMQD_DEBUG("cleaning up secdefs\n");
	munmap(secdefs, secdefs_alsz);
	return;
}
#endif	/* HAVE_LIBFIXC_FIX_H */

static void
clean_up_cache(void)
{
	UMQD_DEBUG("cleaning up cache\n");
	munmap(quod_cache, cache_alsz);
	return;
}

static void
check_cache(unsigned int tgtid)
{
	static size_t ncache = 0;

	if (UNLIKELY(tgtid > ncache)) {
		/* resize */
		size_t nx64k = (tgtid * sizeof(CACHE(0)) + pgsz) & ~(pgsz - 1);

		if (UNLIKELY(quod_cache == NULL)) {
			quod_cache = mmap(NULL, nx64k, PROT_MEM, MAP_MEM, -1, 0);
			atexit(clean_up_cache);
		} else {
			size_t olsz = ncache * sizeof(CACHE(0));
			quod_cache =
				mremap(quod_cache, olsz, nx64k, MREMAP_MAYMOVE);
		}

		ncache = (cache_alsz = nx64k) / sizeof(CACHE(0));
	}
	return;
}

static void
bang_q(unsigned int tgtid, scom_t sp)
{
	if (UNLIKELY(!tgtid)) {
		return;
	}
	/* make sure there's enough room */
	check_cache(tgtid);

	switch (scom_thdr_ttf(sp)) {
	case SL1T_TTF_BID:
		*CACHE(tgtid - 1).bid = *AS_CONST_SL1T(sp);
		break;
	case SL1T_TTF_ASK:
		*CACHE(tgtid - 1).ask = *AS_CONST_SL1T(sp);
		break;
	default:
		break;
	}
	return;
}

#if defined HAVE_LIBFIXC_FIX_H
static void
bang_aid(fixc_msg_t ins, uint16_t idx)
{
	static const struct fixc_fld_s f454 = {
		.tag = (fixc_attr_t)FIXML_ATTR_NoSecurityAltID,
		.typ = FIXC_TYP_INT,
		.tpc = (fixc_comp_t)FIXML_COMP_SecAltIDGrp,
		.cnt = 0,
		.i32 = 1,
	};
	static char sidx[32];
	size_t i454 = 0;
	size_t sz;

	for (size_t i = 1; i < ins->nflds; i++) {
		if (ins->flds[i].tag == 454U) {
			i454 = i;
			break;
		}
	}
	if (!i454) {
		fixc_add_fld(ins, f454);
	}
	/* just bang one more */
	sz = snprintf(sidx, sizeof(sidx), "%hu", idx);
	fixc_add_tag(ins, (fixc_attr_t)FIXML_ATTR_SecurityAltID, sidx, sz);
	/* we know the actual tpc */
	ins->flds[ins->nflds - 1].tpc = FIXML_COMP_SecAltIDGrp;
	ins->flds[ins->nflds - 1].cnt = 0U;

	fixc_add_tag(
		ins, (fixc_attr_t)FIXML_ATTR_SecurityAltIDSource, "100", 3U);
	/* we know the actual tpc */
	ins->flds[ins->nflds - 1].tpc = FIXML_COMP_SecAltIDGrp;
	ins->flds[ins->nflds - 1].cnt = 1U;
	return;
}

static void
bang_sd(fixc_msg_t msg, uint16_t idx)
{
	fixc_msg_t ins;

	/* also make sure we can bang our stuff into the cache array */
	check_cache(idx);

	/* free former resources */
	if (CACHE(idx - 1).ins != NULL) {
		free_fixc(CACHE(idx - 1).ins);
	}
	if (CACHE(idx - 1).msg != NULL) {
		free_fixc(CACHE(idx - 1).msg);
	}

	/* get the instrument bit for later reuse */
	ins = fixc_extr_ctxt_deep(msg, FIXML_COMP_Instrument, 0);

	/* let our cache know */
	CACHE(idx - 1).msg = msg;
	CACHE(idx - 1).ins = ins;

	/* make sure our AIDs (455/456) go on there as well */
	bang_aid(ins, idx);
	return;
}
#else  /* !HAVE_LIBFIXC_FIX_H */
static void
bang_sd(const char *sd, size_t sdsz, uint16_t idx)
{
	static char aid[] = "<AID AltId=\"\"       AltIdSrc=\"100\"/>";
	static size_t secdefs_sz = 0UL;
	size_t post_sz = secdefs_sz + sdsz;
	const char *ins;
	size_t insz;

	if (post_sz + sizeof(aid) > secdefs_alsz) {
		size_t nx64k = (post_sz + sizeof(aid) + pgsz) & ~(pgsz - 1);

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

	memcpy(secdefs + secdefs_sz, sd, sdsz);
	secdefs[secdefs_sz + sdsz] = '\0';
	/* reassign sd */
	sd = secdefs + secdefs_sz;

	/* also try and snarf the Instrmt bit */
	if ((ins = strstr(sd, "<Instrmt"))) {
		static char fixml_instrmt_post[] = "</Instrmt>";
		size_t aidz = sizeof(aid) - 1;
		char *eoins;
		int post = 0;

		if ((eoins = strstr(ins, fixml_instrmt_post))) {
			;
		} else if ((eoins = strstr(ins, "/>"))) {
			*eoins = '>';
			eoins += 1;
			aidz += sizeof(fixml_instrmt_post) - 1;
			post = 1;
		} else {
			/* fubar'd */
			return;
		}

		memmove(eoins + aidz, eoins, sd + sdsz - eoins + 1/*for \nul*/);

		/* glue in our AID */
		memcpy(eoins, aid, aidz);
		{
			int len = snprintf(eoins + 12, 5, "%hu", idx);
			eoins[12 + len] = '"';
		}

		/* update the total secdef size */
		sdsz += aidz;

		if (post) {
			/* expand our Instrmt element */
			size_t postz = sizeof(fixml_instrmt_post) - 1;

			aidz -= postz;
			memcpy(eoins + aidz, fixml_instrmt_post, postz);
		}

		aidz += sizeof(fixml_instrmt_post) - 1;
		insz = eoins + aidz - ins;
	}

	/* let our cache know */
	CACHE(idx - 1).sd = sd;
	CACHE(idx - 1).sdsz = sdsz;
	CACHE(idx - 1).instrmt = ins;
	CACHE(idx - 1).instrmtsz = insz;

	/* advance the pointer, +1 for \nul */
	secdefs_sz += sdsz + 1;
	return;
}
#endif	/* !HAVE_LIBFIXC_FIX_H */

static ssize_t
massage_fetch_uri_rpl(const char *buf, size_t bsz, uint16_t idx)
{
/* if successful returns 0, -1 if an error occurred and
 * a positive value if the whole contents couldn't fit in BUF. */
	static char hdr_cont_len[] = "Content-Length:";
	static char delim[] = "\r\n\r\n";
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

#if defined HAVE_LIBFIXC_FIX_H
	/* P should now point to the content of size SZ */
	{
		fixc_msg_t msg;

		if (LIKELY((msg = make_fixc_from_fixml(p, sz)) != NULL)) {
			bang_sd(msg, idx);
		}
	}
#else  /* !HAVE_LIBFIXC_FIX_H */
	/* SZ is the size, p points to the content
	 * AND the whole shebang is in this packet,
	 * time for fireworks innit? */
	/* check for <SecDef> */
	{
		const char *sd;
		const char *eosd;

		if ((sd = strstr(p, "<SecDef")) == NULL ||
		    (eosd = strstr(sd, "</SecDef>")) == NULL) {
			return 0;
		}
		/* make eosd point to behind </SecDef> */
		eosd += 9;
		/* recompute SZ */
		bang_sd(sd, eosd - sd, idx);
	}
#endif	/* HAVE_LIBFIXC_FIX_H */
	return 0;
}

static void
bang_sym(cli_t c, const struct um_qmeta_s qm[static 1])
{
	size_t slen;

	if (UNLIKELY((slen = qm->symlen) > sizeof(CLI(c)->sym))) {
		slen = sizeof(CLI(c)->sym) - 1;
	}
	/* fill in symbol */
	memcpy(CLI(c)->sym, qm->sym, slen);
	CLI(c)->sym[slen] = '\0';
	return;
}

static void
snarf_meta(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
	struct um_qmeta_s brg[1];
	struct key_s k = {
		.sa = (const void*)aux->src,
	};
	int peruse_uri = 0;
	unsigned int id;
	cli_t c;

	if (UNLIKELY(um_chck_brag(brg, msg) < 0)) {
		return;
	} else if (UNLIKELY((k.id = (uint16_t)brg->idx) == 0U)) {
		return;
	} else if (UNLIKELY(brg->sym == NULL)) {
		return;
	}

	/* find the cli, if any */
	if ((c = find_cli(k)) == 0) {
		c = add_cli(k);
	}
	/* check the symbol */
	bang_sym(c, brg);

	/* check if we know about the symbol */
	if ((id = ute_sym2idx(uctx, CLI(c)->sym)) == CLI(c)->tgtid) {
		/* yep, known and it's the same id, brilliant */
		;
	} else if (CLI(c)->tgtid) {
		/* known but the wrong id */
		UMQD_DEBUG("reass %hu -> %u\n", CLI(c)->tgtid, id);
		CLI(c)->tgtid = (uint16_t)id;
		peruse_uri = 1;
	} else /*if (CLI(c)->tgtid == 0)*/ {
		/* unknown */
		UMQD_DEBUG("ass'ing %s -> %u\n", CLI(c)->sym, id);
		CLI(c)->tgtid = (uint16_t)id;
		ute_bang_symidx(uctx, CLI(c)->sym, CLI(c)->tgtid);
		peruse_uri = 1;
	}

	/* leave a last_seen note */
	{
		struct timeval tv[1];
		gettimeofday(tv, NULL);
		CLI(c)->last_seen = tv->tv_sec;
	}

	/* next up is brag uri, possibly */
	if (peruse_uri && brg->uri != NULL) {
		urifi_t fi = make_uri();

		memcpy(fi->uri, brg->uri, brg->urilen);
		fi->uri[brg->urilen] = '\0';
		fi->idx = (uint16_t)id;
		UMQD_DEBUG("checking out %s ...\n", fi->uri);
		push_uri(fi);
	}
	return;
}

static void
snarf_data(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
	struct sndwch_s ss[4];
	cli_t c;

	/* check how valid the message is */
	switch (msg->dlen) {
	case 1 * sizeof(*ss):
	case 2 * sizeof(*ss):
	case 4 * sizeof(*ss):
		memcpy(ss, msg->data, msg->dlen);
		break;
	default:
		/* out of range */
		return;
	}

	/* find the cli associated */
	{
		struct key_s k = {
			.id = (uint16_t)scom_thdr_tblidx(AS_SCOM(msg->data)),
			.sa = (my_sockaddr_t)aux->src,
		};
		if ((c = find_cli(k)) == 0) {
			c = add_cli(k);
		}
		if (CLI(c)->tgtid == 0) {
			/* don't do shit without a name */
			ign++;
			return;
		}
	}

	/* update our state table */
	bang_q(CLI(c)->tgtid, msg->data);

	/* fiddle with the tblidx */
	scom_thdr_set_tblidx(AS_SCOM_THDR(ss), CLI(c)->tgtid);
	/* and pump the tick to ute */
	ute_add_tick(uctx, AS_SCOM(ss));

	/* leave a last_seen note */
	{
		struct timeval tv[1];
		gettimeofday(tv, NULL);
		CLI(c)->last_seen = tv->tv_sec;
	}
	return;
}

static void
rotate_outfile(EV_P)
{
	struct tm tm[1];
	static char nu_fn[256];
	char *n = nu_fn;
	time_t now;

	fprintf(logerr, "rotate...\n");

	/* get a recent time stamp */
	now = time(NULL);
	gmtime_r(&now, tm);
	strncpy(n, u_fn, sizeof(nu_fn));
	n += strlen(u_fn);
	*n++ = '-';
	strftime(n, sizeof(nu_fn) - (n - nu_fn), "%Y-%m-%dT%H:%M:%S.ute\0", tm);
#if defined HAVE_UTE_FREE
	ute_set_fn(uctx, nu_fn);
#else  /* !HAVE_UTE_FREE */
	/* the next best thing */
	rename(u_fn, nu_fn);
#endif	/* HAVE_UTE_FREE */

	/* magic */
	switch (fork()) {
	case -1:
		fprintf(logerr, "cannot fork :O\n");
		return;

	default: {
		/* i am the parent */
		utectx_t nu = ute_open(u_fn, ute_oflags | UO_TRUNC);
		ign = 0;
		ute_clone_slut(nu, uctx);

		/* free resources */
		ute_free(uctx);
		/* nu u is nu */
		uctx = nu;
		return;
	}

	case 0:
		/* i am the child, just update the file name and nticks
		 * and let unroll do the work */
		u_fn = nu_fn;
		/* let libev know we're a fork */
		ev_loop_fork(EV_DEFAULT);

		/* close everything but the ute file */
		for (size_t i = 0; i < nbeef; i++) {
			int fd = beef[i].fd;

			/* just close the descriptor, as opposed to
			 * calling ud_mcast_fini() on it */
			ev_io_stop(EV_A_ beef + i);
			close(fd);
		}
		/* pretend we're through with the beef */
		nbeef = 0;

		/* then exit */
		ev_unloop(EV_A_ EVUNLOOP_ALL);
		return;
	}
	/* not reached */
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

/* looks like dccp://host:port/secdef?idx=00000 */
static char brag_uri[INET6_ADDRSTRLEN] = "http://";

static int
make_brag_uri(my_sockaddr_t sa, socklen_t UNUSED(sa_len))
{
	struct utsname uts[1];
	char dnsdom[64];
	const size_t uri_host_offs = sizeof("dccp://");
	char *curs = brag_uri + uri_host_offs - 1;
	size_t rest = sizeof(brag_uri) - uri_host_offs;

	if (uname(uts) < 0) {
		return -1;
	} else if (getdomainname(dnsdom, sizeof(dnsdom)) < 0) {
		return -1;
	}

	(void)snprintf(
		curs, rest, "%s.%s:%hu/",
		uts->nodename, dnsdom, ntohs(sa->sin6_port));

	UMQD_DEBUG("adv_name: %s\n", brag_uri);
	return 0;
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
	UMQD_DEBUG("d/l'ing /%s off %s %hu\n", path, host, port);
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
check_urifq(EV_P)
{
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
	struct ud_msg_s msg[1];
	ud_sock_t s = w->data;

	while (ud_chck_msg(msg, s) >= 0) {
		struct ud_auxmsg_s aux[1];

		if (ud_get_aux(aux, s) < 0) {
			continue;
		}

		switch (msg->svc) {
		case UTE_QMETA:
			snarf_meta(msg, aux);
			break;

		case UTE_CMD:
			snarf_data(msg, aux);
			break;
		default:
			break;
		}
	}
	return;
}

static void
dccp_data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	static char buf[65536];
	struct websvc_s ws;
	struct webrsp_s wr;
	ssize_t nrd;
	size_t nwr;

	if ((nrd = read(w->fd, buf, sizeof(buf))) < 0) {
		goto clo;
	} else if ((size_t)nrd < sizeof(buf)) {
		buf[nrd] = '\0';
	} else {
		/* uh oh, mega request, wtf? */
		buf[sizeof(buf) - 1] = '\0';
	}

	ws = websvc(buf, (size_t)nrd);
	wr = web(ws);

	/* send header */
	nwr = 0;
	for (ssize_t tmp;
	     (tmp = send(w->fd, wr.hdr + nwr, wr.hdz - nwr, 0)) > 0 &&
		     (nwr += tmp) < wr.hdz;);

	/* send beef */
	nwr = 0;
	for (ssize_t tmp;
	     (tmp = send(w->fd, wr.cnt + nwr, wr.cnz - nwr, 0)) > 0 &&
		     (nwr += tmp) < wr.cnz;);

	/* free resources */
	free_webrsp(wr);
clo:
	ev_qio_shut(EV_A_ w);
	return;
}

static void
dccp_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	struct sockaddr_storage sa[1];
	socklen_t sz = sizeof(*sa);
	ev_io_i_t qio;
	int s;

	UMQD_DEBUG("interesting activity on %d\n", w->fd);

	if ((s = accept(w->fd, (struct sockaddr*)sa, &sz)) < 0) {
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


#include "um-quod.yucc"

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
		UMQD_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/um-quodmp.log", "w");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_periodic midnight[1];
	ev_prepare prp[1];
	ev_timer prune[1];
	ev_io dccp[2];
	int rc = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
		exit(1);
	}

	if (argi->output_arg && argi->into_arg) {
		fputs("only one of --output and --into can be given\n", logerr);
		rc = 1;
		goto out;
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
	nbeef = argi->beef_nargs + 1U;
	beef = malloc(nbeef * sizeof(*beef));

	/* attach a multicast listener for control messages */
	{
		ud_sock_t s;

		if ((s = ud_socket((struct ud_sockopt_s){UD_SUB})) != NULL) {
			ev_io_init(beef, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef);
		}
		beef->data = s;
	}

	/* go through all beef channels */
	for (size_t i = 0U; i < argi->beef_nargs; i++) {
		char *p;
		long unsigned int port = strtoul(argi->beef_args[i], &p, 0);

		if (UNLIKELY(!port || *p)) {
			/* garbled input */
			continue;
		}

		struct ud_sockopt_s opt = {
			UD_SUB,
			.port = (uint16_t)port,
		};
		ud_sock_t s;

		if (LIKELY((s = ud_socket(opt)) != NULL)) {
			ev_io_init(beef + i + 1, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef + i + 1);
		}
		beef[i + 1].data = s;
	}

	/* make a channel for http/dccp requests */
	if (argi->websvc_port_arg) {
		long unsigned int p = strtoul(argi->websvc_port_arg, NULL, 0);
		struct sockaddr_in6 sa = {
			.sin6_family = AF_INET6,
			.sin6_addr = in6addr_any,
			.sin6_port = htons((short unsigned int)p),
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

			getsockname(s, (struct sockaddr*)&sa, &sa_len);
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

			getsockname(s, (struct sockaddr*)&sa, &sa_len);
		}

		if (s >= 0) {
			make_brag_uri(&sa, sa_len);

			printf("%s\n", brag_uri);
		}
	}

	/* init cli space */
	init_cli();

	/* init ute */
	if (!argi->output_arg && !argi->into_arg) {
		if ((uctx = ute_mktemp(ute_oflags)) == NULL) {
			rc = 1;
			goto past_ute;
		}
		u_fn = strdup(ute_fn(uctx));
	} else {
		int u_fl = ute_oflags;

		if (argi->output_arg) {
			u_fn = argi->output_arg;
			u_fl |= UO_TRUNC;
		} else if (argi->into_arg) {
			u_fn = argi->into_arg;
		}
		if ((uctx = ute_open(u_fn, u_fl)) == NULL) {
			rc = 1;
			goto past_ute;
		}
	}

	if (argi->daemonise_flag && detach() < 0) {
		perror("daemonisation failed");
		rc = 1;
		goto past_loop;
	}

	/* pre and post poll hooks */
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	/* close the file, might take a while due to sorting */
	if (uctx) {
		/* get the number of ticks */
		u_nt = ute_nticks(uctx);
		/* deal with the file */
		ute_close(uctx);
	}
	/* print name and stats */
	fprintf(logerr, "dumped %zu ticks, %zu ignored\n", u_nt, ign);
	fputs(u_fn, stdout);
	fputc('\n', stdout);

past_ute:
	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_sock_t s;

		if ((s = beef[i].data) != NULL) {
			ev_io_stop(EV_A_ beef + i);
			ud_close(s);
		}
	}
	/* free beef resources */
	free(beef);

	/* finish cli space */
	fini_cli();

	/* destroy the default evloop */
	ev_default_destroy();

out:
	/* kick the config context */
	yuck_free(argi);

	/* unloop was called, so exit */
	return rc;
}

/* um-quod.c ends here */
