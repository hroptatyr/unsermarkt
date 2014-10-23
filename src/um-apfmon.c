/*** um-apfmon.c -- unsermarkt portfolio monitor
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

#if defined HAVE_LIBREADLINE
# if defined HAVE_READLINE_READLINE_H
#  include <readline/readline.h>
# elif defined HAVE_READLINE_H
#  include <readline.h>
# endif	 /* * */
#else  /* !HAVE_LIBREADLINE */
/* what's our strategy here? */
extern void add_history(const char*);
#endif	/* HAVE_LIBREADLINE */

#if defined HAVE_READLINE_HISTORY
# if defined HAVE_READLINE_HISTORY_H
#  include <readline/history.h>
# elif defined HAVE_HISTORY_H
#  include <history.h>
# endif	 /* * */
#else  /* !HAVE_READLINE_HISTORY */
/* we've got no backup plan */
extern void add_history(const char*);
#endif	/* HAVE_READLINE_HISTORY */

#include <unserding/unserding.h>

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
# define MAX_CLI_AGE	(600U)
# define PRUNE_INTV	(100U)
#else  /* !DEBUG_FLAG */
# define MAX_CLI_AGE	(18000U)
# define PRUNE_INTV	(600U)
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

typedef const struct sockaddr_in6 *my_sockaddr_t;

struct key_s {
	const struct sockaddr *sa;
};

/* one portfolio item */
struct pfi_s {
	struct gq_item_s ALGN(i, 16);

	/* symbol, not as long as usual */
	char sym[32];
	double lqty;
	double sqty;

	int mark;
	unsigned int last_seen;
};

/* the client */
struct cli_s {
	struct sockaddr_storage sa;

	/* helpers for the renderer */
	char ss[INET6_ADDRSTRLEN + 2 + 6];
	size_t sssz;

	/* portfolio name */
	char pf[32];
	size_t pfsz;

	/* all them positions */
	struct gq_s pool[1];
	struct gq_ll_s poss[1];
};

/* portfolio windows */
struct win_s {
	WINDOW *w;

	char name[32];
	size_t namesz;

	pfi_t sel;
};


/* we support a maximum of 64 order books atm */
static struct cli_s *cli = NULL;
static size_t ncli = 0;
static size_t alloc_cli = 0;

/* renderer counter will be inc'd with each render_cb call */
static unsigned int nrend = 0;
#define NOW		(nrend)

#define CLI(x)		(assert(x), assert(x <= ncli), cli + x - 1)

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


static int
sa_eq_p(my_sockaddr_t sa1, my_sockaddr_t sa2)
{
	const size_t s6sz = sizeof(sa1->sin6_addr);
	return sa1->sin6_family == sa2->sin6_family &&
		sa1->sin6_port == sa2->sin6_port &&
		memcmp(&sa1->sin6_addr, &sa2->sin6_addr, s6sz) == 0;
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
	cli_t idx;

	if (UNLIKELY(k.sa->sa_family != AF_INET6)) {
		return 0U;
	}

	idx = ncli++;
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

static pfi_t
pop_pfi(cli_t c)
{
	pfi_t res;
	gq_t q = CLI(c)->pool;

	if (q->free->i1st == NULL) {
		assert(q->free->ilst == NULL);
		UMAM_DEBUG("q resize +%u\n", 64U);
		init_gq(q, 64U, sizeof(*res));
		UMAM_DEBUG("q resize ->%zu\n", q->nitems / sizeof(*res));
	}
	/* get us a new portfolio item */
	res = (void*)gq_pop_head(q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static pfi_t
find_pos(cli_t c, const char *sym, size_t ssz)
{
	gq_ll_t q = CLI(c)->poss;

	for (gq_item_t i = q->i1st; i; i = i->next) {
		pfi_t pos = (void*)i;

		if (memcmp(pos->sym, sym, ssz) == 0) {
			return pos;
		}
	}
	return NULL;
}

static pfi_t
add_pos(cli_t c, const char *sym, size_t ssz)
{
	pfi_t res = pop_pfi(c);

	if (UNLIKELY((res = pop_pfi(c)) == NULL)) {
		return NULL;
	}
	/* all's fine, copy the sym */
	memcpy(res->sym, sym, ssz);
	res->sym[ssz] = '\0';
	/* and shove it onto our poss list */
	gq_push_tail(CLI(c)->poss, (gq_item_t)res);
	return res;
}


/* fix guts */
static size_t
find_fix_fld(const char **p, const char *msg, const char *key)
{
#define SOH	"\001"
	const char *cand = msg - 1;
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

static double
find_fix_dbl(const char *msg, const char *fld, size_t nfld)
{
	size_t tmp;
	double res = 0.0;
	const char *p;

	if ((tmp = find_fix_fld(&p, msg, fld))) {
		const char *cursor = p + nfld;

		res = strtod(cursor, NULL);
	}
	return res;
}

static int
pr_pos_rpt(const struct ud_msg_s *msg, const struct ud_auxmsg_s *aux)
{
/* process them posrpts */
	static const char fix_pos_rpt[] = "35=AP";
	static const char fix_chksum[] = "10=";
	static const char fix_inssym[] = "55=";
	static const char fix_lqty[] = "704=";
	static const char fix_sqty[] = "705=";
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

	for (const char *p = msg->data, *const ep = p + msg->dlen;
	     p && p < ep && (p = find_fix_eofld(p, fix_pos_rpt));
	     p = find_fix_eofld(p, fix_chksum)) {
		struct pfi_s *pos = NULL;
		size_t tmp;
		const char *sym;

		if ((tmp = find_fix_fld(&sym, p, fix_inssym)) == 0) {
			/* great, we NEED that symbol */
			UMAM_DEBUG("no symbol\n");
			continue;
		}
		/* ffw sym */
		sym += sizeof(fix_inssym) - 1;
		tmp -= sizeof(fix_inssym) - 1;
		/* we don't want no steenkin buffer overfloes */
		if (UNLIKELY(tmp >= sizeof(pos->sym))) {
			tmp = sizeof(pos->sym) - 1;
		}
		if ((pos = find_pos(c, sym, tmp))) {
			/* nothing to do */
			;
		} else if ((pos = add_pos(c, sym, tmp))) {
			/* i cant believe how lucky i am */
			;
		} else {
			/* big fuck up */
			continue;
		}

		/* find the long quantity */
		pos->lqty = find_fix_dbl(p, fix_lqty, sizeof(fix_lqty) - 1);
		pos->sqty = find_fix_dbl(p, fix_sqty, sizeof(fix_sqty) - 1);
		pos->last_seen = NOW;
		res++;
	}
	return res;
}


/* the actual worker function */
#define POS_RPT		(0x757aU)
#define POS_RPT_RPL	(0x757bU)
static int changep = 0;

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
			changep = pr_pos_rpt(msg, aux);
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

	__gwins[res].sel = NULL;
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
wadddbl(WINDOW *w, double foo)
{
/* curses helper, prints double values */
	char buf[64];
	int bsz;

	bsz = snprintf(buf, sizeof(buf), "% 16.4f", foo);
	waddnstr(w, buf, bsz);
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
	for (cli_t c = 1; c <= ncli; c++) {
		gq_ll_t poss = CLI(c)->poss;
		size_t j = 2;

		if (w->sel == NULL) {
			w->sel = (void*)poss->i1st;
		}

		for (gq_item_t ip = poss->i1st; ip; ip = ip->next) {
			const struct pfi_s *pos = (void*)ip;

			wmove(w->w, j++, 4);
			if (pos == w->sel) {
				wattron(w->w, A_STANDOUT);
			} else if (pos->mark) {
				wattron(w->w, COLOR_PAIR(CLIMARK));
			} else {
				wattrset(w->w, A_NORMAL);
			}
			waddstr(w->w, pos->sym);

			waddch(w->w, ' ');
			wattron(w->w, COLOR_PAIR(JUST_GREEN));
			wadddbl(w->w, pos->lqty);

			waddch(w->w, ' ');
			wattron(w->w, COLOR_PAIR(JUST_RED));
			wadddbl(w->w, pos->sqty);

			wattrset(w->w, A_NORMAL);
		}
	}

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
reass_pos(pfi_t pos, widx_t UNUSED(wi))
{
	/* unmark client */
	pos->mark = 0;

	/* unassign the currently selected client */
	__gwins[curw].sel = NULL;
	return;
}

static void
reass_poss(widx_t wi)
{
	int markp = 0;

	/* if there's no marks, just use the currently selected cli */
	for (size_t c = 1; c <= ncli; c++) {
		gq_ll_t poss = CLI(c)->poss;
		for (gq_item_t i = poss->i1st; i; i = i->next) {
			pfi_t pos = (void*)i;

			if (pos->mark) {
				reass_pos(pos, wi);
				markp = 1;
			}
		}
	}
	if (!markp && __gwins[curw].sel) {
		/* no marks found, reass the current selection */
		reass_pos(__gwins[curw].sel, wi);
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
	reass_poss(wi);

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

	switch (getch()) {
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
		if (w->sel == NULL) {
			break;
		}
		/* toggle mark */
		w->sel->mark = !w->sel->mark;
		/* pretend we was a key_down and ... */
		/* fallthrough */
	case KEY_DOWN:
		if (w->sel == NULL || w->sel->i.next == NULL) {
			break;
		}
		w->sel = (void*)w->sel->i.next;
		goto redraw;
	case KEY_UP:
		if (w->sel == NULL || w->sel->i.prev == NULL) {
			break;
		}
		w->sel = (void*)w->sel->i.prev;
		goto redraw;
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


#include "um-apfmon.yucc"

int
main(int argc, char *argv[])
{
	/* args */
	yuck_t argi[1U];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_io *beef = NULL;
	size_t nbeef = 0;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_signal sigwinch_watcher[1];
	ev_timer render[1];
	int rc = 0;

	/* big assignment for logging purposes */
	logerr = fopen("/tmp/um-apfmon.log", "a");

	/* parse the command line */
	if (yuck_parse(argi, argc, argv)) {
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
		double fps = argi->fps_arg
			? strtod(argi->fps_arg, NULL) ?: 10.
			: 10.;
		double slp = 1.0 / fps;
		ev_timer_init(render, render_cb, slp, slp);
		ev_timer_start(EV_A_ render);
	}

	/* make some room for the control channel and the beef chans */
	nbeef = argi->nargs + 1U + 1U;
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
	for (size_t i = 0U; i < argi->nargs; i++) {
		char *p;
		long unsigned int port = strtoul(argi->args[i], &p, 0);

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

	/* start the screen */
	init_wins();
	/* init readline */
	init_rl();

	/* watch the terminal */
	{
		ev_io *keyp = beef + 1 + argi->nargs;
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
		ud_sock_t s;

		if ((s = beef[i].data) != NULL) {
			ev_io_stop(EV_A_ beef + i);
			ud_close(s);
		}
	}
	/* free beef resources */
	free(beef);

	/* destroy the default evloop */
	ev_default_destroy();

	/* close log file */
	fclose(logerr);

out:
	/* kick the config context */
	yuck_free(argi);

	/* unloop was called, so exit */
	return rc;
}

/* um-apfmon.c ends here */
