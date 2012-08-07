/*** pf-tws.c -- portfolio management through tws
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
#include <stdio.h>
/* for gmtime_r */
#include <time.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <fcntl.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <netinet/in.h>
#include <netdb.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>
#define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

/* the tws api */
#include "gen-tws.h"
#include "pf-tws-private.h"
#include "nifty.h"
#include "strops.h"
#include "gq.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define PF_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define PF_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

typedef struct ctx_s *ctx_t;
typedef struct pf_pqpr_s *pf_pqpr_t;

struct ctx_s {
	struct tws_s tws[1];

	/* static context */
	const char *host;
	uint16_t port;
	int client;

	/* dynamic context */
	int tws_sock;
};

struct comp_s {
	struct in6_addr addr;
	uint16_t port;
};

struct pf_pq_s {
	struct gq_s q[1];
	struct gq_ll_s sbuf[1];
};

struct pf_pqpr_s {
	struct gq_item_s i;

	/* a/c name */
	char ac[16];
	/* symbol not as big, but fits in neatly with the 16b a/c identifier */
	char sym[48];
	double lqty;
	double sqty;
};


/* the actual core, ud and fix stuff */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))

/* for them pf oper queues */
#include "gq.c"

/* our tws conn(s?) */
static struct comp_s counter = {0};
/* our beef channels */
static size_t nbeef = 0;
static ev_io beef[1];

/* the sender buffer queue */
static struct pf_pq_s pq = {0};

#define SOH		"\001"
static const char fix_stdhdr[] = "8=FIXT.1.1" SOH "9=0000" SOH;
static const char fix_stdftr[] = "10=xyz" SOH;

static uint8_t
fix_chksum(const char *str, size_t len)
{
	uint8_t res = 0;
	for (const char *p = str, *ep = str + len; p < ep; res += *p++);
	return res;
}

static bool
udpc_seria_pr_feasible_p(udpc_seria_t ser, pf_pqpr_t UNUSED(pr))
{
	/* super quick check if we can afford to take it the pkt on
	 * we need roughly 256 bytes */
	if (ser->msgoff + 256 > ser->len) {
		return false;
	}
	return true;
}

static void
udpc_seria_add_pr(udpc_seria_t ser, pf_pqpr_t pr)
{
	static size_t sno = 0;
	char ntop_src[INET6_ADDRSTRLEN];
	uint16_t port_src;
	char dt[24];
	struct timeval now[1];
	char *sp, *p, *ep;
	size_t plen;

	/* some work beforehand */
	inet_ntop(AF_INET6, (void*)&counter.addr, ntop_src, sizeof(ntop_src));
	port_src = ntohs(counter.port);

	/* what's the time */
	{
		struct tm *tm;

		gettimeofday(now, NULL);
		tm = gmtime(&now->tv_sec);
		strftime(dt, sizeof(dt), "%Y%m%d-%H:%M:%S", tm);
	}

	if (LIKELY(nbeef)) {
		ud_chan_t ch = beef->data;
		const struct in6_addr *addr = &ch->sa.sa6.sin6_addr;
		char ntop_tgt[INET6_ADDRSTRLEN];
		uint16_t port_tgt;
		uint8_t chksum;

		inet_ntop(AF_INET6, addr, ntop_tgt, sizeof(ntop_tgt));
		port_tgt = ntohs(ch->sa.sa6.sin6_port);

		sp = p = ser->msg + ser->msgoff;
		plen = ser->len - ser->msgoff;
		ep = p + plen;

/* unsafe BANG */
#define __BANG(tgt, eot, src, ssz)				\
		{						\
			size_t __ssz = ssz;			\
			memcpy(tgt, src, __ssz);		\
			tgt += __ssz;				\
		}
#define BANGP(tgt, eot, p)			\
		__BANG(tgt, eot, p, strlen(p))
#define BANGL(tgt, eot, literal)				\
		__BANG(tgt, eot, literal, sizeof(literal) - 1)

		BANGL(p, ep, fix_stdhdr);
		/* message type */
		BANGL(p, ep, "35=AP" SOH);
		/* sender */
		BANGL(p, ep, "49=[");
		BANGP(p, ep, ntop_src);
		BANGL(p, ep, "]:");
		p += ui16tostr(p, ep - p, port_src);
		*p++ = *SOH;
		/* recipient */
		BANGL(p, ep, "56=[");
		BANGP(p, ep, ntop_tgt);
		BANGL(p, ep, "]:");
		p += ui16tostr(p, ep - p, port_tgt);
		*p++ = *SOH;
		/* seqno */
		BANGL(p, ep, "34=");
		p += ui32tostr(p, ep - p, sno);
		*p++ = *SOH;

		/* sending time */
		BANGL(p, ep, "52=");
		BANGP(p, ep, dt);
		*p++ = '.';
		p += ui16tostr_pad(p, ep - p, now->tv_usec / 1000, 3);
		*p++ = *SOH;

		/* report id */
		BANGL(p, ep, "721=r");
		p += ui32tostr(p, ep - p, sno);
		*p++ = *SOH;

		/* clearing bizdate */
		BANGL(p, ep, "715=2012-06-28" SOH);
		/* #ptys */
		BANGL(p, ep, "453=0" SOH);
		/* ac name */
		BANGL(p, ep, "1=");
		BANGP(p, ep, pr->ac);
		*p++ = *SOH;

		/* instr name */
		BANGL(p, ep, "55=");
		BANGP(p, ep, pr->sym);
		*p++ = *SOH;

		/* #positions */
		BANGL(p, ep, "702=1" SOH);
		/* position type */
		BANGL(p, ep, "703=TOT" SOH);
		/* qty long */
		BANGL(p, ep, "704=");
		{
			m30_t qty = ffff_m30_get_d(pr->lqty);
			p += ffff_m30_s(p, qty);
		}
		*p++ = *SOH;
		/* qty long */
		BANGL(p, ep, "705=");
		{
			m30_t qty = ffff_m30_get_d(pr->sqty);
			p += ffff_m30_s(p, qty);
		}
		*p++ = *SOH;

		/* qty status */
		BANGL(p, ep, "706=0" SOH);
		/* qty date */
		BANGL(p, ep, "976=");
		__BANG(p, ep, dt, 8);
		*p++ = *SOH;

		/* get the length so far and print it */
		plen = p - sp - (sizeof(fix_stdhdr) - 1);
		ui16tostr_pad(sp + 13, 4, plen, 4);

		/* compute the real plen now */
		plen = p - sp;
		chksum = fix_chksum(sp, plen);
		BANGL(p, ep, fix_stdftr);
		ui8tostr_pad(p - 4, ep - p, chksum, 3);
		*p = '\0';

		/* and now plen again, this time with the footer */
		plen = p - sp;
		ep = sp + plen;

#if !defined BENCHMARK && defined DEBUG_FLAG && 0
		/* quickly massage the string suitable for printing */
		for (p = sp; p < ep; p++) {
			if (*p == *SOH) {
				*p = '|';
			}
		}
		fwrite(sp, sizeof(char), plen, logerr);
		fputc('\n', logerr);
#endif	/* !BENCHMARK && DEBUG_FLAG */

		/* up the seria msgoff*/
		ser->msgoff += plen;
		sno++;
	}
	return;
}

static void
ud_chan_send_ser_all(udpc_seria_t ser)
{
	ud_chan_t ch = beef->data;

	/* assume it's possible to write */
	ud_chan_send_ser(ch, ser);
	return;
}

static void
flush_queue(tws_t UNUSED(tws))
{
	static size_t pno = 0;
	char buf[UDPC_PKTLEN];
	struct udpc_seria_s ser[1];
	size_t nsnt = 0;

#define PKT(x)		((ud_packet_t){sizeof(x), x})
#define MAKE_PKT							\
	udpc_make_pkt(PKT(buf), 0, pno++, POS_RPT_RPL);			\
	udpc_set_data_pkt(PKT(buf));					\
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)))

	/* get the packet ctor'd */
	MAKE_PKT;
	for (gq_item_t ip; (ip = gq_pop_head(pq.sbuf)); nsnt++) {
		pf_pqpr_t pr = (pf_pqpr_t)ip;

		if (!udpc_seria_pr_feasible_p(ser, pr)) {
			ud_chan_send_ser_all(ser);
			MAKE_PKT;
		}
		udpc_seria_add_pr(ser, pr);
		gq_push_tail(pq.q->free, ip);
	}
	ud_chan_send_ser_all(ser);
	return;
}


/* the queue */
static void
check_pq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = pq.q->free->i1st; ip; ip = ip->next, ni++);
	for (gq_item_t ip = pq.sbuf->i1st; ip; ip = ip->next, ni++);
	assert(ni == pq.q->nitems / sizeof(struct pf_pqpr_s));

	ni = 0;
	for (gq_item_t ip = pq.q->free->ilst; ip; ip = ip->prev, ni++);
	for (gq_item_t ip = pq.sbuf->ilst; ip; ip = ip->prev, ni++);
	assert(ni == pq.q->nitems / sizeof(struct pf_pqpr_s));
#endif	/* DEBUG_FLAG */
	return;
}

static pf_pqpr_t
pop_pr(void)
{
	pf_pqpr_t res;

	if (pq.q->free->i1st == NULL) {
		size_t nitems = pq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(pq.q->free->ilst == NULL);
		PF_DEBUG("PQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(pq.q, sizeof(*res), nitems + 64);
		gq_rbld_ll(pq.sbuf, df);
		check_pq();
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(pq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

void
fix_pos_rpt(pf_pq_t UNUSED(pf), const char *ac, struct pf_pos_s pos)
{
/* shall we rely on c++ code passing us a pointer we handed out earlier? */
	pf_pqpr_t pr = pop_pr();

	/* keep a rough note of the account name
	 * not that it does anything at the mo */
	strncpy(pr->ac, ac, sizeof(pr->ac));
	pr->ac[sizeof(pr->ac) - 1] = '\0';
	/* keep a rough note of the account name
	 * not that it does anything at the mo */
	strncpy(pr->sym, pos.sym, sizeof(pr->sym));
	pr->sym[sizeof(pr->sym) - 1] = '\0';
	/* and of course our quantities */
	pr->lqty = pos.lqty;
	pr->sqty = pos.sqty;

	gq_push_tail(pq.sbuf, (gq_item_t)pr);
	PF_DEBUG("pushed %p\n", pr);
	return;
}


/* callbacks coming from the tws */
static void
infra_cb(tws_t tws, tws_cb_t what, struct tws_infra_clo_s clo)
{
	switch (what) {
	case TWS_CB_INFRA_ERROR:
		PF_DEBUG("tws %p: oid %u  code %u: %s\n",
			tws, clo.oid, clo.code, (const char*)clo.data);
		break;
	case TWS_CB_INFRA_CONN_CLOSED:
		PF_DEBUG("tws %p: connection closed\n", tws);
		break;
	default:
		PF_DEBUG("%p infra called: what %u  oid %u  code %u  data %p\n",
			tws, what, clo.oid, clo.code, clo.data);
		break;
	}
	return;
}


static void
beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nrd;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

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
	case POS_RPT:
		break;
	case POS_RPT_RPL:
		break;
	default:
		break;
	}

out_revok:
	return;
}

static void
cake_cb(EV_P_ ev_io *w, int revents)
{
	tws_t tws = w->data;

	if (revents & EV_READ) {
		if (tws_recv(tws) < 0) {
			/* grrrr */
			goto del_cake;
		}
	}
	if (revents & EV_WRITE) {
		if (tws_send(tws) < 0) {
			/* brilliant */
			goto del_cake;
		}
	}
	return;
del_cake:
	ev_io_stop(EV_A_ w);
	w->fd = -1;
	w->data = NULL;
	PF_DEBUG("cake stopped\n");
	return;
}

static void
req_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
	tws_t tws = w->data;

	PF_DEBUG("req\n");
	if (UNLIKELY(tws == NULL)) {
		/* stop ourselves */
		goto del_req;
	}

#define TWS_ALL_ACCOUNTS	(NULL)
	/* call the a/c requester */
	if (tws_req_ac(tws, TWS_ALL_ACCOUNTS) < 0) {
		goto del_req;
	}
#undef TWS_ALL_ACCOUNTS
	return;
del_req:
	ev_timer_stop(EV_A_ w);
	w->data = NULL;
	PF_DEBUG("req stopped\n");
	return;
}

static void
reco_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
/* this is a do fuckall thing */
	ctx_t p = w->data;
	tws_t tws = w->data;
	int s;

	if ((s = tws_connect(tws, p->host, p->port, p->client)) < 0) {
		/* retry later */
		return;
	}

	/* also hand out details to the COUNTER struct */
	{
		union ud_sockaddr_u sa;
		socklen_t ss = sizeof(sa);

		getsockname(s, &sa.sa, &ss);
		counter.addr = sa.sa6.sin6_addr;
		counter.port = htons(p->port);
	}

	/* pass on the socket we've got */
	p->tws_sock = s;

	/* stop ourselves */
	ev_timer_stop(EV_A_ w);
	w->data = NULL;
	PF_DEBUG("reco stopped\n");
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	static ev_io cake[1] = {{0}};
	static ev_timer tm_req[1] = {{0}};
	static ev_timer tm_reco[1] = {{0}};
	ctx_t ctx = w->data;
	tws_t tws = w->data;

	/* check if the tws is there */
	if (cake->fd <= 0 && ctx->tws_sock <= 0 && tm_reco->data == NULL) {
		/* uh oh! */
		ev_timer_stop(EV_A_ tm_req);
		ev_io_stop(EV_A_ cake);
		cake->data = NULL;
		tm_req->data = NULL;

		/* start the reconnection timer */
		tm_reco->data = ctx;
		ev_timer_init(tm_reco, reco_cb, 0.0, 2.0/*option?*/);
		ev_timer_start(EV_A_ tm_reco);
		PF_DEBUG("reco started\n");

	} else if (cake->fd <= 0 && ctx->tws_sock <= 0) {
		/* great, no connection yet */
		cake->data = NULL;
		tm_req->data = NULL;
		PF_DEBUG("no cake yet\n");

	} else if (cake->fd <= 0) {
		/* ah, connection is back up, init the watcher */
		cake->data = tws;
		ev_io_init(cake, cake_cb, ctx->tws_sock, EV_READ);
		ev_io_start(EV_A_ cake);
		PF_DEBUG("cake started\n");

		/* also set up the timer to emit the portfolio regularly */
		ev_timer_init(tm_req, req_cb, 0.0, 60.0/*option?*/);
		tm_req->data = tws;
		ev_timer_start(EV_A_ tm_req);
		PF_DEBUG("req started\n");

		/* clear tws_sock */
		ctx->tws_sock = -1;

	} else {
		/* check the queue integrity */
		check_pq();

		/* maybe we've got something up our sleeve */
		flush_queue(tws);
	}

	/* and check the queue's integrity again */
	check_pq();

	PF_DEBUG("queue %zu\n", pq.q->nitems / sizeof(struct pf_pqpr_s));
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	PF_DEBUG("unlooping\n");
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "pf-tws-clo.h"
#include "pf-tws-clo.c"
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
		PF_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/pf-tws.log", "a");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	struct ctx_s ctx[1] = {{0}};
	/* args */
	struct pf_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_io ctrl[1];
	ev_prepare prp[1];
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (pf_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* snarf host name and port */
	if (argi->tws_host_given) {
		ctx->host = argi->tws_host_arg;
	} else {
		ctx->host = "localhost";
	}
	if (argi->tws_port_given) {
		ctx->port = (uint16_t)argi->tws_port_arg;
	} else {
		ctx->port = (uint16_t)7474;
	}
	if (argi->tws_client_id_given) {
		ctx->client = argi->tws_client_id_arg;
	} else {
		struct timeval now[1];

		(void)gettimeofday(now, NULL);
		ctx->client = now->tv_sec;
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	ev_signal_init(sighup_watcher, sigall_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	union __chan_u {
		ud_chan_t c;
		void *p;
	};
	{
		union __chan_u x = {ud_chan_init(UD_NETWORK_SERVICE)};
		int s = ud_chan_init_mcast(x.c);

		ctrl->data = x.p;
		ev_io_init(ctrl, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ ctrl);
	}

	/* go through all beef channels */
	if (argi->beef_given) {
		union __chan_u x = {ud_chan_init(argi->beef_arg)};
		int s = ud_chan_init_mcast(x.c);

		beef->data = x.p;
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
		nbeef = 1;
	}

	if (init_tws(ctx->tws, -1, ctx->client) < 0) {
		res = 1;
		goto unroll;
	}
	/* prepare the tws and the context */
	ctx->tws->infra_cb = infra_cb;
	ctx->tws_sock = -1;
	/* pre and post poll hooks */
	prp->data = ctx;
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* cancel them timers and stuff */
	ev_prepare_stop(EV_A_ prp);

	/* first off, get rid of the tws intrinsics */
	PF_DEBUG("finalising tws guts\n");
	(void)fini_tws(ctx->tws);

	/* finish the order queue */
	check_pq();
	fini_gq(pq.q);

	/* detaching beef and ctrl channels */
	if (argi->beef_given) {
		ud_chan_t c = beef->data;

		ev_io_stop(EV_A_ beef);
		ud_chan_fini(c);
	}
	{
		ud_chan_t c = ctrl->data;

		ev_io_stop(EV_A_ ctrl);
		ud_chan_fini(c);
	}

unroll:
	/* destroy the default evloop */
	ev_default_destroy();
out:
	pf_parser_free(argi);
	return res;
}

/* pf-tws.c ends here */
