/*** um-quodmp.c -- unsermarkt quote dumper
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
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

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

#include <uterus.h>

#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

typedef uint32_t lobidx_t;
typedef struct lob_cli_s *lob_cli_t;

/* the client */
struct lob_cli_s {
	union ud_sockaddr_u sa;
	uint16_t id;

	char sym[64];
	size_t ssz;
};


/* we support a maximum of 64 order books atm */
static lob_cli_t cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)

#if defined MAP_ANON && !defined MAP_ANONYMOUS
# define MAP_ANONYMOUS	MAP_ANON
#endif	/* MAP_ANON && !MAP_ANONYMOUS */
#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

static int
sa_eq_p(ud_sockaddr_t sa1, ud_sockaddr_t sa2)
{
	const size_t s6sz = sizeof(sa1->sa6.sin6_addr);
	return sa1->sa6.sin6_family == sa2->sa6.sin6_family &&
		sa1->sa6.sin6_port == sa2->sa6.sin6_port &&
		memcmp(&sa1->sa6.sin6_addr, &sa2->sa6.sin6_addr, s6sz) == 0;
}

static lobidx_t
find_cli(ud_sockaddr_t sa, uint16_t id)
{
	for (size_t i = 0; i < ncli; i++) {
		lob_cli_t c = cli + i;
		ud_sockaddr_t cur_sa = &c->sa;
		uint16_t cur_id = c->id;

		if (sa_eq_p(cur_sa, sa) && cur_id == id) {
			return i + 1;
		}
	}
	return 0;
}

static lobidx_t
add_cli(ud_sockaddr_t sa, uint16_t id)
{
	size_t idx = ncli++;
#define CATCHALL_BIDLOB	(0)
#define CATCHALL_ASKLOB	(1)

	if (ncli * sizeof(*cli) > alloc_cli) {
		size_t nu = alloc_cli + 4096;
		cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
		alloc_cli = nu;
	}

	cli[idx].sa = *sa;
	cli[idx].id = id;
	cli[idx].ssz = 0;
	return idx + 1;
}

static void
init_cli(void)
{
	/* and our client list */
	cli = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	alloc_cli = 4096;
	return;
}

static void
fini_cli(void)
{
	/* and the list of clients */
	munmap(cli, alloc_cli);
	return;
}


static void
snarf_meta(job_t j)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct udpc_seria_s ser[1];
	uint8_t tag;

	udpc_seria_init(ser, pbuf, plen);
	while (ser->msgoff < plen && (tag = udpc_seria_tag(ser))) {
		lobidx_t c;
		uint16_t id;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		id = udpc_seria_des_ui16(ser);
		/* and the cli, if any */
		if ((c = find_cli(&j->sa, id)) == 0) {
			c = add_cli(&j->sa, id);
		}
		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (UNLIKELY(tag != UDPC_TYPE_STR || c == 0)) {

			break;
		}
		/* fuck error checking */
		CLI(c)->ssz = udpc_seria_des_str_into(
			CLI(c)->sym, sizeof(CLI(c)->sym), ser);
	}
	return;
}

static void
snarf_data(job_t j)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	lobidx_t c;

	if (UNLIKELY(plen == 0)) {
		return;
	}

	for (scom_thdr_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t id = scom_thdr_tblidx(sp);

		if ((c = find_cli(&j->sa, id)) == 0) {
			c = add_cli(&j->sa, id);
		}

		/* fiddle with the tblidx */
		scom_thdr_set_tblidx(sp, CLI(c)->id);
		/* and pump the tick to ute */
		;
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
	ev_unloop(EV_A_ EVUNLOOP_ALL);
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

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_io *beef = NULL;
	size_t nbeef = 0;
	/* args */
	struct umqd_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];

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
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

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

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		int s = ud_mcast_init(argi->beef_arg[i]);
		ev_io_init(beef + i + 1, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	/* init cli space */
	init_cli();

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	for (unsigned int i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
		ud_mcast_fini(s);
	}

	/* finish cli space */
	fini_cli();

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	umqd_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* um-quodmp.c ends here */
