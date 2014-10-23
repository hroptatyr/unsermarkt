/*** xross-quo.c -- read fx quotes from beef and quote crosses
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
#include <netinet/in.h>

#include <unserding/unserding.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "svc-uterus.h"
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
# define MAX_CLI_AGE	(60)
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

typedef const struct sockaddr_in6 *my_sockaddr_t;

struct key_s {
	my_sockaddr_t sa;
	uint16_t id;
};

/* the client */
struct cli_s {
	struct sockaddr_in6 sa __attribute__((aligned(16)));
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
static size_t npaths;

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

#if !defined __clang__
	/* often used, so just compute them here */
	pri = ffff_m30_d(cell->pri);
	qty = ffff_m30_d(cell->qty);
#else  /* __clang__ */
/* see bug 15134 */
	pri = ffff_m30_d(ffff_m30_get_ui32(cell->pri));
	qty = ffff_m30_d(ffff_m30_get_ui32(cell->qty));
#endif	/* !__clang__ */

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

		if (res.b.u == 0 || bid.u && res.b.v < bid.v) {
			res.b = bid;
		}
		if (res.a.u == 0 || ask.u && res.a.v > ask.v) {
			res.a = ask;
		}
	}
	return res;
}


static ud_sock_t ute_out_ch;

static void
dissem_bbo(struct bbo_s bbo)
{
	static struct bbo_s last_bbo;
	static time_t last_brag;
	struct sl1t_s new[1];
	struct timeval now[1];

	if (bbo.b.u == last_bbo.b.u && bbo.a.u == last_bbo.a.u) {
		/* piss off right away, nothing's changed */
		return;
	}

	gettimeofday(now, NULL);
	if (now->tv_sec >= last_brag + 10) {
		struct um_qmeta_s brg = {
			.idx = 1U,
			.sym = "EURAUDx",
			.symlen = 7U,
			.uri = NULL,
			.urilen = 0U,
		};

		(void)um_pack_brag(ute_out_ch, &brg);
		last_brag = now->tv_sec;
	}

	/* bit of prep work */
	sl1t_set_stmp_sec(new, now->tv_sec);
	sl1t_set_stmp_msec(new, now->tv_usec / 1000);
	sl1t_set_tblidx(new, 1);

	if (bbo.b.u == last_bbo.b.u) {
		sl1t_set_ttf(new, SL1T_TTF_BID);

		new->pri = bbo.b.u;
#if defined HAVE_ANON_STRUCTS_INIT
		new->qty = ((union m30_u){.expo = 1, .mant = 10000}).u;
#else  /* !HAVE_ANON_STRUCTS_INIT */
		{
			m30_t tmp;
			tmp.expo = 1;
			tmp.mant = 10000;
			new->qty = tmp.u;
		}
#endif	/* HAVE_ANON_STRUCTS_INIT */
		(void)um_pack_sl1t(ute_out_ch, new);
	}
	if (bbo.a.u != last_bbo.a.u) {
		sl1t_set_ttf(new, SL1T_TTF_ASK);

		new->pri = bbo.a.u;
#if defined HAVE_ANON_STRUCTS_INIT
		new->qty = ((union m30_u){.expo = 1, .mant = 10000}).u;
#else  /* !HAVE_ANON_STRUCTS_INIT */
		{
			m30_t tmp;
			tmp.expo = 1;
			tmp.mant = 10000;
			new->qty = tmp.u;
		}
#endif	/* HAVE_ANON_STRUCTS_INIT */
		(void)um_pack_sl1t(ute_out_ch, new);
	}
	/* just to have something we can compare things to */
	last_bbo = bbo;

	/* and really really send him off now */
	ud_flush(ute_out_ch);
	return;
}

static void
snarf_meta(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux, graph_t g)
{
	struct um_qmeta_s brg[1];
	struct key_s k;
	cli_t c;
	gpair_t p;

	if (um_chck_brag(brg, msg) < 0) {
		return;
	} else if ((k.sa = (my_sockaddr_t)aux->src) == NULL) {
		return;
	}

	/* fill in the rest of the key */
	k.id = (uint16_t)brg->idx;

	/* and off to find the cli */
	if ((c = find_cli(k)) == 0) {
		c = add_cli(k);
	}

	/* update the symbol */
	{
		size_t z;

		if ((z = brg->symlen) > sizeof(CLI(c)->sym)) {
			z = sizeof(CLI(c)->sym);
		}
		/* keep a not of the symbol */
		memcpy(CLI(c)->sym, brg->sym, z);
	}

	/* generate a pair and add him */
	if ((p = find_pair_by_sym(g, CLI(c)->sym)) != CLI(c)->tgtid) {
		XQ_DEBUG("g'd pair %s %zu\n", CLI(c)->sym, p);
		CLI(c)->tgtid = p;
	}

	/* leave a last_seen note */
	{
		struct timeval tv[1];
		gettimeofday(tv, NULL);
		CLI(c)->last_seen = tv->tv_sec;
	}
	return;
}

static uint64_t
snarf_data(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux, graph_t g)
{
	struct sndwch_s s[4];
	struct timeval tv[1];
	scom_t sp;
	size_t sz;
	struct key_s k;
	uint64_t aff;
	cli_t c;

	if ((sp = msg->data, (sz = scom_tick_size(sp)) != msg->dlen)) {
		/* no idea what went wrong here */
		return 0UL;
	} else if ((k.sa = (my_sockaddr_t)aux->src) == NULL) {
		/* no source address, very bad */
		return 0UL;
	}
	/* bang to aliged space */
	memcpy(s, sp, sz);
	sp = AS_SCOM(s);

	/* fill in the rest of the key */
	k.id = scom_thdr_tblidx(sp);

	if ((c = find_cli(k)) == 0) {
		c = add_cli(k);
	}
	if (CLI(c)->tgtid == 0) {
		return 0UL;
	}

	/* fiddle with the tblidx */
	scom_thdr_set_tblidx(AS_SCOM_THDR(s), CLI(c)->tgtid);
	aff = upd_pair(g, CLI(c)->tgtid, CONST_SL1T_T(sp));

	/* lest we miss our flight */
	gettimeofday(tv, NULL);
	/* leave a last_seen note */
	CLI(c)->last_seen = tv->tv_sec;
	return aff;
}


/* the actual worker function */
static graph_t gg;

static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	struct ud_msg_s msg[1];
	ud_sock_t s = w->data;
	uint64_t aff = 0;

	while (ud_chck_msg(msg, s) >= 0) {
		struct ud_auxmsg_s aux[1];

		(void)ud_get_aux(aux, s);
		switch (msg->svc) {
		case UTE_QMETA:
			snarf_meta(msg, aux, gg);
			break;

		case UTE_CMD:
			aff |= snarf_data(msg, aux, gg);
			break;
		default:
			break;
		}
	}

	if (aff && ute_out_ch != NULL) {
		struct bbo_s bbo = find_bbo(gg);

		dissem_bbo(bbo);
	}
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
	npaths = ccyg_add_paths(g, (struct pair_s){ISO_4217_EUR, ISO_4217_AUD});
	XQ_DEBUG("%zu virtual paths added\n", npaths);

#if defined DEBUG_FLAG
	XQ_DEBUG("\nGRAPH NOW\n");
	prnt_graph(g);
#endif	/* DEBUG_FLAG */
	return;
}



#include "xross-quo.yucc"

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
	/* args */
	yuck_t argi[1U];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_timer prune[1];
	int rc = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
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
	nbeef = argi->beef_nargs + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* generate the graph we're talking */
	gg = make_graph();
	build_hops(gg);

	/* attach a multicast listener, default channel for control msgs */
	{
		struct ud_sockopt_s opt = {UD_SUB};
		ud_sock_t s;

		if (UNLIKELY((s = ud_socket(opt)) == NULL)) {
			/* grrr, ok we can dispense with control messages */
			;
		} else {
			beef->data = s;
			ev_io_init(beef, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef);
		}
	}

	/* go through all beef channels */
	for (size_t i = 0U; i < argi->beef_nargs; i++) {
		struct ud_sockopt_s opt = {UD_PUBSUB};
		long unsigned int tmp;
		ud_sock_t s;

		if ((tmp = strtoul(argi->beef_args[i], NULL, 0),
		     !(opt.port = (short unsigned int)tmp))) {
			;
		} else if (LIKELY((s = ud_socket(opt)) != NULL)) {
			beef[i + 1].data = s;
			ev_io_init(beef + i + 1, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef + i + 1);
		}
	}
	if (argi->beef_nargs) {
		/* for simplicity use the first beef channel for
		 * dissemination of crosses */
		ute_out_ch = beef[1].data;
	}

	/* init cli space */
	init_cli();

	if (argi->daemonise_flag && detach() < 0) {
		perror("daemonisation failed");
		rc = 1;
		goto past_loop;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_sock_t s = beef[i].data;
		ev_io_stop(EV_A_ beef + i);
		ud_close(s);
		beef[i].data = NULL;
	}
	/* free beef resources */
	free(beef);

	/* free the graph */
	free_graph(gg);

	/* finish cli space */
	fini_cli();

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	yuck_free(argi);

	/* unloop was called, so exit */
	return rc;
}

/* xross-quo.c ends here */
