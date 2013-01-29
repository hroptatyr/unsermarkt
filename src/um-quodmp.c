/*** um-quodmp.c -- unsermarkt quote dumper
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

#include "svc-uterus.h"
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
# define UMQD_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define UMQD_DEBUG(args...)
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
	struct sockaddr_storage sa __attribute__((aligned(16)));
	uint16_t id;
	uint16_t tgtid;

	volatile uint32_t last_seen;

	char sym[64];
};

/* children need access to beef resources */
static ev_io *beef = NULL;
static size_t nbeef = 0;


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


static utectx_t u = NULL;
static const char *u_fn = NULL;
static size_t u_nt = 0;
/* number of ticks ignored due to missing symbols */
static size_t ign = 0;

static void
snarf_meta(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
	struct um_qmeta_s brg[1];
	struct key_s k = {
		.sa = (const void*)aux->src,
	};
	uint16_t id;
	cli_t c;

	if (UNLIKELY(um_chck_msg_brag(brg, msg) < 0)) {
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
	if (UNLIKELY(brg->symlen > sizeof(CLI(c)->sym))) {
		brg->symlen = sizeof(CLI(c)->sym);
	}
	/* fill in symbol */
	memcpy(CLI(c)->sym, brg->sym, brg->symlen);

	/* check if we know about the symbol */
	if ((id = ute_sym2idx(u, CLI(c)->sym)) == CLI(c)->tgtid) {
		/* yep, known and it's the same id, brilliant */
		;
	} else if (CLI(c)->tgtid) {
		/* known but the wrong id */
		UMQD_DEBUG("reass %hu -> %hu\n", CLI(c)->tgtid, id);
		CLI(c)->tgtid = id;
	} else /*if (CLI(c)->tgtid == 0)*/ {
		/* unknown */
		UMQD_DEBUG("ass'ing %s -> %hu\n", CLI(c)->sym, id);
		CLI(c)->tgtid = id;
		ute_bang_symidx(u, CLI(c)->sym, CLI(c)->tgtid);
	}

	/* leave a last_seen note */
	{
		struct timeval tv[1];
		gettimeofday(tv, NULL);
		CLI(c)->last_seen = tv->tv_sec;
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
			.id = scom_thdr_tblidx(AS_SCOM(msg->data)),
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

	/* fiddle with the tblidx */
	scom_thdr_set_tblidx(AS_SCOM_THDR(ss), CLI(c)->tgtid);
	/* and pump the tick to ute */
	ute_add_tick(u, AS_SCOM(ss));

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
	ute_set_fn(u, nu_fn);
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
		utectx_t nu = ute_open(u_fn, UO_CREAT | UO_RDWR | UO_TRUNC);
		ign = 0;
		ute_clone_slut(nu, u);

		/* free resources */
		ute_free(u);
		/* nu u is nu */
		u = nu;
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
#include "um-quodmp-clo.h"
#include "um-quodmp-clo.c"
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
	ev_timer prune[1];
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (umqd_parser(argc, argv, argi)) {
		exit(1);
	}

	if (argi->output_given && argi->into_given) {
		fputs("only one of --output and --into can be given\n", logerr);
		res = 1;
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

	/* init cli space */
	init_cli();

	/* init ute */
	if (!argi->output_given && !argi->into_given) {
		if ((u = ute_mktemp(UO_RDWR)) == NULL) {
			res = 1;
			goto past_ute;
		}
		u_fn = strdup(ute_fn(u));
	} else {
		int u_fl = UO_CREAT | UO_RDWR;

		if (argi->output_given) {
			u_fn = argi->output_arg;
			u_fl |= UO_TRUNC;
		} else if (argi->into_given) {
			u_fn = argi->into_arg;
		}
		if ((u = ute_open(u_fn, u_fl)) == NULL) {
			res = 1;
			goto past_ute;
		}
	}

	if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto past_loop;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	/* close the file, might take a while due to sorting */
	if (u) {
		/* get the number of ticks */
		u_nt = ute_nticks(u);
		/* deal with the file */
		ute_close(u);
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
	umqd_parser_free(argi);

	/* unloop was called, so exit */
	return res;
}

/* um-quodmp.c ends here */
