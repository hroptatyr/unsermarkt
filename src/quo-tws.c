/*** quo-tws.c -- quotes and trades from tws
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
/* for mmap */
#include <sys/mman.h>
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
#include "quo-tws-wrapper.h"
#include "quo-tws-private.h"
#include "nifty.h"
#include "strops.h"
#include "gq.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define QUO_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define QUO_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

typedef struct ctx_s *ctx_t;
typedef struct quo_qqq_s *quo_qqq_t;
typedef struct q30_s q30_t;
typedef m30_t q30_pack_t[4];

struct ctx_s {
	/* static context */
	const char *host;
	uint16_t port;
	int client;

	/* dynamic context */
	my_tws_t tws;
	int tws_sock;
};

struct comp_s {
	struct in6_addr addr;
	uint16_t port;
};

struct quo_qq_s {
	struct gq_s q[1];
	struct gq_ll_s sbuf[1];
};

/* indexing into the quo_sub_s->quos */
struct q30_s {
	union {
		struct {
			size_t subtyp:1;
			size_t:1;
		};
		size_t typ:2;
	};
	size_t idx:16;
};

/* the quote-queue quote, i.e. an item of the quote queue */
struct quo_qqq_s {
	struct gq_item_s i;

	/* pointer into our quotes array */
	q30_t q;
};

/* AoV-based subscriptions class */
struct quo_sub_s {
	size_t nsubs;
	tws_instr_t *inss;

	/* actual quotes, this is bid, bsz, ask, asz  */
	q30_pack_t *quos;

	/* array of last dissemination time stamps */
	uint32_t *last_dsm;
};


/* the quotes array */
static inline q30_t
make_q30(uint16_t iidx, quo_typ_t t)
{
#if defined HAVE_ANON_STRUCTS_INIT
	if (LIKELY(t >= QUO_TYP_BID && t <= QUO_TYP_ASZ)) {
		return __extension__(q30_t){.typ = t - 1, .idx = iidx};
	}
	return __extension__(q30_t){0};
#else  /* !HAVE_ANON_STRUCTS_INIT */
	struct q30_s res = {0};

	if (LIKELY(t >= QUO_TYP_BID && t <= QUO_TYP_ASZ)) {
		res.typ = t - 1;
		res.idx = iidx;
	}
	return res;
#endif	/* HAVE_ANON_STRUCTS_INIT */
}

static inline uint16_t
q30_idx(q30_t q)
{
	return (uint16_t)q.idx;
}

static inline quo_typ_t
q30_typ(q30_t q)
{
	return (quo_typ_t)q.typ;
}

static inline unsigned int
q30_sl1t_typ(q30_t q)
{
	return q30_typ(q) / 2 + SL1T_TTF_BID;
}


/* the actual core, ud and fix stuff */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))

/* for them quo oper queues */
#include "gq.c"

/* our beef channels */
static ev_io beef[1];

/* them top-level snapper */
static struct quo_sub_s subs = {0};

/* the sender buffer queue */
static struct quo_qq_s qq = {0};
static utectx_t uu = NULL;

/* ute services come in 2 flavours little endian "ut" and big endian "UT" */
#define UTE_CMD_LE	0x7574
#define UTE_CMD_BE	0x5554
#if defined WORDS_BIGENDIAN
# define UTE_CMD	UTE_CMD_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE_CMD	UTE_CMD_LE
#endif	/* WORDS_BIGENDIAN */

#define BRAG_INTV	(10)
#define UTE_QMETA	0x7572

static bool
udpc_seria_fits_qqq_p(udpc_seria_t ser, quo_qqq_t UNUSED(q))
{
	/* super quick check if we can afford to take it the pkt on
	 * we need roughly 16 bytes */
	if (ser->msgoff + sizeof(struct sl1t_s) > ser->len) {
		return false;
	}
	return true;
}

static bool
udpc_seria_fits_dsm_p(udpc_seria_t ser, const char *UNUSED(sym), size_t len)
{
	return udpc_seria_msglen(ser) + len + 2 + 4 <= UDPC_PLLEN;
}

static inline void
udpc_seria_add_scom(udpc_seria_t ser, scom_t s, size_t len)
{
	memcpy(ser->msg + ser->msgoff, s, len);
	ser->msgoff += len;
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
flush_queue(my_tws_t UNUSED(tws))
{
	static size_t pno = 0;
	char buf[UDPC_PKTLEN];
	char dsm[UDPC_PKTLEN];
	struct udpc_seria_s ser[2];
	struct sl1t_s l1t[1];
	struct timeval now[1];

#define PKT(x)		((ud_packet_t){sizeof(x), x})
#define MAKE_DSM_PKT							\
	udpc_make_pkt(PKT(dsm), 0, pno++, UDPC_PKT_RPL(UTE_QMETA));	\
	udpc_seria_init(ser, UDPC_PAYLOAD(dsm), UDPC_PAYLLEN(sizeof(dsm)))
#define MAKE_PKT							\
	MAKE_DSM_PKT;							\
	udpc_make_pkt(PKT(buf), 0, pno++, UTE_CMD);			\
	udpc_set_data_pkt(PKT(buf));					\
	udpc_seria_init(ser + 1, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)))

	/* time */
	gettimeofday(now, NULL);

	/* populate l1t somewhat */
	sl1t_set_stmp_sec(l1t, now->tv_sec);
	sl1t_set_stmp_msec(l1t, now->tv_usec / 1000);

	/* get the packet ctor'd */
	MAKE_PKT;
	for (gq_item_t ip; (ip = gq_pop_head(qq.sbuf));
	     gq_push_tail(qq.q->free, ip)) {
		quo_qqq_t q = (quo_qqq_t)ip;
		uint16_t tblidx;
		unsigned int ttf;

		if (UNLIKELY(!udpc_seria_fits_qqq_p(ser + 1, q))) {
			ud_chan_send_ser_all(ser);
			ud_chan_send_ser_all(ser + 1);
			MAKE_PKT;
		}

		if ((tblidx = q30_idx(q->q)) == 0 ||
		    (ttf = q30_sl1t_typ(q->q)) == SCOM_TTF_UNK) {
			continue;
		} else if (subs.quos[tblidx - 1][q30_typ(q->q)].u == 0) {
			continue;
		}
		/* the typ was designed to coincide with ute's sl1t types */
		sl1t_set_ttf(l1t, ttf);
		sl1t_set_tblidx(l1t, tblidx);

		l1t->pri = subs.quos[tblidx - 1][q30_typ(q->q)].u;
		l1t->qty = subs.quos[tblidx - 1][q30_typ(q->q) + 1].u;

		udpc_seria_add_scom(ser + 1, AS_SCOM(l1t), sizeof(*l1t));

		/* i think it's worth checking when we last disseminated this */
		if (now->tv_sec - subs.last_dsm[tblidx - 1] >= BRAG_INTV) {
			const char *sym = ute_idx2sym(uu, tblidx);
			size_t len = len = strlen(sym);

			if (UNLIKELY(!udpc_seria_fits_dsm_p(ser, sym, len))) {
				ud_chan_send_ser_all(ser);
				MAKE_DSM_PKT;
			}
			/* add this guy */
			udpc_seria_add_ui16(ser, tblidx);
			udpc_seria_add_str(ser, sym, len);
			subs.last_dsm[tblidx - 1] = now->tv_sec;
		}
	}
	ud_chan_send_ser_all(ser);
	ud_chan_send_ser_all(ser + 1);
	return;
}


/* the queue */
static void
check_qq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = qq.q->free->i1st; ip; ip = ip->next, ni++);
	for (gq_item_t ip = qq.sbuf->i1st; ip; ip = ip->next, ni++);
	assert(ni == qq.q->nitems / sizeof(struct quo_qqq_s));

	ni = 0;
	for (gq_item_t ip = qq.q->free->ilst; ip; ip = ip->prev, ni++);
	for (gq_item_t ip = qq.sbuf->ilst; ip; ip = ip->prev, ni++);
	assert(ni == qq.q->nitems / sizeof(struct quo_qqq_s));
#endif	/* DEBUG_FLAG */
	return;
}

static quo_qqq_t
pop_q(void)
{
	quo_qqq_t res;

	if (qq.q->free->i1st == NULL) {
		size_t nitems = qq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(qq.q->free->ilst == NULL);
		QUO_DEBUG("QQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(qq.q, sizeof(*res), nitems + 64);
		gq_rbld_ll(qq.sbuf, df);
		check_qq();
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(qq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

void
fix_quot(quo_qq_t UNUSED(qq_unused), struct quo_s q)
{
/* shall we rely on c++ code passing us a pointer we handed out earlier? */
	q30_t tgt;

	/* use the dummy ute file to do the sym2idx translation */
	if (q.idx == 0) {
		return;
	} else if (!(tgt = make_q30(q.idx, q.typ)).idx) {
		return;
	} else if (q.idx > subs.nsubs) {
		/* that's actually so fatal I wanna vomit
		 * that means IB sent us ticker ids we've never requested */
		return;
	}

	/* only when the coffee is roasted to perfection:
	 * update the slot TGT ... */
	subs.quos[tgt.idx - 1][tgt.typ] = ffff_m30_get_d(q.val);
	/* ... and push a reminder on the queue */
	{
		quo_qqq_t qi = pop_q();

		qi->q = tgt;
		qi->q.subtyp = 0UL;
		gq_push_tail(qq.sbuf, (gq_item_t)qi);
		QUO_DEBUG("pushed %p\n", qi);
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
	my_tws_t tws = w->data;

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
	QUO_DEBUG("cake stopped\n");
	return;
}

static inline size_t
mmap_size(size_t nelem, size_t elemsz)
{
	static size_t pgsz = 0;

	if (UNLIKELY(!pgsz)) {
		pgsz = sysconf(_SC_PAGESIZE);
	}
	return ((nelem * elemsz) / pgsz + 1) * pgsz;
}

static void
redo_subs(my_tws_t tws)
{
	static const char test[] = "EURSEK";
	uint16_t iidx;

	QUO_DEBUG("req\n");

	if (UNLIKELY(tws == NULL)) {
		/* stop ourselves */
		goto del_req;
	}

	/* get ourselves an index */
	if (UNLIKELY((iidx = ute_sym2idx(uu, test)) == 0)) {
		/* stop outselves */
		goto del_req;
	}

	if (iidx > subs.nsubs) {
		/* singleton/resizer */
		size_t new_sz;
		void *new;

		/* sort the subs array out first */
		new_sz = mmap_size(iidx, sizeof(*subs.inss));
		new = mmap(subs.inss, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.inss, subs.nsubs * sizeof(*subs.inss));
		subs.inss = new;

		/* while we're at it, resize the quos array */
		/* we should at least accomodate 4 * iidx slots innit? */
		new_sz = mmap_size(iidx, sizeof(*subs.quos));
		new = mmap(subs.quos, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.quos, subs.nsubs * sizeof(*subs.quos));
		subs.quos = new;

		new_sz = mmap_size(iidx, sizeof(*subs.last_dsm));
		new = mmap(subs.last_dsm, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.last_dsm, subs.nsubs * sizeof(*subs.last_dsm));
		subs.last_dsm = new;

		/* the largest guy determines the number of subs now */
		subs.nsubs = mmap_size(iidx, sizeof(*subs.quos)) /
			sizeof(*subs.quos) - 1;
	}

	/* construct a contract */
	if (UNLIKELY(
		    subs.inss[iidx - 1] == NULL &&
		    (subs.inss[iidx - 1] = tws_assemble_instr(test)) == NULL)) {
		/* nightmare innit */
		goto del_req;
	}

	/* and finally call the a/c requester */
	if (tws_req_quo(tws, iidx, subs.inss[iidx - 1]) < 0) {
		/* hattrick, how can it go wrong at the last minute, fuck
		 * on the other hand, we just leave the assembled instrument
		 * where it is, maybe the (l)user is gonna retry later */
		goto del_req;
	}
	return;
del_req:
	/* clean up work if something got fucked */
	QUO_DEBUG("req stopped\n");
	return;
}

static void
undo_subs(my_tws_t UNUSED(tws))
{
	for (size_t i = 0; i < subs.nsubs; i++) {
		if (subs.inss[i]) {
			tws_disassemble_instr(subs.inss[i]);
			subs.inss[i] = NULL;
		}
	}
	if (subs.nsubs) {
		size_t alloc_sz;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.inss));
		munmap(subs.inss, alloc_sz);
		subs.inss = NULL;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.quos));
		munmap(subs.quos, alloc_sz);
		subs.quos = NULL;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.last_dsm));
		munmap(subs.last_dsm, alloc_sz);
		subs.last_dsm = NULL;

		subs.nsubs = 0;
	}
	return;
}

static inline bool
got_oid_p(my_tws_t tws)
{
/* inspect TWS and return non-nil if requests to the tws can be made */
	return tws->next_oid;
}

static void
reco_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
/* this is a do fuckall thing */
	ctx_t p = w->data;
	int s;

	if ((s = tws_connect(p->tws, p->host, p->port, p->client)) < 0) {
		/* retry later */
		return;
	}

	/* pass on the socket we've got */
	p->tws_sock = s;
	/* reset tws structure */
	p->tws->next_oid = 0;
	p->tws->time = 0;

	/* stop ourselves */
	ev_timer_stop(EV_A_ w);
	w->data = NULL;
	QUO_DEBUG("reco stopped\n");
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	static ev_io cake[1] = {{0}};
	static ev_timer tm_reco[1] = {{0}};
	static int got_oid = 0;
	ctx_t ctx = w->data;

	/* check if the tws is there */
	if (cake->fd <= 0 && ctx->tws_sock <= 0 && tm_reco->data == NULL) {
		/* uh oh! */
		ev_io_stop(EV_A_ cake);
		cake->data = NULL;

		/* start the reconnection timer */
		tm_reco->data = ctx;
		ev_timer_init(tm_reco, reco_cb, 0.0, 2.0/*option?*/);
		ev_timer_start(EV_A_ tm_reco);
		QUO_DEBUG("reco started\n");

	} else if (cake->fd <= 0 && ctx->tws_sock <= 0) {
		/* great, no connection yet */
		cake->data = NULL;
		QUO_DEBUG("no cake yet\n");

	} else if (cake->fd <= 0) {
		/* ah, connection is back up, init the watcher */
		cake->data = ctx->tws;
		ev_io_init(cake, cake_cb, ctx->tws_sock, EV_READ);
		ev_io_start(EV_A_ cake);
		QUO_DEBUG("cake started\n");

		/* clear tws_sock */
		ctx->tws_sock = -1;
		/* and the oid semaphore */
		got_oid = 0;

	} else if (!got_oid && got_oid_p(ctx->tws)) {
		/* a DREAM i tell ya, let's do our subscriptions */
		redo_subs(ctx->tws);
		got_oid = 1;

	} else {
		/* check the queue integrity */
		check_qq();

		/* maybe we've got something up our sleeve */
		flush_queue(ctx->tws);
	}

	/* and check the queue's integrity again */
	check_qq();

	QUO_DEBUG("queue %zu\n", qq.q->nitems / sizeof(struct quo_qqq_s));
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	QUO_DEBUG("unlooping\n");
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "quo-tws-clo.h"
#include "quo-tws-clo.c"
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
		QUO_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/quo-tws.log", "a");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	struct ctx_s ctx[1];
	/* args */
	struct quo_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_io ctrl[1];
	ev_prepare prp[1];
	/* tws stuff */
	struct my_tws_s tws[1];
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (quo_parser(argc, argv, argi)) {
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
	}

	if (init_tws(tws) < 0) {
		res = 1;
		goto unroll;
	} else if ((uu = ute_mktemp(0)) == NULL) {
		/* shall we warn the user about this */
		res = 1;
		goto unroll;
	}
	/* prepare the context */
	ctx->tws = tws;
	ctx->tws_sock = -1;
	/* pre and post poll hooks */
	prp->data = ctx;
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* cancel them timers and stuff */
	ev_prepare_stop(EV_A_ prp);

	/* kill all tws associated data */
	undo_subs(tws);
	/* secondly, get rid of the tws intrinsics */
	QUO_DEBUG("finalising tws guts\n");
	(void)fini_tws(ctx->tws);

	/* finish the order queue */
	check_qq();
	fini_gq(qq.q);
	ute_free(uu);

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
	quo_parser_free(argi);
	return res;
}

/* quo-tws.c ends here */
