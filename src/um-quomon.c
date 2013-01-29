/*** um-quomon.c -- unsermarkt quote monitor
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

#include "config.h"
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
#include <readline/readline.h>
#include <readline/history.h>

#include <unserding/unserding.h>

/* to get a take on them m30s and m62s */
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

/* for the limit order book */
#include <sys/mman.h>

#include <ncurses.h>

#include "svc-uterus.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

typedef uint32_t lobidx_t;
typedef struct lob_cli_s *lob_cli_t;
typedef union lob_side_u *lob_side_t;
typedef struct lob_s *lob_t;
typedef struct lob_win_s *lob_win_t;

typedef const struct sockaddr_in6 *my_sockaddr_t;

/* the client */
struct lob_cli_s {
	struct sockaddr_storage sa;
	uint16_t id;

	lobidx_t blob;
	lobidx_t alob;
	lobidx_t b;
	lobidx_t a;

	/* helpers for the renderer */
	char ss[INET6_ADDRSTRLEN + 2 + 6];
	size_t sz;

	char sym[64];
	size_t ssz;

	int mark;
	unsigned int last_seen;
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
	struct lob_entnav_s e[0];
	struct lob_entnav_s pad;

	struct {
		lobidx_t head;
		lobidx_t tail;
		lobidx_t free;
	};
};

/* all sides */
struct lob_s {
	lob_side_t lob;
	size_t alloc_sz;
};

/* order book windows */
struct lob_win_s {
	WINDOW *w;

	lobidx_t bbook;
	lobidx_t abook;

	char sym[64];
	size_t ssz;

	lobidx_t selcli;
	int selside;
};


/* we support a maximum of 64 order books atm */
static struct lob_s lob[128] = {0};
static size_t nlob = 0;
static lob_cli_t cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
static unsigned int nrend = 0;
#define NOW		(nrend)

#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)
#define EAT(y, x)	(lob[y].lob->e[x])
#define NEXT(y, x)	EAT(y, x).next
#define PREV(y, x)	EAT(y, x).prev

static void
resz_lob(lobidx_t li, size_t at_least)
{
	lob_side_t l = lob[li].lob;
	size_t old_sz = lob[li].alloc_sz;
	size_t new_sz = (at_least * sizeof(struct lob_entnav_s) + 4095) & ~4095;
	size_t last_free;
	size_t ol_nidx = old_sz / sizeof(struct lob_entnav_s);
	size_t nu_nidx = new_sz / sizeof(struct lob_entnav_s);

	if (l) {
		l = lob[li].lob = mremap(l, old_sz, new_sz, MREMAP_MAYMOVE);
	} else {
		l = lob[li].lob = mmap(NULL, new_sz, PROT_MEM, MAP_MEM, -1, 0);
	}
	lob[li].alloc_sz = new_sz;

	for (last_free = l->free;
	     last_free && NEXT(li, last_free);
	     last_free = NEXT(li, last_free));

	/* i should now point to the last guy */
	if (last_free) {
		NEXT(li, last_free) = ol_nidx + 1;
	} else {
		l->free = ol_nidx + 1;
	}
	for (last_free = ol_nidx + 1; last_free < nu_nidx - 1; last_free++) {
		NEXT(li, last_free) = last_free + 1;
	}
	return;
}

static lobidx_t
add_lob(void)
{
	const size_t ini_sz = 4096;
	lobidx_t res = nlob++;

	resz_lob(res, ini_sz / sizeof(struct lob_entnav_s));
	return res;
}

static void
rem_lob(lobidx_t li)
{
	munmap(lob[li].lob, lob[li].alloc_sz);
	lob[li].alloc_sz = 0;
	return;
}

static void
init_lob(void)
{
	/* and our client list */
	cli = mmap(NULL, 4096, PROT_MEM, MAP_MEM, -1, 0);
	alloc_cli = 4096;
	return;
}

static void
free_lob(void)
{
	/* free the lob blob first */
	for (size_t i = 0; i < nlob; i++) {
		rem_lob(i);
	}
	/* and the list of clients */
	munmap(cli, alloc_cli);
	return;
}

#if defined DEBUG_FLAG
static void
__attribute__((noinline))
check_lob(lobidx_t li)
{
	lob_t l = lob + li;
	lob_side_t ls = l->lob;
	size_t nbeef = 0;
	size_t nfree = 0;

	/* count beef list */
	for (size_t i = ls->head; i; i = NEXT(li, i)) {
		nbeef++;
	}
	/* count free list */
	for (size_t i = ls->free; i; i = NEXT(li, i)) {
		nfree++;
	}
	if (nbeef + nfree != l->alloc_sz / 4096 * (4096 / sizeof(*ls->e)) - 1) {
		endwin();
		fprintf(stderr, "\
book %u: nbeef (%zu) nfree (%zu) and alloc_sz (%zu (%zu)) don't match\n",
			li, nbeef, nfree,
			l->alloc_sz, l->alloc_sz / sizeof(*ls->e));
		abort();
	}

	/* no client must be listed twice */
	for (size_t i = ls->head; i; i = NEXT(li, i)) {
		lobidx_t ic = EAT(li, i).v.cli;

		for (size_t j = ls->head; j; j = NEXT(li, j)) {
			lobidx_t jc = EAT(li, j).v.cli;

			if (j != i && ic == jc) {
				endwin();
				fprintf(stderr, "\
DOUBLE LISTING: cli %u at both %zu and %zu\n", ic, i, j);
				abort();
			}
		}
	}

	/* entries in the chain must not be in the free list */
	for (size_t i = ls->head; i; i = NEXT(li, i)) {
		for (size_t j = ls->free; j; j = NEXT(li, j)) {
			if (j == i) {
				endwin();
				fprintf(stderr, "\
FREE AND NOT: %zu is both in the beef and the free list\n", i);
				abort();
			}
		}
	}
	return;
}
#endif	/* DEBUG_FLAG */

static lobidx_t
lob_ins_at(lobidx_t li, lobidx_t pr, struct lob_entry_s v)
{
	lobidx_t nu;
	lob_side_t s = lob[li].lob;

	if (!(nu = s->free)) {
		resz_lob(li, (lob[li].alloc_sz + 4096) / sizeof(*s->e));
		s = lob[li].lob;
		nu = s->free;
	}
	assert(nu);
	assert(nu != pr);
	assert(s->head != s->free);
	s->free = NEXT(li, s->free);

	/* populate the cell */
	EAT(li, nu).v = v;
	/* update navigators */
	if (pr) {
		lobidx_t nx = NEXT(li, pr);

		NEXT(li, nu) = nx;
		PREV(li, nu) = pr;
		NEXT(li, pr) = nu;
		if (nx) {
			PREV(li, nx) = nu;
		} else {
			/* set tail pointer also */
			s->tail = nu;
		}
	} else {
		/* ins at head */
		PREV(li, nu) = 0;
		if ((NEXT(li, nu) = s->head)) {
			PREV(li, s->head) = nu;
		} else {
			/* head == tail */
			s->tail = nu;
		}
		s->head = nu;
	}

	assert(pr != nu);
#if defined DEBUG_FLAG
	check_lob(li);
#endif	/* DEBUG_FLAG */
	return nu;
}

static void
lob_rem_at(lobidx_t li, lobidx_t idx)
{
	lob_side_t s = lob[li].lob;

	assert(idx);

	/* fix up navigators */
	if (s->head != idx) {
		assert(PREV(li, idx));

		if (NEXT(li, idx)) {
			PREV(li, NEXT(li, idx)) = PREV(li, idx);
		} else {
			/* tail pointer fucked */
			s->tail = PREV(li, idx);
		}
		NEXT(li, PREV(li, idx)) = NEXT(li, idx);
	} else {
		/* head fiddling */
		assert(s->head);

		if ((s->head = NEXT(li, idx))) {
			PREV(li, s->head) = 0;
		} else {
			/* head is naught, so is tail */
			s->tail = 0;
		}
	}

	assert(idx != s->free);
	NEXT(li, idx) = s->free;
	s->free = idx;
	assert(s->head != s->free);
	return;
}

static inline bool PURE_CONST
m30_less_p(m30_t a, m30_t b)
{
#if defined SL1T_PRC_MKT
	if (a.u == SL1T_PRC_MKT) {
		return false;
	}
#endif	/* SL1T_PRC_MKT */
	switch (a.expo - b.expo) {
	case 0:
		return a.mant < b.mant;
	case 1:
		if (UNLIKELY(a.mant == b.mant / 10000)) {
			return a.mant * 10000 < b.mant;
		}
		return a.mant < b.mant / 10000;

	case -1:
		if (UNLIKELY(a.mant / 10000 == b.mant)) {
			return a.mant < b.mant * 10000;
		}
		return a.mant / 10000 < b.mant;
	case 2:
	case 3:
		/* a is too large to be less than b */
		return false;
	case -2:
	case -3:
		/* a is too small to not be less than b */
		return true;
	default:
		break;
	}
	/* should not be reached */
	assert(0 == 1);
	return false;
}

static inline bool PURE_CONST
m30_eq_p(m30_t a, m30_t b)
{
	switch (a.expo - b.expo) {
	case 0:
		return a.mant == b.mant;
	case 1:
		if (UNLIKELY(a.mant == b.mant / 10000)) {
			return a.mant * 10000 == b.mant;
		}
		return false;

	case -1:
		if (UNLIKELY(a.mant / 10000 == b.mant)) {
			return a.mant == b.mant * 10000;
		}
		return false;
	case 2:
	case -2:
	case 3:
	case -3:
		/* a or b is too large to equal b or a */
		return false;
	default:
		break;
	}
	/* we're fucked when we get here */
	assert(0 == 1);
	return false;
}

static lobidx_t
find_quote(lobidx_t li, m30_t ref)
{
	lobidx_t idx = 0;
	lob_side_t l = lob[li].lob;

#if defined DEBUG_FLAG
	check_lob(li);
#endif	/* DEBUG_FLAG */

	for (size_t i = l->head; i; idx = i, i = NEXT(li, i)) {
		m30_t p = EAT(li, i).v.p;
		if (m30_less_p(p, ref)) {
			return idx;
		} else if (m30_eq_p(p, ref)) {
			return i;
		}
	}
	return idx;
}

static int
sa_eq_p(my_sockaddr_t sa1, my_sockaddr_t sa2)
{
	const size_t s6sz = sizeof(sa1->sin6_addr);
	return sa1->sin6_family == sa2->sin6_family &&
		sa1->sin6_port == sa2->sin6_port &&
		memcmp(&sa1->sin6_addr, &sa2->sin6_addr, s6sz) == 0;
}

static lobidx_t
find_cli(const struct sockaddr *sa, uint16_t id)
{
	if (UNLIKELY(sa->sa_family != AF_INET6)) {
		return 0U;
	}
	for (size_t i = 0; i < ncli; i++) {
		lob_cli_t c = cli + i;
		my_sockaddr_t cur_sa = (const void*)&c->sa;
		uint16_t cur_id = c->id;

		if (sa_eq_p(cur_sa, (my_sockaddr_t)sa) && cur_id == id) {
			return i + 1;
		}
	}
	return 0U;
}

static lobidx_t
add_cli(const struct sockaddr *sa, uint16_t id)
{
	size_t idx;
#define CATCHALL_BIDLOB	(0)
#define CATCHALL_ASKLOB	(1)

	if (UNLIKELY(sa->sa_family != AF_INET6)) {
		return 0U;
	}

	idx = ncli++;
	if (ncli * sizeof(*cli) > alloc_cli) {
		size_t nu = alloc_cli + 4096;
		cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
		alloc_cli = nu;
	}

	cli[idx].sa = *(const struct sockaddr_storage*)sa;
	cli[idx].id = id;
	cli[idx].blob = CATCHALL_BIDLOB;
	cli[idx].alob = CATCHALL_ASKLOB;
	cli[idx].b = 0;
	cli[idx].a = 0;
	cli[idx].ssz = 0;
	cli[idx].mark = 0;
	cli[idx].last_seen = 0;

	/* obtain the address in human readable form */
	{
		char *epi = cli[idx].ss;
		my_sockaddr_t mysa = (const void*)sa;
		int fam = mysa->sin6_family;
		uint16_t port = htons(mysa->sin6_port);

		*epi++ = '[';
		if (inet_ntop(fam, sa, epi, sizeof(cli->ss))) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
		cli[idx].sz = epi - cli[idx].ss;
	}
	return idx + 1;
}

static void
snarf_syms(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
	struct um_qmeta_s brg[1];
	uint16_t idx;
	lobidx_t c;

	if (UNLIKELY(um_chck_msg_brag(brg, msg) < 0)) {
		return;
	} else if (UNLIKELY((idx = (uint16_t)brg->idx) == 0U)) {
		return;
	} else if (UNLIKELY(brg->sym == NULL)) {
		return;
	}
	
	/* find the cli, if any */
	if ((c = find_cli(aux->src, idx)) == 0) {
		c = add_cli(aux->src, idx);
	}
	/* check the symbol */
	if (UNLIKELY(brg->symlen > sizeof(CLI(c)->sym))) {
		brg->symlen = sizeof(CLI(c)->sym);
	}
	/* fill in symbol */
	CLI(c)->ssz = brg->symlen;
	memcpy(CLI(c)->sym, brg->sym, brg->symlen);
	return;
}

/* variable through which we communicate with the updater below */
static int changep = 0;

static void
snarf_tick(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
	struct sndwch_s ss[4];
	const_sl1t_t sp = (void*)ss;
	uint16_t idx;
	uint16_t ttf;
	lobidx_t c;

	switch (msg->dlen) {
	case sizeof(struct sl1t_s):
	case sizeof(struct scdl_s):
	case sizeof(ss):
		memcpy(ss, msg->data, msg->dlen);
		break;
	default:
		/* out of range */
		return;
	}

	/* check for idx validity */
	if (UNLIKELY((idx = scom_thdr_tblidx(AS_SCOM(sp))) == 0U)) {
		return;
	}

	/* check whether we support the tick type */
	switch ((ttf = scom_thdr_ttf(AS_SCOM(sp)))) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
		break;

	case SL2T_TTF_BID:
	case SL2T_TTF_ASK:
		/* we really ought to support these */
		return;

	case SSNP_FLAVOUR:
	case SBAP_FLAVOUR:
		break;

	case SL1T_TTF_TRA:
	case SL1T_TTF_FIX:
	case SL1T_TTF_STL:
	case SL1T_TTF_AUC:
#if defined SL1T_TTF_G32
	case SL1T_TTF_G32:
#endif	/* SL1T_TTF_G32 */
		return;

	case SL1T_TTF_VOL:
	case SL1T_TTF_VPR:
	case SL1T_TTF_OI:
#if defined SL1T_TTF_G64
	case SL1T_TTF_G64:
#endif	/* SL1T_TTF_G64 */
		return;

		/* candles */
	case SL1T_TTF_BID | SCOM_FLAG_LM:
	case SL1T_TTF_ASK | SCOM_FLAG_LM:
	case SL1T_TTF_TRA | SCOM_FLAG_LM:
	case SL1T_TTF_FIX | SCOM_FLAG_LM:
	case SL1T_TTF_STL | SCOM_FLAG_LM:
	case SL1T_TTF_AUC | SCOM_FLAG_LM:
		return;

	case SCOM_TTF_UNK:
	default:
		return;
	}

	/* find the associated cli, or create one */
	if ((c = find_cli(aux->src, idx)) == 0) {
		c = add_cli(aux->src, idx);
	}

	/* update last seen */
	CLI(c)->last_seen = NOW;

	{
		struct lob_entry_s v;

		/* populate the value tables */
		v.cli = c;
		v.p = (m30_t)sp->v[0];
		v.q = (m30_t)sp->v[1];

		switch (ttf) {
			lobidx_t e;
			lobidx_t book;
		case SL1T_TTF_BID:
			/* get the book we're talking */
			book = CLI(c)->blob;
			/* delete the former bid first */
			if (CLI(c)->b) {
				lob_rem_at(book, CLI(c)->b);
			}
			/* find our spot in the lob */
			e = find_quote(book, v.p);
			/* and insert */
			CLI(c)->b = lob_ins_at(book, e, v);
			changep = 1;
			break;
		case SL1T_TTF_ASK:
			/* get the book we're talking */
			book = CLI(c)->alob;
			/* delete the former ask first */
			if (CLI(c)->a) {
				lob_rem_at(book, CLI(c)->a);
			}
			/* find our spot in the lob */
			e = find_quote(book, v.p);
			/* and insert */
			CLI(c)->a = lob_ins_at(book, e, v);
			changep = 1;
			break;

		case SL2T_TTF_BID:
		case SL2T_TTF_ASK:
			/* support me, i'm hungry */
			break;

			/* snaps */
		case SSNP_FLAVOUR:
		case SBAP_FLAVOUR:
			/* get the book we're talking */
			book = CLI(c)->blob;
			if (CLI(c)->b) {
				lob_rem_at(book, CLI(c)->b);
			}
			e = find_quote(book, v.p);
			CLI(c)->b = lob_ins_at(book, e, v);

			/* get the other book */
			book = CLI(c)->alob;
			if (CLI(c)->a) {
				lob_rem_at(book, CLI(c)->a);
			}
			v.p = v.q;
			e = find_quote(book, v.q);
			CLI(c)->a = lob_ins_at(book, e, v);
			changep = 1;
			break;

		default:
			/* haha as if */
			break;
		}
	}
	return;
}


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
			snarf_syms(msg, aux);
			break;

		case UTE_CMD:
			snarf_tick(msg, aux);
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
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}


static struct lob_win_s __gwins[countof(lob) / 2];
static size_t __ngwins = 0;
static lobidx_t curw = -1;
#define CURW		(__gwins[curw].w)
#define BIDLOB(i)	(__gwins[i].bbook)
#define ASKLOB(i)	(__gwins[i].abook)

#define JUST_RED	1
#define JUST_GREEN	2
#define JUST_YELLOW	3
#define JUST_BLUE	4
#define CLISEL		5
#define CLIMARK		6
#define STATUS		7

static void
init_lobwin(lobidx_t li)
{
	unsigned int nr = getmaxy(stdscr);
	unsigned int nc = getmaxx(stdscr);

	__gwins[li].w = newwin(nr - 1, nc, 0, 0);
	return;
}

static void
fini_lobwin(lobidx_t li)
{
	if (__gwins[li].w) {
		delwin(__gwins[li].w);
		__gwins[li].w = NULL;
	}
	return;
}

static void
rem_lobwin(lobidx_t li)
{
	fini_lobwin(li);
	__gwins[li].ssz = 0;

	rem_lob(__gwins[li].bbook);
	rem_lob(__gwins[li].abook);
	return;
}

static lobidx_t
add_lobwin(const char *name)
{
	lobidx_t res = __ngwins++;

	/* just to make sure */
	fini_lobwin(res);
	/* get the new one */
	init_lobwin(res);

	if (name) {
		size_t sz = strlen(name);

		if (sz > countof(__gwins->sym)) {
			sz = countof(__gwins->sym) - 1;
		}
		memcpy(__gwins[res].sym, name, sz);
		__gwins[res].sym[sz] = '\0';
		__gwins[res].ssz = sz;
	} else {
		__gwins[res].sym[0] = '\0';
		__gwins[res].ssz = 0;
	}

	/* oh, and get us some order books */
	__gwins[res].bbook = add_lob();
	__gwins[res].abook = add_lob();

	__gwins[res].selcli = 0;
	__gwins[res].selside = 0;
	return res;
}

static void
render_scr(void)
{
	unsigned int nr = getmaxy(stdscr);
	unsigned int nc = getmaxx(stdscr);

	/* also leave a note on how to exit */
	move(nr - 1, 0);
	attron(COLOR_PAIR(STATUS));
	hline(' ', nc);
	move(nr - 1, 0);
	addstr(" Press q to quit ");
	attrset(A_NORMAL);

	for (size_t i = 0; i < __ngwins; i++) {
		/* delete old beef window */
		fini_lobwin(i);
		/* start with the beef window */
		init_lobwin(i);
	}

	/* big refreshment */
	refresh();
	return;
}

static void
init_wins(void)
{
	initscr();
	keypad(stdscr, TRUE);
	noecho();

	/* colour */
	start_color();
	use_default_colors();
	init_pair(JUST_RED, COLOR_RED, -1);
	init_pair(JUST_GREEN, COLOR_GREEN, -1);
	init_pair(JUST_YELLOW, COLOR_YELLOW, -1);
	init_pair(JUST_BLUE, COLOR_BLUE, -1);
	init_pair(CLISEL, COLOR_BLACK, COLOR_YELLOW);
	init_pair(CLIMARK, 2, -1);
	init_pair(STATUS, COLOR_BLACK, COLOR_GREEN);

	/* instantiate the catch-all window */
	curw = add_lobwin("ALL");
	return;
}

static void
fini_wins(void)
{
	for (size_t i = 0; i < __ngwins; i++) {
		rem_lobwin(i);
	}
	endwin();
	return;
}

static void
render_win(lobidx_t wi)
{
	lob_win_t w = __gwins + wi;
	const unsigned int nwr = getmaxy(w->w);
	const unsigned int nwc = getmaxx(w->w);

	/* start with a clear window */
	wclear(w->w);

	/* box with the name */
	box(w->w, 0, 0);

	/* draw the window `tabs' */
	wmove(w->w, 0, 4);
	for (size_t i = 0; i < __ngwins; i++) {
		if (w->ssz) {
			if (i == curw) {
				wattron(w->w, COLOR_PAIR(CLISEL));
			}
			waddstr(w->w, __gwins[i].sym);
			wattrset(w->w, A_NORMAL);
			waddch(w->w, ' ');
		}
	}

	/* check if selection points to pruned cli */
	if (w->selcli && UNLIKELY(!CLI(w->selcli)->b && !CLI(w->selcli)->a)) {
		w->selcli = 0;
	}
	/* check if we've got a selection */
	if (w->selcli == 0) {
		lob_side_t s;

		/* just select anything in this case */
		if ((s = lob[BIDLOB(wi)].lob)->head) {
			w->selcli = EAT(BIDLOB(wi), s->head).v.cli;
		} else if ((s = lob[ASKLOB(wi)].lob)->head) {
			w->selcli = EAT(ASKLOB(wi), s->head).v.cli;
		} else {
			/* tough luck */
			;
		}
	}

#if defined DEBUG_FLAG
	check_lob(BIDLOB(wi));
	check_lob(ASKLOB(wi));
#endif	/* DEBUG_FLAG */

	/* go through bids */
	for (size_t i = lob[BIDLOB(wi)].lob->head, j = 1;
	     i && j < nwr - 1;
	     i = NEXT(BIDLOB(wi), i), j++) {
		char tmp[128], *p = tmp;
		lobidx_t c = EAT(BIDLOB(wi), i).v.cli;
		lob_cli_t cp = CLI(c);

		if (cp->ssz) {
			memcpy(p, cp->sym, cp->ssz);
			p += cp->ssz;
			*p++ = ' ';
		} else {
			memcpy(p, cp->ss, cp->sz);
			p += cp->sz;
			p += sprintf(p, " %04x ", cp->id);
		}
		p += ffff_m30_s(p, EAT(BIDLOB(wi), i).v.q);
		*p++ = ' ';
		p += ffff_m30_s(p, EAT(BIDLOB(wi), i).v.p);
		*p = '\0';

		wmove(w->w, j, nwc / 2 - 1 - (p - tmp));
		if (c == w->selcli && w->selside == 0) {
			wattron(w->w, COLOR_PAIR(CLISEL));
		} else if (c == w->selcli) {
			wattron(w->w, A_STANDOUT);
		} else if (CLI(c)->mark) {
			wattron(w->w, COLOR_PAIR(CLIMARK));
		}
		waddstr(w->w, tmp);
		wattrset(w->w, A_NORMAL);
	}

	for (size_t i = lob[ASKLOB(wi)].lob->tail, j = 1;
	     i && j < nwr - 1;
	     i = PREV(ASKLOB(wi), i), j++) {
		char tmp[128], *p = tmp;
		lobidx_t c = EAT(ASKLOB(wi), i).v.cli;
		lob_cli_t cp = CLI(c);

		p += ffff_m30_s(p, EAT(ASKLOB(wi), i).v.p);
		*p++ = ' ';
		p += ffff_m30_s(p, EAT(ASKLOB(wi), i).v.q);
		if (cp->ssz) {
			*p++ = ' ';
			memcpy(p, cp->sym, cp->ssz);
			p += cp->ssz;
		} else {
			p += sprintf(p, " %04x ", cp->id);
			memcpy(p, cp->ss, cp->sz);
			p += cp->sz;
		}
		*p = '\0';

		wmove(w->w, j, nwc / 2  + 1);
		if (c == w->selcli && w->selside == 1) {
			wattron(w->w, COLOR_PAIR(CLISEL));
		} else if (c == w->selcli) {
			wattron(w->w, A_STANDOUT);
		} else if (CLI(c)->mark) {
			wattron(w->w, COLOR_PAIR(CLIMARK));
		}
		waddstr(w->w, tmp);
		wattrset(w->w, A_NORMAL);
	}

	/* actually render the window */
	wmove(w->w, nwr - 1, nwc - 1);
	wrefresh(w->w);

	/* reset state */
	changep = 0;
	return;
}

static void
prune_clis(void)
{
/* max age of ticks in renderings */
#define MAX_AGE		(600U)
	for (size_t c = 1; c <= ncli; c++) {
		if (CLI(c)->last_seen + MAX_AGE < NOW) {
			/* client needs kicking */
			if (CLI(c)->b) {
				lob_rem_at(CLI(c)->blob, CLI(c)->b);
				CLI(c)->b = 0;
			}
			if (CLI(c)->a) {
				lob_rem_at(CLI(c)->alob, CLI(c)->a);
				CLI(c)->a = 0;
			}
		}
	}
	return;
}


static WINDOW *symw = NULL;
static const char symw_prompt[] = " Enter symbol:";

static void
render_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
	/* don't bother if nothing's changed */
	if (changep) {
		/* render the current window */
		render_win(curw);

		if (UNLIKELY(symw != NULL)) {
			wrefresh(symw);
		}
	}

	/* prune old clients */
	prune_clis();
	/* update the rendering counter*/
	nrend++;
	/* and then set the timer again */
	ev_timer_again(EV_A_ w);
	return;
}

static void
sigwinch_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	render_scr();
	render_win(curw);
	return;
}

static void
reass_cli(lobidx_t ci, lobidx_t wi)
{
	/* rem from the old books */
	assert(CLI(ci)->b || CLI(ci)->a);
	if (CLI(ci)->b) {
		lobidx_t book = CLI(ci)->blob;

		assert(EAT(book, CLI(ci)->b).v.cli == ci);
		lob_rem_at(book, CLI(ci)->b);
	}
	if (CLI(ci)->a) {
		lobidx_t book = CLI(ci)->alob;

		assert(EAT(book, CLI(ci)->a).v.cli == ci);
		lob_rem_at(book, CLI(ci)->a);
	}

	/* assert */
	assert(ASKLOB(wi) == BIDLOB(wi) + 1);

	/* find the entry in the new book */
	if (CLI(ci)->b) {
		lobidx_t ol_book = CLI(ci)->blob;
		lobidx_t nu_book = BIDLOB(wi);
		struct lob_entry_s v = EAT(ol_book, CLI(ci)->b).v;
		lobidx_t e;

		assert(v.cli == ci);
		e = find_quote(nu_book, v.p);
		CLI(ci)->b = lob_ins_at(nu_book, e, v);
	}
	if (CLI(ci)->a) {
		lobidx_t ol_book = CLI(ci)->alob;
		lobidx_t nu_book = ASKLOB(wi);
		struct lob_entry_s v = EAT(ol_book, CLI(ci)->a).v;
		lobidx_t e;

		assert(v.cli == ci);
		e = find_quote(nu_book, v.p);
		CLI(ci)->a = lob_ins_at(nu_book, e, v);
	}

	/* assign the new books */
	CLI(ci)->blob = BIDLOB(wi);
	CLI(ci)->alob = ASKLOB(wi);

	/* unmark client */
	CLI(ci)->mark = 0;

	/* unassign the currently selected client */
	__gwins[curw].selcli = 0;
	return;
}

static void
reass_clis(lobidx_t wi)
{
	int markp = 0;

	/* if there's no marks, just use the currently selected cli */
	for (size_t i = 1; i <= ncli; i++) {
		if (CLI(i)->mark) {
			reass_cli(i, wi);
			markp = 1;
		}
	}
	if (!markp) {
		/* no marks found, reass the current selection */
		reass_cli(__gwins[curw].selcli, wi);
	}
	return;
}

/* readline glue */
static void
handle_el(char *line)
{
	lobidx_t wi;

	/* print newline */
	if (UNLIKELY(line == NULL || line[0] == '\0' || line[0] == ' ')) {
		goto out;
	}

	/* stuff up our history */
	add_history(line);

	/* quick check */
	for (size_t i = 0; i < __ngwins; i++) {
		if (strcmp(line, __gwins[i].sym) == 0) {
			wi = i;
			goto reass;
		}
	}
	/* otherwise we need to create a window */
	wi = add_lobwin(line);
reass:
	/* the selected client gets a new lob */
	reass_clis(wi);

out:
	/* ah, user entered something? */
	delwin(symw);
	symw = NULL;
	/* and free him */
	free(line);

	/* redraw it all */
	sigwinch_cb(NULL, NULL, 0);
	return;
}

static void
keypress_cb(EV_P_ ev_io *UNUSED(io), int UNUSED(revents))
{
	int k;
	lob_win_t w = __gwins + curw;

	if (UNLIKELY(symw != NULL)) {
		rl_callback_read_char();

		/* rebuild and print the string, John Greco's trick */
		wmove(symw, 0, sizeof(symw_prompt));
		wclrtoeol(symw);
		mvwprintw(symw, 0, sizeof(symw_prompt), rl_line_buffer);
		wmove(symw, 0, sizeof(symw_prompt) + rl_point);
		wrefresh(symw);
		return;
	}

	switch ((k = getch())) {
	case 'q':
		ev_unloop(EV_A_ EVUNLOOP_ALL);
		break;

		/* flick between windows */
	case '\t':
		if (++curw >= __ngwins) {
			curw = 0;
		}
		goto redraw;
	case KEY_BTAB:
		if (curw-- == 0) {
			curw = __ngwins - 1;
		}
		goto redraw;

		/* marking */
	case ' ':
		CLI(w->selcli)->mark = !CLI(w->selcli)->mark;
		/* pretend we was a key_down */
		k = KEY_DOWN;
		/* fallthrough */
	case KEY_UP:
	case KEY_DOWN:
		if (w->selcli) {
			lobidx_t side;
			lobidx_t qidx;
			lobidx_t nu;
			int prevnext;

			if (w->selside == 0) {
				side = BIDLOB(curw);
				qidx = CLI(w->selcli)->b;
				prevnext = 0;
			} else {
				side = ASKLOB(curw);
				qidx = CLI(w->selcli)->a;
				prevnext = 1;
			}

			if (k == KEY_UP && prevnext == 0 ||
			    k == KEY_DOWN && prevnext == 1) {
				nu = EAT(side, qidx).prev;
			} else if (k == KEY_DOWN && prevnext == 0 ||
				   k == KEY_UP && prevnext == 1) {
				nu = EAT(side, qidx).next;
			}

			if (nu) {
				w->selcli = EAT(side, nu).v.cli;
				goto redraw;
			}
		}
		break;
	case KEY_RIGHT:
		if (w->selside == 0) {
			w->selside = 1;
			goto redraw;
		}
		break;
	case KEY_LEFT:
		if (w->selside == 1) {
			w->selside = 0;
			goto redraw;
		}
		break;

	case '\n': {
		unsigned int nr = getmaxy(stdscr);
		unsigned int nc = getmaxx(stdscr);

		symw = newwin(1, nc, nr - 1, 0);
		wmove(symw, 0, 0);
		wattrset(symw, COLOR_PAIR(STATUS));
		wprintw(symw, symw_prompt);
		wattrset(symw, A_NORMAL);
		waddch(symw, ' ');
		wrefresh(symw);
		break;
	}
	default:
		break;
	}
	return;

redraw:
	changep = 1;
	return;
}

static void
init_rl(void)
{
	rl_initialize();
	rl_already_prompted = 1;
	rl_readline_name = "unsermarkt";
#if 0
	rl_attempted_completion_function = udcli_comp;
#endif	/* 0 */

	rl_basic_word_break_characters = "\t\n@$><=;|&{( ";
	rl_catch_signals = 0;

#if 0
	/* load the history file */
	(void)read_history(histfile);
	history_set_pos(history_length);
#endif	/* 0 */

	/* just install the handler */
	rl_callback_handler_install("", handle_el);
	return;
}

static void
fini_rl(void)
{
	rl_callback_handler_remove();

#if 0
	/* save the history file */
	(void)write_history(histfile);
#endif	/* 0 */
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
	ev_signal sigwinch_watcher[1];
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
	/* initialise a SIGWINCH handler */
	ev_signal_init(sigwinch_watcher, sigwinch_cb, SIGWINCH);
	ev_signal_start(EV_A_ sigwinch_watcher);
	/* initialise a timer */
	{
		double slp = 1.0 / (double)argi->fps_arg;
		ev_timer_init(render, render_cb, slp, slp);
		ev_timer_start(EV_A_ render);
	}

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1 + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
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

	/* start the screen */
	init_wins();
	/* init readline */
	init_rl();

	/* watch the terminal */
	{
		ev_io *keyp = beef + 1 + argi->beef_given;
		ev_io_init(keyp, keypress_cb, STDOUT_FILENO, EV_READ);
		ev_io_start(EV_A_ keyp);
	}

	/* init the limit order book */
	init_lob();

	/* give him a sigwinch, so everything gets rerendered */
	sigwinch_cb(EV_A_ sigwinch_watcher, 0);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* and readline too */
	fini_rl();
	/* reset the screen */
	fini_wins();

	/* finish order book */
	free_lob();

	/* detaching beef channels */
	for (unsigned int i = 0; i < nbeef; i++) {
		ud_sock_t s;

		if ((s = beef[i].data) != NULL) {
			ev_io_stop(EV_A_ beef + i);
			ud_close(s);
		}
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	umqm_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* um-quomon.c ends here */
