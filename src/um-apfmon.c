/*** um-apfmon.c -- unsermarkt quote monitor
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

#include <readline/readline.h>
#include <readline/history.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

/* for our memory management */
#include <sys/mman.h>

#include <ncurses.h>

#include "nifty.h"
#include "gq.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#define PURE		__attribute__((pure))
#define PURE_CONST	__attribute__((const, pure))

#define ALGN(x, to)	x __attribute__((aligned(to)))

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
# define UMAM_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define UMAM_DEBUG(args...)
#endif	/* DEBUG_FLAG */


typedef size_t cli_t;
typedef size_t widx_t;
typedef struct pfi_s *pfi_t;
typedef struct win_s *win_t;

struct key_s {
	ud_sockaddr_t sa;
};

/* one portfolio item */
struct pfi_s {
	struct gq_item_s ALGN(i, 16);

	/* symbol, not as long as usual */
	char sym[32];
	double lqty;
	double sqty;
};

/* the client */
struct cli_s {
	union ud_sockaddr_u ALGN(sa, 16);

	/* helpers for the renderer */
	char ss[INET6_ADDRSTRLEN + 2 + 6];
	size_t sssz;

	/* portfolio name */
	char pf[32];
	size_t pfsz;

	int mark;
	unsigned int last_seen;

	/* all them positions */
	struct gq_s pool[1];
	struct gq_ll_s poss[1];
};

/* portfolio windows */
struct win_s {
	WINDOW *w;

	char name[32];
	size_t namesz;

	cli_t sel;
};


/* we support a maximum of 64 order books atm */
static struct cli_s *cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
static unsigned int nrend = 0;
#define NOW		(nrend)

#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)

#if defined MAP_ANON && !defined MAP_ANONYMOUS
# define MAP_ANONYMOUS	MAP_ANON
#endif	/* MAP_ANON && !MAP_ANONYMOUS */
#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

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
resz_cli(size_t nu)
{
	cli = mremap(cli, alloc_cli, nu, MREMAP_MAYMOVE);
	alloc_cli = nu;
	return;
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
	return CLI(c)->sa.sa6.sin6_family == 0;
}

static void
prune_clis(void)
{
	size_t nu_ncli = ncli;

	/* prune clis */
	for (cli_t i = 1, ei = ncli; i <= ei; i++) {
		if (CLI(i)->last_seen + MAX_CLI_AGE < NOW) {
			UMAM_DEBUG("pruning %zu\n", i);
			prune_cli(i);
		}
	}

	/* condense the cli array a bit */
	for (cli_t i = 1, ei = ncli; i <= ei; i++) {
		size_t consec;

		for (consec = 0; i <= ei && cli_pruned_p(i); i++) {
			consec++;
		}
		assert(consec <= ei);
		assert(consec <= i);
		if (consec && i <= ei) {
			/* shrink */
			size_t nmv = CLI(i) - CLI(i - consec);

			UMAM_DEBUG("condensing %zu/%zu clis\n", nmv, ei);
			memcpy(CLI(i - consec), CLI(i), nmv * sizeof(*cli));
			nu_ncli -= nmv;
		} else if (consec) {
			UMAM_DEBUG("condensing %zu/%zu clis\n", consec, ei);
			nu_ncli -= consec;
		}
	}

	/* let everyone know how many clis we've got */
	ncli = nu_ncli;
	return;
}

static void
check_poss(cli_t c)
{
	gq_t UNUSED(q) = CLI(c)->pool;
	gq_ll_t UNUSED(ll) = CLI(c)->poss;

#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = q->free->i1st; ip; ip = ip->next, ni++);
	for (gq_item_t ip = ll->i1st; ip; ip = ip->next, ni++);
	assert(ni == q->nitems / sizeof(struct pfi_s));

	ni = 0;
	for (gq_item_t ip = q->free->ilst; ip; ip = ip->prev, ni++);
	for (gq_item_t ip = ll->ilst; ip; ip = ip->prev, ni++);
	assert(ni == q->nitems / sizeof(struct pfi_s));
#endif	/* DEBUG_FLAG */
	return;
}


static int
sa_eq_p(ud_sockaddr_t sa1, ud_sockaddr_t sa2)
{
	const size_t s6sz = sizeof(sa1->sa6.sin6_addr);
	return sa1->sa6.sin6_family == sa2->sa6.sin6_family &&
		sa1->sa6.sin6_port == sa2->sa6.sin6_port &&
		memcmp(&sa1->sa6.sin6_addr, &sa2->sa6.sin6_addr, s6sz) == 0;
}

static cli_t
find_cli(struct key_s k)
{
	for (size_t i = 1; i <= ncli; i++) {
		ud_sockaddr_t cur_sa = &CLI(i)->sa;

		if (sa_eq_p(cur_sa, k.sa)) {
			return i;
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
	cli[idx].last_seen = 0;

	/* obtain the address in human readable form */
	{
		char *epi = cli[idx].ss;
		int fam = ud_sockaddr_fam(k.sa);
		const struct sockaddr *addr = ud_sockaddr_addr(k.sa);
		uint16_t port = ud_sockaddr_port(k.sa);

		*epi++ = '[';
		if (inet_ntop(fam, addr, epi, sizeof(cli->ss))) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
		cli[idx].sssz = epi - cli[idx].ss;
	}

	/* queue needs no init'ing, we use lazy adding */
	return idx + 1;
}

static pfi_t
pop_pfi(cli_t c)
{
	pfi_t res;
	gq_t q = CLI(c)->pool;

	if (q->free->i1st == NULL) {
		size_t nitems = q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(q->free->ilst == NULL);
		UMAM_DEBUG("q resize %zu -> %zu\n", nitems, nitems + 64);
		df = init_gq(q, sizeof(*res), nitems + 64);
		gq_rbld_ll(CLI(c)->poss, df);
		check_poss(c);
	}
	/* get us a new portfolio item */
	res = (void*)gq_pop_head(q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static pfi_t
find_pos(cli_t c, const char *sym)
{
	gq_ll_t q = CLI(c)->poss;

	for (gq_item_t i = q->i1st; i; i = i->next) {
		pfi_t pos = (void*)i;

		if (strcmp(pos->sym, sym) == 0) {
			return pos;
		}
	}
	return NULL;
}


/* fix guts */
static size_t
find_fix_fld(char **p, char *msg, const char *key)
{
#define SOH	"\001"
	char *cand = msg - 1;
	char *eocand;

	while ((cand = strstr(cand + 1, key)) && cand != msg && cand[-1] != *SOH);
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

static char*
find_fix_eofld(char *msg, const char *key)
{
#define SOH	"\001"
	char *cand;
	size_t clen;

	if (UNLIKELY((clen = find_fix_fld(&cand, msg, key)) == 0)) {
		return NULL;
	} else if (UNLIKELY(cand[++clen] == '\0')) {
		return NULL;
	}
	return cand + clen;
}

static double
find_fix_dbl(char *msg, const char *fld, size_t nfld)
{
	size_t tmp;
	double res = 0.0;
	char *p;

	if ((tmp = find_fix_fld(&p, msg, fld))) {
		char bkup = p[tmp];
		const char *cursor = p + nfld;

		p[tmp] = '\0';
		res = strtod(cursor, NULL);
		p[tmp] = bkup;
	}
	return res;
}

static void
pr_pos_rpt(job_t j)
{
/* process them posrpts */
	static const char fix_pos_rpt[] = "35=AP";
	static const char fix_chksum[] = "10=";
	static const char fix_inssym[] = "55=";
	static const char fix_lqty[] = "704=";
	static const char fix_sqty[] = "705=";
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct key_s k = {
		.sa = &j->sa,
	};
	cli_t c;

	if (UNLIKELY(plen == 0)) {
		return;
	} else if ((c = find_cli(k))) {
		;
	} else if ((c = add_cli(k))) {
		;
	} else {
		/* fuck */
		return;
	}

	/* update last seen */
	CLI(c)->last_seen = NOW;

	for (char *p = pbuf, *ep = pbuf + plen;
	     p && p < ep && (p = find_fix_eofld(p, fix_pos_rpt));
	     p = find_fix_eofld(p, fix_chksum)) {
		struct pfi_s *pos = NULL;
		size_t tmp;
		char *sym;

		if ((tmp = find_fix_fld(&sym, p, fix_inssym)) == 0) {
			/* great, we NEED that symbol */
			continue;
		}
		/* ffw sym */
		sym += sizeof(fix_inssym) - 1;
		tmp -= sizeof(fix_inssym) - 1;
		/* we don't want no steenkin buffer overfloes */
		if (UNLIKELY(tmp >= sizeof(pos->sym))) {
			tmp = sizeof(pos->sym) - 1;
		}
		if ((pos = find_pos(c, sym))) {
			/* nothing to do */
			;
		} else if ((pos = pop_pfi(c))) {
			/* all's fine, copy the sym */
			memcpy(pos->sym, sym, tmp);
			pos->sym[tmp] = '\0';
		} else {
			/* big fuck up */
			continue;
		}

		/* find the long quantity */
		pos->lqty = find_fix_dbl(p, fix_lqty, sizeof(fix_lqty) - 1);
		pos->sqty = find_fix_dbl(p, fix_sqty, sizeof(fix_sqty) - 1);

		fprintf(stdout, "POSRPT\t%s\n", pos->sym);
	}
	return;
}


/* the actual worker function */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))
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
	case POS_RPT:
	case POS_RPT_RPL:
		/* parse the message here */
		pr_pos_rpt(j);
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


static struct win_s __gwins[1];
static size_t __ngwins = 0;
static size_t curw = -1UL;
#define CURW		(__gwins[curw].w)

#define JUST_RED	1
#define JUST_GREEN	2
#define JUST_YELLOW	3
#define JUST_BLUE	4
#define CLISEL		5
#define CLIMARK		6
#define STATUS		7

static void
init_win(widx_t li)
{
	unsigned int nr = getmaxy(stdscr);
	unsigned int nc = getmaxx(stdscr);

	__gwins[li].w = newwin(nr - 1, nc, 0, 0);
	return;
}

static void
fini_win(widx_t li)
{
	if (__gwins[li].w) {
		delwin(__gwins[li].w);
		__gwins[li].w = NULL;
	}
	return;
}

static void
rem_win(widx_t li)
{
	fini_win(li);
	__gwins[li].namesz = 0;

	//rem_lob(__gwins[li].bbook);
	//rem_lob(__gwins[li].abook);
	return;
}

static widx_t
add_win(const char *name)
{
	widx_t res = __ngwins++;

	/* just to make sure */
	fini_win(res);
	/* get the new one */
	init_win(res);

	if (name) {
		size_t sz = strlen(name);

		if (sz > countof(__gwins->name)) {
			sz = countof(__gwins->name) - 1;
		}
		memcpy(__gwins[res].name, name, sz);
		__gwins[res].name[sz] = '\0';
		__gwins[res].namesz = sz;
	} else {
		__gwins[res].name[0] = '\0';
		__gwins[res].namesz = 0;
	}

	/* oh, and get us some order books */
	//__gwins[res].bbook = add_lob();
	//__gwins[res].abook = add_lob();

	__gwins[res].sel = 0;
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
		fini_win(i);
		/* start with the beef window */
		init_win(i);
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
	curw = add_win("ALL");
	return;
}

static void
fini_wins(void)
{
	for (size_t i = 0; i < __ngwins; i++) {
		rem_win(i);
	}
	endwin();
	return;
}

static void
render_win(widx_t wi)
{
	win_t w = __gwins + wi;
	const unsigned int nwr = getmaxy(w->w);
	const unsigned int nwc = getmaxx(w->w);

	/* start with a clear window */
	wclear(w->w);

	/* box with the name */
	box(w->w, 0, 0);

	/* draw the window `tabs' */
	wmove(w->w, 0, 4);
	for (size_t i = 0; i < __ngwins; i++) {
		if (w->namesz) {
			if (i == curw) {
				wattron(w->w, COLOR_PAIR(CLISEL));
			}
			waddstr(w->w, __gwins[i].name);
			wattrset(w->w, A_NORMAL);
			waddch(w->w, ' ');
		}
	}

	/* actual rendering goes here */
	;

	/* actually render the window */
	wmove(w->w, nwr - 1, nwc - 1);
	wrefresh(w->w);

	/* reset state */
	changep = 0;
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
reass_cli(cli_t ci, widx_t UNUSED(wi))
{
	/* unmark client */
	CLI(ci)->mark = 0;

	/* unassign the currently selected client */
	__gwins[curw].sel = 0;
	return;
}

static void
reass_clis(widx_t wi)
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
		reass_cli(__gwins[curw].sel, wi);
	}
	return;
}

/* readline glue */
static void
handle_el(char *line)
{
	widx_t wi;

	/* print newline */
	if (UNLIKELY(line == NULL || line[0] == '\0' || line[0] == ' ')) {
		goto out;
	}

	/* stuff up our history */
	add_history(line);

	/* quick check */
	for (size_t i = 0; i < __ngwins; i++) {
		if (strcmp(line, __gwins[i].name) == 0) {
			wi = i;
			goto reass;
		}
	}
	/* otherwise we need to create a window */
	wi = add_win(line);
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
	win_t w = __gwins + curw;

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
		CLI(w->sel)->mark = !CLI(w->sel)->mark;
		/* pretend we was a key_down */
		k = KEY_DOWN;
		/* fallthrough */
	case KEY_UP:
	case KEY_DOWN:
		if (w->sel) {
			;
		}
		break;
	case KEY_RIGHT:
		break;
	case KEY_LEFT:
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
#include "um-apfmon-clo.h"
#include "um-apfmon-clo.c"
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
	struct umam_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_signal sigwinch_watcher[1];
	ev_timer render[1];

	/* parse the command line */
	if (umam_parser(argc, argv, argi)) {
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
	/* init readline */
	init_rl();

	/* watch the terminal */
	{
		ev_io *keyp = beef + 1 + argi->beef_given;
		ev_io_init(keyp, keypress_cb, STDOUT_FILENO, EV_READ);
		ev_io_start(EV_A_ keyp);
	}

	/* init the portfolio cli list */
	init_cli();

	/* give him a sigwinch, so everything gets rerendered */
	sigwinch_cb(EV_A_ sigwinch_watcher, 0);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* and readline too */
	fini_rl();
	/* reset the screen */
	fini_wins();

	/* finish portfolio clis */
	fini_cli();

	/* detaching beef channels */
	for (unsigned int i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
		ud_mcast_fini(s);
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	umam_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* um-apfmon.c ends here */
