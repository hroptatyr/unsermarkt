/*** xross-quo.c -- read fx quotes from beef and quote crosses
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
#include <sys/mman.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "iso4217.h"
#include "ccy-graph.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

#define ONE_DAY		86400.0
#define MIDNIGHT	0.0

/* maximum allowed age for clients (in seconds) */
#if defined DEBUG_FLAG
# define MAX_CLI_AGE	(60.0)
# define PRUNE_INTV	(10.0)
#else  /* !DEBUG_FLAG */
# define MAX_CLI_AGE	(1800)
# define PRUNE_INTV	(60.0)
#endif	/* DEBUG_FLAG */

static FILE *logerr;
#if defined DEBUG_FLAG
# define XQ_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define XQ_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef size_t cli_t;
typedef intptr_t hx_t;

struct key_s {
	ud_sockaddr_t sa;
	uint16_t id;
};

/* the client */
struct cli_s {
	union ud_sockaddr_u sa __attribute__((aligned(16)));
	uint16_t id;
	uint16_t tgtid;

	uint32_t last_seen;

	char sym[64];
};

/* children need access to beef resources */
static ev_io *beef = NULL;
static size_t nbeef = 0;


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
			XQ_DEBUG("pruning %zu\n", i);
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

			XQ_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			memcpy(CLI(i - consec), CLI(i), nmv * sizeof(*cli));
			nu_ncli -= consec;
		} else if (consec) {
			XQ_DEBUG("condensing %zu/%zu clis\n", consec, ncli);
			nu_ncli -= consec;
		}
	}

	/* let everyone know how many clis we've got */
	ncli = nu_ncli;
	return;
}


/* pair handling */
struct bbo_s {
	m30_t b;
	m30_t a;
};

static gpair_t
find_pair_by_sym(graph_t g, const char *sym)
{
	struct pair_s p;

	if ((p.bas = find_iso_4217_by_name(sym)) == NULL) {
		return NULL_PAIR;
	}
	/* otherwise at least the base currency is there */
	switch (sym[3]) {
	case '.':
	case '/':
		sym++;
	default:
		sym += 3;
		break;
	case '\0':
		/* fuck!!! */
		return NULL_PAIR;
	}
	/* YAY, we survived the hardest part, the unstandardised separator */
	if ((p.trm = find_iso_4217_by_name(sym)) == NULL) {
		return NULL_PAIR;
	}
	return ccyg_find_pair(g, p);
}

static uint64_t
upd_pair(graph_t g, gpair_t p, const_sl1t_t cell)
{
	double pri, qty;

	/* often used, so just compute them here */
	pri = ffff_m30_d(cell->pri);
	qty = ffff_m30_d(cell->qty);

	switch (sl1t_ttf(cell)) {
	case SL1T_TTF_BID:
		upd_bid(g, p, pri, qty);
		break;
	case SL1T_TTF_ASK:
		upd_ask(g, p, pri, qty);
		break;
	default:
		XQ_DEBUG("unknown tick type %hu\n", sl1t_ttf(cell));
		return 0;
	}

	return recomp_affected(g, p);
}

static struct bbo_s
find_bbo(graph_t g)
{
	struct bbo_s res = {0, 0};

	for (size_t i = 9; i < 9 + npaths; i++) {
		m30_t bid = ffff_m30_get_d(get_bid(g, i));
		m30_t ask = ffff_m30_get_d(get_ask(g, i));

		bid.mant -= bid.mant % 1000;
		ask.mant -= ask.mant % 1000;

		if (res.b.mant == 0 || bid.mant && res.b.mant < bid.mant) {
			res.b = bid;
		}
		if (res.a.mant == 0 || ask.mant && res.a.mant > ask.mant) {
			res.a = ask;
		}
	}
	return res;
}


static void
snarf_meta(job_t j, graph_t g)
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
		gpair_t p;

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

		/* generate a pair and add him */
		if ((p = find_pair_by_sym(g, CLI(c)->sym)) != CLI(c)->tgtid) {
			XQ_DEBUG("g'd pair %s %zu\n", CLI(c)->sym, p);
			CLI(c)->tgtid = p;
		}

		/* leave a last_seen note */
		CLI(c)->last_seen = tv->tv_sec;
	}
	return;
}

static void
snarf_data(job_t j, graph_t g)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct key_s k = {
		.sa = &j->sa,
	};
	struct timeval tv[1];
	uint64_t aff = 0;

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
		if (CLI(c)->tgtid == 1) {
			double p = ffff_m30_d(CONST_SL1T_T(sp)->pri);

			XQ_DEBUG("PAIR 1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
			XQ_DEBUG("%c %.6f\n", 'c' - sl1t_ttf(CONST_SL1T_T(sp)), p);
		} else if (CLI(c)->tgtid == 0) {
			continue;
		}

		/* fiddle with the tblidx */
		scom_thdr_set_tblidx(sp, CLI(c)->tgtid);
		aff |= upd_pair(g, CLI(c)->tgtid, CONST_SL1T_T(sp));

		/* leave a last_seen note */
		CLI(c)->last_seen = tv->tv_sec;
	}

	if (aff) {
		struct bbo_s bbo = find_bbo(g);
		double bb = ffff_m30_d(bbo.b);
		double ba = ffff_m30_d(bbo.a);

		XQ_DEBUG("bbid %.6f  %.6f bask\n", bb, ba);
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
	graph_t g = w->data;

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
		snarf_meta(j, g);
		break;

	case UTE:
	case UTE_RPL:
		snarf_data(j, g);
		break;
	default:
		break;
	}

out_revok:
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
	return;
}

static void
prune_cb(EV_P_ ev_timer *w, int UNUSED(r))
{
	prune_clis();
	ev_timer_again(EV_A_ w);
	return;
}


/* graph guts */
#include "ccy-graph.c"

/* this must be configurable somehow */
static void
build_hops(graph_t g)
{
	ccyg_add_pair(g, (struct pair_s){ISO_4217_EUR, ISO_4217_AUD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_EUR, ISO_4217_USD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_AUD, ISO_4217_USD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_GBP, ISO_4217_USD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_NZD, ISO_4217_USD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_EUR, ISO_4217_GBP});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_EUR, ISO_4217_NZD});
	ccyg_add_pair(g, (struct pair_s){ISO_4217_AUD, ISO_4217_NZD});

	/* population */
	ccyg_populate(g);

	/* and construct all paths */
	ccyg_add_paths(g, (struct pair_s){ISO_4217_EUR, ISO_4217_AUD});

	XQ_DEBUG("\nGRAPH NOW\n");
	prnt_graph(g);
	return;
}



#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "xross-quo-clo.h"
#include "xross-quo-clo.c"
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
		XQ_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/xross-quo.log", "w");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	static graph_t g;
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* args */
	struct xq_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_timer prune[1];
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (xq_parser(argc, argv, argi)) {
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

	/* prune timer, check occasionally for old unused clients */
	ev_timer_init(prune, prune_cb, PRUNE_INTV, PRUNE_INTV);
	ev_timer_start(EV_A_ prune);

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* generate the graph we're talking */
	g = make_graph();
	build_hops(g);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	{
		int s = ud_mcast_init(UD_NETWORK_SERVICE);
		ev_io_init(beef, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		int s = ud_mcast_init(argi->beef_arg[i]);
		ev_io_init(beef + i + 1, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
		/* make sure we let our beef handler know about our graph */
		beef[i + 1].data = g;
	}

	/* init cli space */
	init_cli();

	if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto past_loop;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
		ud_mcast_fini(s);
		beef[i].data = NULL;
	}
	/* free beef resources */
	free(beef);

	/* free the graph */
	free_graph(g);

	/* finish cli space */
	fini_cli();

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	xq_parser_free(argi);

	/* unloop was called, so exit */
	return res;
}

/* xross-quo.c ends here */
