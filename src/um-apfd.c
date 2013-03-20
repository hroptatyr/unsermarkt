/*** um-apfd.c -- unsermarkt quote daemon
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
#include <string.h>
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

#define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
# include <uterus/m62.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
# include <m62.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#if !defined SL1T_TTF_G64
# define SL1T_TTF_G64		(15)
#endif	/* !SL1T_TTF_G64 */

#if defined HAVE_LIBFIXC_FIX_H
# include <libfixc/fix.h>
# include <libfixc/fixml-msg.h>
# include <libfixc/fixml-comp.h>
# include <libfixc/fixml-attr.h>
#endif	/* HAVE_LIBFIXC_FIX_H */

#include "um-apfd.h"
#include "nifty.h"
#include "ud-sock.h"
#include "gq.h"
#include "web.h"
#include "apfd-cache.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
# pragma warning (disable:2405)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

#define ALGN(x, to)	x __attribute__((aligned(to)))

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

/* maximum allowed age for clients (in seconds) */
#if defined DEBUG_FLAG
# define MAX_CLI_AGE	(600U)
# define PRUNE_INTV	(100U)
#else  /* !DEBUG_FLAG */
# define MAX_CLI_AGE	(18000U)
# define PRUNE_INTV	(600U)
#endif	/* DEBUG_FLAG */

/* exposed to sub systems (like web.c) */
void *logerr;
#if defined DEBUG_FLAG
# define UMAD_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define UMAD_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef size_t cli_t;

typedef const struct sockaddr_in6 *my_sockaddr_t;

struct key_s {
	const struct sockaddr *sa;
};

typedef struct urifq_s *urifq_t;
typedef struct urifi_s *urifi_t;

typedef struct ev_io_q_s *ev_io_q_t;
typedef struct ev_io_i_s *ev_io_i_t;

/* the client */
struct cli_s {
	struct sockaddr_storage sa __attribute__((aligned(16)));

	/* helpers for the renderer */
	char ss[INET6_ADDRSTRLEN + 2 + 6];
	size_t sssz;

	volatile uint32_t last_seen;
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

struct pos_s {
	double lqty;
	double sqty;

	unsigned int last_seen;
};

/* children need access to beef resources */
static ev_io *beef = NULL;
static size_t nbeef = 0;


/* cli handling */
static struct cli_s *cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)

static void
init_cli(void)
{
	/* and our client list */
	cli = mmap(NULL, 4096U, PROT_MEM, MAP_MEM, -1, 0);
	alloc_cli = 4096U;
	return;
}

static void
fini_cli(void)
{
	/* and the list of clients */
	munmap(cli, alloc_cli);
	return;
}

#if !defined HAVE_LIBFIXC_FIX_H
static void
resz_cli(size_t nu)
{
	cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
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

static cli_t
find_cli(struct key_s k)
{
	if (UNLIKELY(k.sa->sa_family != AF_INET6)) {
		return 0U;
	}
	for (size_t i = 0; i < ncli; i++) {
		struct cli_s *c = cli + i;
		my_sockaddr_t cur_sa = (const void*)&c->sa;

		if (sa_eq_p(cur_sa, (my_sockaddr_t)k.sa)) {
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

	/* obtain the address in human readable form */
	{
		char *epi = cli[idx].ss;
		my_sockaddr_t mysa = (const void*)k.sa;
		int fam = mysa->sin6_family;
		uint16_t port = htons(mysa->sin6_port);

		*epi++ = '[';
		if (inet_ntop(fam, k.sa, epi, sizeof(cli->ss))) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
		cli[idx].sssz = epi - cli[idx].ss;
	}

	/* queue needs no init'ing, we use lazy adding */
	return idx + 1;
}
#endif	/* !HAVE_LIBFIXC_FIX_H */

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
	return CLI(c)->sa.ss_family;
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
			UMAD_DEBUG("pruning %zu\n", i);
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

			UMAD_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			memcpy(CLI(i - consec), CLI(i), nmv * sizeof(*cli));
			nu_ncli -= consec;
		} else if (consec) {
			UMAD_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
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


/* ev io object queue */
static struct ev_io_q_s ioq = {0};

static ev_io_i_t
make_io(void)
{
	ev_io_i_t res;

	if (ioq.q->free->i1st == NULL) {
		assert(ioq.q->free->ilst == NULL);
		UMAD_DEBUG("IOQ RESIZE +%u\n", 16U);
		init_gq(ioq.q, 16U, sizeof(*res));
		UMAD_DEBUG("IOQ RESIZE ->%zu\n", ioq.q->nitems / sizeof(*res));
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


/* fix guts */
#if !defined HAVE_LIBFIXC_FIX_H
static size_t
find_fix_fld(const char **p, const char *msg, const char *key)
{
#define SOH	"\001"
	const char *cand = msg - 1;
	char *eocand;

	while ((cand = strstr(cand + 1, key)) &&
	       cand != msg && cand[-1] != *SOH);
	/* cand should be either NULL or point to the key */
	if (UNLIKELY(cand == NULL)) {
		return 0;
	}
	/* search for the next SOH */
	if (UNLIKELY((eocand = strchr(cand, *SOH)) == NULL)) {
		return 0;
	}
	*p = cand;
	return eocand - cand;
}

static const char*
find_fix_eofld(const char *msg, const char *key)
{
#define SOH	"\001"
	const char *cand;
	size_t clen;

	if (UNLIKELY((clen = find_fix_fld(&cand, msg, key)) == 0)) {
		return NULL;
	} else if (UNLIKELY(cand[++clen] == '\0')) {
		return NULL;
	}
	return cand + clen;
}
#endif	/* !HAVE_LIBFIXC_FIX_H */


/* allocation cache */
apfd_cache_t apfd_cache = NULL;
static size_t cache_alsz = 0UL;
#define CACHE(x)	(apfd_cache[x])
/* backing ute file */
utectx_t uctx = NULL;
static const char *u_fn = NULL;
static size_t u_nt = 0;
/* number of ticks ignored due to missing symbols */
static size_t ign = 0;

static void
clean_up_cache(void)
{
	UMAD_DEBUG("cleaning up cache\n");
	munmap(apfd_cache, cache_alsz);
	return;
}

static void
check_cache(unsigned int tgtid)
{
	static const size_t pgsz = 4096U;
	static size_t ncache = 0;

	if (UNLIKELY(tgtid > ncache)) {
		/* resize */
		size_t nx64k = (tgtid * sizeof(CACHE(0)) + pgsz) & ~(pgsz - 1);

		if (UNLIKELY(apfd_cache == NULL)) {
			apfd_cache = mmap(NULL, nx64k, PROT_MEM, MAP_MEM, -1, 0);
			atexit(clean_up_cache);
		} else {
			size_t olsz = ncache * sizeof(CACHE(0));
			apfd_cache =
				mremap(apfd_cache, olsz, nx64k, MREMAP_MAYMOVE);
		}

		ncache = (cache_alsz = nx64k) / sizeof(CACHE(0));
	}
	return;
}

#if defined HAVE_LIBFIXC_FIX_H
static int
snarf_pos_rpt(const struct ud_msg_s *msg, const struct ud_auxmsg_s *UNUSED(aux))
{
/* process them posrpts */
	static char sbuf[64U + 64U];
	fixc_msg_t f;
	struct timeval tv[1];
	const char *ac = NULL;
	size_t az = 0UL;
	const char *sym = NULL;
	size_t sz = 0UL;
	struct sl1t_s l = {0};
	struct sl1t_s s = {0};
	apfd_cache_t pos;
	uint16_t id;
	int res = 0;

	/* what's the wallclock time */
	gettimeofday(tv, NULL);

	if (UNLIKELY((f = make_fixc_from_fix(msg->data, msg->dlen)) == NULL)) {
		return 0;
	} else if (UNLIKELY(f->f35.typ != FIXC_TYP_MSGTYP ||
			f->f35.mtyp != FIXML_MSG_PositionReport)) {
		/* what about batch? */
		goto out;
	}

	for (size_t i = 0; i < f->nflds; i++) {
		const struct fixc_fld_s *fld = f->flds + i;
		const char *val = f->pr + fld->off;

		switch (f->flds[i].tag) {
		case 1:
			/* account name */
			ac = val;
			az = strlen(val);
			break;
		case 55:
			/* symbol name */
			sym = val;
			sz = strlen(val);
			break;
		case 704:
			/* @long */
			l.w[0] = ffff_m62_get_s(&val).u;
			sl1t_set_stmp_sec(&l, tv->tv_sec);
			sl1t_set_stmp_msec(&l, tv->tv_usec / 1000);
			sl1t_set_ttf(&l, SL1T_TTF_G64);
			sl1t_set_tblidx(&l, id);
			break;
		case 705:
			/* @short */
			s.w[0] = ffff_m62_get_s(&val).u;
			sl1t_set_stmp_sec(&s, tv->tv_sec);
			sl1t_set_stmp_msec(&s, tv->tv_usec / 1000);
			sl1t_set_ttf(&s, SL1T_TTF_G64);
			sl1t_set_tblidx(&s, id);
			break;
		default:
			break;
		}
	}

	/* we don't want no steenkin buffer overfloes */
	if (UNLIKELY(az >= sizeof(sbuf) / 2 - 1)) {
		az = sizeof(sbuf) / 2 - 1 - 1;
	}
	if (UNLIKELY(sz >= sizeof(sbuf) / 2 - 1)) {
		sz = sizeof(sbuf) / 2 - 1;
	}
	/* roll a symbol */
	memcpy(sbuf, ac, az);
	sbuf[az] = '/';
	memcpy(sbuf + az + 1, sym, sz);
	sbuf[az + 1 + sz] = '\0';

	/* try and have ute assign us an id */
	if ((id = ute_sym2idx(uctx, sbuf)) == 0) {
		/* big bugger */
		goto out;
	}

	UMAD_DEBUG("banging  %s -> %hu\n", sbuf, id);
	/* make sure the cache is wide enough */
	check_cache(id);

	/* find the cache cell */
	pos = &CACHE(id);

	if (LIKELY(l.hdr->sec != 0U)) {
		*pos->lng = l;
		ute_add_tick(uctx, AS_SCOM(&l));
		res++;
	}

	if (LIKELY(s.hdr->sec != 0U)) {
		*pos->shrt = s;
		ute_add_tick(uctx, AS_SCOM(&s));
		res++;
	}

out:
	free_fixc(f);
	return res;
}
#else  /* !HAVE_LIBFIXC_FIX_H */
static int
snarf_pos_rpt(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
/* process them posrpts */
	static const char fix_pos_rpt[] = "35=AP";
	static const char fix_chksum[] = "10=";
	static const char fix_inssym[] = "55=";
	static const char fix_acct[] = "1=";
	static const char fix_lqty[] = "704=";
	static const char fix_sqty[] = "705=";
	struct timeval tv[1];
	cli_t c;
	int res = 0;

	/* find the cli or add it if not there already */
	if (UNLIKELY(msg->dlen == 0)) {
		return 0;
	} else if ((c = find_cli((struct key_s){aux->src}))) {
		;
	} else if ((c = add_cli((struct key_s){aux->src}))) {
		;
	} else {
		/* fuck */
		return -1;
	}

	/* what's the wallclock time */
	gettimeofday(tv, NULL);

	for (const char *p = msg->data, *q, *const ep = p + msg->dlen;
	     p && p < ep && (q = find_fix_eofld(p, fix_pos_rpt));
	     p = find_fix_eofld(p, fix_chksum)) {
		static char sbuf[64U + 64U];
		const char *ac;
		size_t az;
		const char *sym;
		size_t sz;
		uint16_t id;
		apfd_cache_t pos;
		const char *q;

		if ((az = find_fix_fld(&ac, p, fix_acct)) == 0) {
			UMAD_DEBUG("no acct\n");
			continue;
		} else if ((sz = find_fix_fld(&sym, p, fix_inssym)) == 0) {
			/* great, we NEED that symbol */
			UMAD_DEBUG("no symbol\n");
			continue;
		}
		/* ffw a/c */
		ac += sizeof(fix_acct) - 1;
		az -= sizeof(fix_acct) - 1;
		/* ffw sym */
		sym += sizeof(fix_inssym) - 1;
		sz -= sizeof(fix_inssym) - 1;
		/* we don't want no steenkin buffer overfloes */
		if (UNLIKELY(az >= sizeof(sbuf) / 2 - 1)) {
			az = sizeof(sbuf) / 2 - 1 - 1;
		}
		if (UNLIKELY(sz >= sizeof(sbuf) / 2 - 1)) {
			sz = sizeof(sbuf) / 2 - 1;
		}
		/* roll a symbol */
		memcpy(sbuf, ac, az);
		sbuf[az] = '/';
		memcpy(sbuf + az + 1, sym, sz);
		sbuf[az + 1 + sz] = '\0';
		/* try and have ute assign us an id */
		if ((id = ute_sym2idx(uctx, sbuf)) == 0) {
			/* big bugger */
			continue;
		}

		/* make sure the cache is wide enough */
		check_cache(id);

		/* find the cache cell */
		pos = &CACHE(id);

		if (find_fix_fld(&q, p, fix_lqty)) {
			q += sizeof(fix_lqty) - 1;
			pos->lng->w[0] = ffff_m62_get_s(&q).u;
			sl1t_set_stmp_sec(pos->lng, tv->tv_sec);
			sl1t_set_stmp_msec(pos->lng, tv->tv_usec / 1000);
			sl1t_set_ttf(pos->lng, SL1T_TTF_G64);
			sl1t_set_tblidx(pos->lng, id);
			ute_add_tick(uctx, AS_SCOM(pos->lng));
		}

		if (find_fix_fld(&q, p, fix_sqty)) {
			q += sizeof(fix_sqty) - 1;
			pos->shrt->w[0] = ffff_m62_get_s(&q).u;
			sl1t_set_stmp_sec(pos->shrt, tv->tv_sec);
			sl1t_set_stmp_msec(pos->shrt, tv->tv_usec / 1000);
			sl1t_set_ttf(pos->shrt, SL1T_TTF_G64);
			sl1t_set_tblidx(pos->shrt, id);
			ute_add_tick(uctx, AS_SCOM(pos->shrt));
		}

		res++;
	}

	/* leave a last_seen note */
	CLI(c)->last_seen = tv->tv_sec;
	return res;
}
#endif	/* HAVE_LIBFIXC_FIX_H */

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
	ute_set_fn(uctx, nu_fn);

	/* magic */
	switch (fork()) {
	case -1:
		fprintf(logerr, "cannot fork :O\n");
		return;

	default: {
		/* i am the parent */
		utectx_t nu = ute_open(u_fn, UO_CREAT | UO_RDWR | UO_TRUNC);
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

	UMAD_DEBUG("adv_name: %s\n", brag_uri);
	return 0;
}


/* the actual worker function */
#define POS_RPT		(0x757aU)
#define POS_RPT_RPL	(0x757bU)

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
		case POS_RPT:
		case POS_RPT_RPL:
			/* parse the message here */
			snarf_pos_rpt(msg, aux);
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

	UMAD_DEBUG("interesting activity on %d\n", w->fd);

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
#include "um-apfd-clo.h"
#include "um-apfd-clo.c"
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
		UMAD_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/um-apfdmp.log", "w");
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
	struct um_args_info argi[1];
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
	if (um_parser(argc, argv, argi)) {
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
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		struct ud_sockopt_s opt = {
			UD_SUB,
			.port = (uint16_t)argi->beef_arg[i],
		};
		ud_sock_t s;

		if (LIKELY((s = ud_socket(opt)) != NULL)) {
			ev_io_init(beef + i + 1, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef + i + 1);
		}
		beef[i + 1].data = s;
	}

	/* make a channel for http/dccp requests */
	if (argi->websvc_port_given) {
		struct sockaddr_in6 sa = {
			.sin6_family = AF_INET6,
			.sin6_addr = in6addr_any,
			.sin6_port = 0,
		};
		socklen_t sa_len = sizeof(sa);
		int s;

		if (argi->websvc_port_given) {
			sa.sin6_port = htons(argi->websvc_port_arg);
		}

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
	if (!argi->output_given && !argi->into_given) {
		if ((uctx = ute_mktemp(UO_RDWR)) == NULL) {
			res = 1;
			goto past_ute;
		}
		u_fn = strdup(ute_fn(uctx));
	} else {
		int u_fl = UO_CREAT | UO_RDWR;

		if (argi->output_given) {
			u_fn = argi->output_arg;
			u_fl |= UO_TRUNC;
		} else if (argi->into_given) {
			u_fn = argi->into_arg;
		}
		if ((uctx = ute_open(u_fn, u_fl)) == NULL) {
			res = 1;
			goto past_ute;
		}
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

	/* kick the config context */
	um_parser_free(argi);

	/* unloop was called, so exit */
	return res;
}

/* um-apfd.c ends here */
