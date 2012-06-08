/*** um-quomon.c -- unsermarkt quote monitor
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

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

#include <unserding/unserding.h>
#include <unserding/protocore.h>

#include <uterus.h>
/* to get a take on them m30s and m62s */
#define DEFINE_GORY_STUFF
#include <m30.h>
#include <m62.h>

/* for the limit order book */
#include <sys/mman.h>

#include <ncurses.h>

typedef uint32_t lobidx_t;
typedef struct lob_cli_s *lob_cli_t;
typedef union lob_side_u *lob_side_t;

/* the client */
struct lob_cli_s {
	union ud_sockaddr_u sa;
	uint16_t id;

	lobidx_t b;
	lobidx_t a;

	char ss[INET6_ADDRSTRLEN + 2 + 6];
	size_t sz;

	char sym[64];
	size_t ssz;
};

/* our limit order book, well just level 2 */
struct lob_entry_s {
	/* client, that is addr+port */
	lobidx_t cli;
	lobidx_t pad;
	/* price */
	m30_t p;
	/* quantity */
	m30_t q;
};

/* same but with navigation */
struct lob_entnav_s {
	struct lob_entry_s v;
	lobidx_t prev;
	lobidx_t next;
};

/* all entries of one side */
union lob_side_u {
	struct lob_entnav_s e[];
	struct lob_entnav_s pad;

	struct {
		lobidx_t head;
		lobidx_t free;
	};
};


static lob_side_t lobb = NULL;
static lob_side_t loba = NULL;
static lob_cli_t cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)
#define EAT(y, x)	y->e[x]
#define NEXT(y, x)	EAT(y, x).next
#define PREV(y, x)	EAT(y, x).prev

#if defined MAP_ANON && !defined MAP_ANONYMOUS
# define MAP_ANONYMOUS	MAP_ANON
#endif	/* MAP_ANON && !MAP_ANONYMOUS */
#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

static void
init_lob(void)
{
	lobb = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	loba = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	cli = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	alloc_cli = 4096;

	/* init the lob */
	lobb->free = 1;
	loba->free = 1;
	for (lobidx_t i = 1; i < 4096 / sizeof(*lobb->e) - 1; i++) {
		NEXT(lobb, i) = i + 1;
		NEXT(loba, i) = i + 1;
	}
	return;
}

static void
free_lob(void)
{
	munmap(lobb, 4096);
	munmap(loba, 4096);
	munmap(cli, 4096);
	return;
}

static lobidx_t
lob_ins_at(lob_side_t s, lobidx_t pr, struct lob_entry_s v)
{
	lobidx_t nu;

	nu = s->free;
	assert(nu);
	assert(nu != pr);
	assert(s->head != s->free);
	s->free = NEXT(s, s->free);

	/* populate the cell */
	EAT(s, nu).v = v;
	/* update navigators */
	if (pr) {
		lobidx_t nx = NEXT(s, pr);

		NEXT(s, nu) = nx;
		PREV(s, nu) = pr;
		NEXT(s, pr) = nu;
		if (nx) {
			PREV(s, nx) = nu;
		}
	} else {
		/* ins at head */
		PREV(s, nu) = 0;
		if ((NEXT(s, nu) = s->head)) {
			PREV(s, s->head) = nu;
		}
		s->head = nu;
	}

	assert(pr != nu);
	return nu;
}

static void
lob_rem_at(lob_side_t s, lobidx_t idx)
{
	assert(idx);

	/* fix up navigators */
	if (s->head != idx) {
		assert(PREV(s, idx));

		if (NEXT(s, idx)) {
			PREV(s, NEXT(s, idx)) = PREV(s, idx);
		}
		NEXT(s, PREV(s, idx)) = NEXT(s, idx);
	} else {
		/* head fiddling */
		assert(s->head);

		if ((s->head = NEXT(s, idx))) {
			PREV(s, s->head) = 0;
		}
	}

	assert(idx != s->free);
	NEXT(s, idx) = s->free;
	s->free = idx;
	assert(s->head != s->free);
	return;
}

static lobidx_t
find_bid(m30_t p)
{
	lobidx_t idx = 0;

	for (lobidx_t i = lobb->head; i; idx = i, i = NEXT(lobb, i)) {
		if (EAT(lobb, i).v.p.v < p.v) {
			return idx;
		} else if (EAT(lobb, i).v.p.v == p.v) {
			return i;
		}
	}
	return idx;
}

static lobidx_t
find_ask(m30_t p)
{
	lobidx_t idx = 0;

	for (lobidx_t i = loba->head; i; idx = i, i = NEXT(loba, i)) {
		if (EAT(loba, i).v.p.v > p.v) {
			return idx;
		} else if (EAT(loba, i).v.p.v == p.v) {
			return i;
		}
	}
	return idx;
}

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

	if (ncli * sizeof(*cli) > alloc_cli) {
		size_t nu = alloc_cli + 4096;
		cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
		alloc_cli = nu;
	}

	cli[idx].sa = *sa;
	cli[idx].id = id;
	cli[idx].b = 0;
	cli[idx].a = 0;
	cli[idx].ssz = 0;

	/* obtain the address in human readable form */
	{
		char *epi = cli[idx].ss;
		int fam = ud_sockaddr_fam(sa);
		const struct sockaddr *addr = ud_sockaddr_addr(sa);
		uint16_t port = ud_sockaddr_port(sa);

		*epi++ = '[';
		if (inet_ntop(fam, addr, epi, sizeof(cli->ss))) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
		cli[idx].sz = epi - cli[idx].ss;
	}
	return idx + 1;
}

static void
snarf_syms(job_t j)
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


/* the actual worker function */
static int changep = 0;

static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nread;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	j->sock = w->fd;
	nread = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nread < 0)) {
		goto out_revok;
	} else if (nread == 0) {
		/* no need to bother */
		goto out_revok;
	}

	j->blen = nread;

	/* intercept special channels */
	switch (udpc_pkt_cmd(JOB_PACKET(j))) {
	case 0x7573:
		snarf_syms(j);
		break;

	case 0x7574:
	case 0x7575: {
		char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
		size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
		lobidx_t c;

		for (scom_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
		     sp < ep;
		     sp += scom_tick_size(sp) *
			     (sizeof(struct sndwch_s) / sizeof(*sp))) {
			uint16_t ttf = scom_thdr_ttf(sp);
			uint16_t id = scom_thdr_tblidx(sp);
			const_sl1t_t l1t;
			struct lob_entry_s v;

			if ((c = find_cli(&j->sa, id)) == 0) {
				c = add_cli(&j->sa, id);
			}

			l1t = (const void*)sp;
			/* populate the value tables */
			v.cli = c;
			v.p = (m30_t)l1t->v[0];
			v.q = (m30_t)l1t->v[1];

			switch (ttf) {
				lobidx_t e;
			case SL1T_TTF_BID:
				/* delete the former bid first */
				if (CLI(c)->b) {
					lob_rem_at(lobb, CLI(c)->b);
				}
				/* find our spot in the lob */
				e = find_bid(v.p);
				/* and insert */
				e = lob_ins_at(lobb, e, v);
				/* update client info */
				CLI(c)->b = e;
				changep = 1;
				break;
			case SL1T_TTF_ASK:
				/* delete the former ask first */
				if (CLI(c)->a) {
					lob_rem_at(loba, CLI(c)->a);
				}
				/* find our spot in the lob */
				e = find_ask(v.p);
				/* and insert */
				e = lob_ins_at(loba, e, v);
				/* update client info */
				CLI(c)->a = e;
				changep = 1;
				break;
			case SL1T_TTF_TRA:
			case SL1T_TTF_FIX:
			case SL1T_TTF_STL:
			case SL1T_TTF_AUC:
				break;
			case SL1T_TTF_VOL:
			case SL1T_TTF_VPR:
			case SL1T_TTF_OI:
				break;

				/* snaps */
			case SSNP_FLAVOUR:
			case SBAP_FLAVOUR:
				if (CLI(c)->b) {
					lob_rem_at(lobb, CLI(c)->b);
				}
				e = find_bid(v.p);
				e = lob_ins_at(lobb, e, v);
				CLI(c)->b = e;

				if (CLI(c)->a) {
					lob_rem_at(loba, CLI(c)->a);
				}
				v.p = v.q;
				e = find_ask(v.q);
				e = lob_ins_at(loba, e, v);
				CLI(c)->a = e;
				changep = 1;
				break;

				/* candles */
			case SL1T_TTF_BID | SCOM_FLAG_LM:
			case SL1T_TTF_ASK | SCOM_FLAG_LM:
			case SL1T_TTF_TRA | SCOM_FLAG_LM:
			case SL1T_TTF_FIX | SCOM_FLAG_LM:
			case SL1T_TTF_STL | SCOM_FLAG_LM:
			case SL1T_TTF_AUC | SCOM_FLAG_LM:
				break;

			case SCOM_TTF_UNK:
			default:
				break;
			}
		}
		break;
	}
	case 0x5554:
	case 0x5555:
		/* don't want to deal with big-endian crap */
		;
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


static WINDOW *curw = NULL;

static void
init_wins(void)
{
	unsigned int nr;
	unsigned int nc;

	initscr();
	keypad(stdscr, TRUE);
	noecho();

	nr = getmaxy(stdscr);
	nc = getmaxx(stdscr);

	/* colour */
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_GREEN);

	/* box the whole screen */
	box(stdscr, 0, 0);
	move(nr - 2, 0);
	addch(ACS_LLCORNER);
	hline(0, nc - 1);
	move(nr - 2, nc - 1);
	addch(ACS_LRCORNER);

	/* also leave a note on how to exit */
	move(nr - 1, 0);
	attron(COLOR_PAIR(1));
	hline(' ', nc);
	move(nr - 1, 0);
	addstr(" Press q to quit ");

	/* start with the beef window */
	curw = newwin(nr - 3, nc - 2, 1, 1);

	/* big refreshment */
	refresh();
	return;
}

static void
fini_wins(void)
{
	delwin(curw);
	endwin();
	return;
}

static void
render_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
	unsigned int nwr = getmaxy(curw);
	unsigned int nwc = getmaxx(curw);

	if (!changep) {
		/* don't bother */
		return;
	}

	/* start with a clear window */
	wclear(curw);

	/* go through bids */
	for (lobidx_t i = lobb->head, j = 1;
	     i && j < nwr;
	     i = NEXT(lobb, i), j++) {
		char tmp[128], *p = tmp;
		lob_cli_t c = CLI(EAT(lobb, i).v.cli);

		if (c->ssz) {
			memcpy(p, c->sym, c->ssz);
			p += c->ssz;
			*p++ = ' ';
		} else {
			memcpy(p, c->ss, c->sz);
			p += c->sz;
			p += sprintf(p, " %04x ", c->id);
		}
		p += ffff_m30_s(p, EAT(lobb, i).v.q);
		*p++ = ' ';
		p += ffff_m30_s(p, EAT(lobb, i).v.p);
		*p = '\0';

		wmove(curw, j, nwc / 2 - 1 - (p - tmp));
		wprintw(curw, tmp);
	}

	for (lobidx_t i = loba->head, j = 1;
	     i && j < nwr;
	     i = NEXT(loba, i), j++) {
		char tmp[128], *p = tmp;
		lob_cli_t c = CLI(EAT(loba, i).v.cli);

		p += ffff_m30_s(p, EAT(loba, i).v.p);
		*p++ = ' ';
		p += ffff_m30_s(p, EAT(loba, i).v.q);
		if (c->ssz) {
			*p++ = ' ';
			memcpy(p, c->sym, c->ssz);
			p += c->ssz;
		} else {
			p += sprintf(p, " %04x ", c->id);
			memcpy(p, c->ss, c->sz);
			p += c->sz;
		}
		*p = '\0';

		wmove(curw, j, nwc / 2  + 1);
		wprintw(curw, tmp);
	}

	/* actually render the window */
	wrefresh(curw);

	/* reset state */
	changep = 0;

	/* and then set the timer again */
	ev_timer_again(EV_A_ w);
	return;
}

static void
keypress_cb(EV_P_ ev_io *UNUSED(w), int UNUSED(revents))
{
	switch (getch()) {
	case 'q':
		ev_unloop(EV_A_ EVUNLOOP_ALL);
	default:
		break;
	}
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "um-quomon-clo.h"
#include "um-quomon-clo.c"
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
	struct umqm_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_timer render[1];

	/* parse the command line */
	if (umqm_parser(argc, argv, argi)) {
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
	/* initialise a timer */
	ev_timer_init(render, render_cb, 0.1, 0.1);
	ev_timer_start(EV_A_ render);

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1 + 1;
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

	/* start the screen */
	init_wins();

	/* watch the terminal */
	{
		ev_io *keyp = beef + 1 + argi->beef_given;
		ev_io_init(keyp, keypress_cb, STDOUT_FILENO, EV_READ);
		ev_io_start(EV_A_ keyp);
	}

	/* init the limit order book */
	init_lob();

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* reset the screen */
	fini_wins();

	/* finish order book */
	free_lob();

	/* detaching beef channels */
	for (unsigned int i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
		ud_mcast_fini(s);
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	umqm_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* um-quomon.c ends here */
