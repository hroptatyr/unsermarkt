/*** ox-tws.c -- order execution through tws
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

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#include <unserding/unserding.h>
#include <unserding/protocore.h>

/* our match messages */
#include "match.h"

/* the tws api */
#include "gen-tws.h"
#include "gen-tws-cont.h"
#include "gen-tws-cont-glu.h"
#include "gen-tws-order.h"
#include "ox-tws-private.h"
#include "gq.h"
#include "nifty.h"

#include "proto-tx-ns.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG
# include <assert.h>
# define OX_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define OX_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

typedef uint32_t ox_idx_t;

typedef struct ctx_s *ctx_t;
typedef struct ox_cl_s *ox_cl_t;

#define CL(x)		(x)

struct ctx_s {
	/* static context */
	const char *host;
	uint16_t port;
	int client;

	/* dynamic context */
	tws_t tws;
	int tws_sock;
};

struct ox_oq_item_s {
	union {
		struct gq_item_s i;
		struct {
			ox_oq_item_t next;
			ox_oq_item_t prev;
		};
	};

	/* the sym, ib instr and agent we're talking */
	ox_cl_t cl;

	/* the order we're talking */
	struct sl1t_s l1t[1];

	/* ib order id */
	tws_oid_t oid;
	/* for partial fills */
	m30_t rem_qty;

};

/* client queue, generic */
struct ox_cq_s {
	struct gq_s q[1];
	struct gq_ll_s used[1];
};

struct ox_cl_s {
	struct gq_item_s i;

	struct umm_agt_s agt;
	/* ib contract */
	tws_cont_t ins;
	/* channel we're talking */
	ud_chan_t ch;
	/* number of relevant orders */
	size_t no;
	/* should be enough */
	char sym[64];
};


static size_t umm_pno = 0;
static struct ox_cq_s cq = {0};
static struct ox_oq_s oq = {0};

#include "gq.c"

/* simplistic dllists */
static void
check_oq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = oq.q->free->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.unpr->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.sent->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.ackd->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.cncd->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.flld->i1st; ip; ip = ip->next, ni++);
	assert(ni == oq.q->nitems / sizeof(struct ox_oq_item_s));

	ni = 0;
	for (gq_item_t ip = oq.q->free->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.unpr->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.sent->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.ackd->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.cncd->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.flld->ilst; ip; ip = ip->prev, ni++);
	assert(ni == oq.q->nitems / sizeof(struct ox_oq_item_s));
#endif	/* DEBUG_FLAG */
	return;
}

static ox_oq_item_t
pop_free(void)
{
	ox_oq_item_t res;

	if (oq.q->free->i1st == NULL) {
		size_t nitems = oq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(oq.q->free->ilst == NULL);
		OX_DEBUG("OQ RESIZE -> %zu\n", nitems + 256);
		df = init_gq(oq.q, sizeof(*res), nitems + 256);
		/* fix up all lists */
		gq_rbld_ll((gq_ll_t)oq.flld, df);
		gq_rbld_ll((gq_ll_t)oq.cncd, df);
		gq_rbld_ll((gq_ll_t)oq.ackd, df);
		gq_rbld_ll((gq_ll_t)oq.sent, df);
		gq_rbld_ll((gq_ll_t)oq.unpr, df);
		check_oq();
	}
	res = (ox_oq_item_t)gq_pop_head(oq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static void
push_free(ox_oq_item_t i)
{
	gq_push_tail(oq.q->free, (gq_item_t)i);
	return;
}

static bool
ox_oq_item_matches_p(ox_oq_item_t i, ox_cl_t cl, const_sl1t_t l1t)
{
	return memcmp(&cl->agt, &i->cl->agt, sizeof(cl->agt)) == 0 &&
		sl1t_tblidx(l1t) == sl1t_tblidx(i->l1t) &&
		l1t->pri == i->l1t->pri;
}

static ox_oq_item_t
pop_match_cl_l1t(ox_oq_dll_t dll, ox_cl_t cl, const_sl1t_t l1t)
{
	for (ox_oq_item_t ip = dll->i1st; ip; ip = ip->next) {
		if (ox_oq_item_matches_p(ip, cl, l1t)) {
			oq_pop_item(dll, ip);
			return ip;
		}
	}
	return NULL;
}

ox_oq_item_t
find_match_oid(ox_oq_dll_t dll, tws_oid_t oid)
{
	for (ox_oq_item_t ip = dll->i1st; ip; ip = ip->next) {
		if (ip->oid == oid) {
			/* found him */
			return ip;
		}
	}
	return NULL;
}

/* client queue implemented through gq */
static ox_cl_t
find_cli(struct umm_agt_s agt)
{
	for (gq_item_t ip = cq.used->i1st; ip; ip = ip->next) {
		ox_cl_t cp = (ox_cl_t)ip;
		if (memcmp(&cp->agt, &agt, sizeof(agt)) == 0) {
			return cp;
		}
	}
	return NULL;
}

static ox_cl_t
add_cli(struct umm_agt_s agt)
{
	ox_cl_t res;

	if (cq.q->free->i1st == NULL) {
		size_t nitems = cq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(cq.q->free->ilst == NULL);
		OX_DEBUG("CQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(cq.q, sizeof(*res), nitems + 64);
		gq_rbld_ll(cq.used, df);
	}
	/* get us a new client and populate the object */
	res = (ox_cl_t)gq_pop_head(cq.q->free);
	memset(res, 0, sizeof(*res));
	res->agt = agt;
	gq_push_tail(cq.used, (gq_item_t)res);
	return res;
}


/* ox item fiddlers */
void
set_prc(ox_oq_item_t ip, double pri)
{
	ip->l1t->pri = ffff_m30_get_d(pri).u;
	return;
}

void
set_qty(ox_oq_item_t ip, double qty)
{
	ip->l1t->qty = ffff_m30_get_d(qty).u;
	return;
}

ox_oq_item_t
clone_item(ox_oq_item_t ip)
{
	ox_oq_item_t res = pop_free();

	memcpy(res, ip, sizeof(*ip));
	res->next = res->prev = NULL;
	return res;
}


/* the actual core */
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
/* unsermarkt match messages */
#define UMM		(0x7576)
#define UMM_RPL		(UDPC_PKT_RPL(UMM))
/* unsermarkt cancel messages */
#define UMU		(0x7578)
#define UMU_RPL		(UDPC_PKT_RPL(UMU))

#if !defined SL2T_TTF_BID
# define SL2T_TTF_BID	(13)
#endif	/* !SL2T_TTF_BID */
#if !defined SL2T_TTF_ASK
# define SL2T_TTF_ASK	(14)
#endif	/* !SL2T_TTF_ASK */

static struct umm_agt_s UNUSED(voidagt) = {0};
static struct umm_agt_s counter = {0};

static inline void
udpc_seria_add_umm(udpc_seria_t sctx, umm_pair_t p)
{
	memcpy(sctx->msg + sctx->msgoff, p, sizeof(*p));
	sctx->msgoff += sizeof(*p);
	return;
}

static inline void
udpc_seria_add_uno(udpc_seria_t sctx, umm_uno_t u)
{
	memcpy(sctx->msg + sctx->msgoff, u, sizeof(*u));
	sctx->msgoff += sizeof(*u);
	return;
}

static inline void
udpc_seria_add_sl1t(udpc_seria_t sctx, const_sl1t_t s)
{
	memcpy(sctx->msg + sctx->msgoff, s, sizeof(*s));
	sctx->msgoff += sizeof(*s);
	return;
}

static void
prep_umm_flld(umm_pair_t mmp, ox_oq_item_t ip)
{
/* take information from IP and populate a match message in MMP
 * the contents of IP will be modified */
	uint16_t ttf = sl1t_ttf(ip->l1t);
	struct timeval now[1];

	/* time, anyone? */
	(void)gettimeofday(now, NULL);

	/* massage the tick so it fits both sides of the pair */
	sl1t_set_ttf(ip->l1t, SL1T_TTF_TRA);
	sl1t_set_stmp_sec(ip->l1t, now->tv_sec);
	sl1t_set_stmp_msec(ip->l1t, now->tv_usec / 1000);
	/* copy the whole tick */
	memcpy(mmp->l1, ip->l1t, sizeof(*ip->l1t));

	if (ttf == SL1T_TTF_BID || ttf == SL2T_TTF_BID) {
		mmp->agt[0] = ip->cl->agt;
		mmp->agt[1] = counter;
	} else if (ttf == SL1T_TTF_ASK || ttf == SL2T_TTF_ASK) {
		mmp->agt[0] = counter;
		mmp->agt[1] = ip->cl->agt;
	} else {
		OX_DEBUG("uh oh, ttf is %hx\n", ttf);
		abort();
	}
	return;
}

static void
prep_umm_cncd(umm_uno_t umu, ox_oq_item_t ip)
{
/* take information from IP and populate a match message in MMP
 * the contents of IP will be modified */
	struct timeval now[1];

	/* time, anyone? */
	(void)gettimeofday(now, NULL);

	/* massage the tick */
	sl1t_set_stmp_sec(ip->l1t, now->tv_sec);
	sl1t_set_stmp_msec(ip->l1t, now->tv_usec / 1000);
	ip->l1t->qty = 0;
	/* copy the whole tick */
	memcpy(umu->l1, ip->l1t, sizeof(*ip->l1t));
	/* and the agent */
	umu->agt[0] = ip->cl->agt;
	/* set the reason */
	umu->reason = UMM_UNO_CANCELLED;
	return;
}

static void
prep_order(ox_cl_t cl, scom_t sc)
{
	/* pop us an item from the queue */
	ox_oq_item_t o = pop_free();
	uint16_t ttf = scom_thdr_ttf(sc);

	/* nifty that umm_pair_s looks like a struct sl1t_s at the beginning */
	memcpy(o->l1t, sc, sizeof(*o->l1t));

	switch (ttf) {
	case SL1T_TTF_BID:
	case SL2T_TTF_BID:
		OX_DEBUG("B %s\n", cl->sym);
		break;
	case SL1T_TTF_ASK:
	case SL2T_TTF_ASK:
		OX_DEBUG("S %s\n", cl->sym);
		break;
	default:
		OX_DEBUG("order type not supported\n");
		return;
	}

	/* copy the agent */
	o->cl = cl;
	/* push on the unprocessed list */
	oq_push_tail(oq.unpr, o);
	return;
}

static void
prep_cancel(ox_cl_t cl, scom_t s)
{
	ox_oq_item_t ip;
	const_sl1t_t l1t = (const void*)s;

	/* traverse the ackd queue first, then the sent queue */
	if ((ip = pop_match_cl_l1t(oq.ackd, cl, l1t)) == NULL &&
	    (ip = pop_match_cl_l1t(oq.sent, cl, l1t)) == NULL) {
		OX_DEBUG("no matches\n");
		return;
	}
	/* else do something */
	OX_DEBUG("ORDER %p matches <-> %u -> CANCEL\n", ip, ip->oid);

	ip->l1t->qty = 0;
	oq_push_tail(oq.unpr, ip);
	return;
}


static void
snarf_data(job_t j, ud_chan_t UNUSED(unused_c))
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct umm_agt_s probe;

	if (UNLIKELY(plen == 0)) {
		return;
	}

	/* the network part of the agent, remains constant throughout */
	probe.addr = j->sa.sa6.sin6_addr;
	probe.port = j->sa.sa6.sin6_port;

	for (scom_thdr_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t ttf = scom_thdr_ttf(sp);

		switch (ttf) {
			ox_cl_t cl;
		case SL1T_TTF_BID:
		case SL1T_TTF_ASK:
		case SL2T_TTF_BID:
		case SL2T_TTF_ASK:
			/* dig deeper */
			probe.uidx = scom_thdr_tblidx(sp);

			if ((cl = find_cli(probe)) == NULL) {
				/* fuck this, we don't even know what
				 * instrument to trade */
				break;
			}

			/* make sure it isn't a cancel */
			if (LIKELY(((sl1t_t)sp)->qty)) {
				/* it's an order, enqueue */
				prep_order(cl, sp);
			} else {
				/* too bad, it's a cancel */
				prep_cancel(cl, sp);
			}
			break;
		default:
			break;
		}
	}
	return;
}

static void
snarf_meta(job_t j, ud_chan_t c)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct udpc_seria_s ser[1];
	struct umm_agt_s probe;
	uint8_t tag;

	/* snarf the network bits and bobs */
	probe.addr = j->sa.sa6.sin6_addr;
	probe.port = j->sa.sa6.sin6_port;

	udpc_seria_init(ser, pbuf, plen);
	while (ser->msgoff < plen && (tag = udpc_seria_tag(ser))) {
		ox_cl_t cl;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		probe.uidx = udpc_seria_des_ui16(ser);
		/* and the cli, if any */
		if ((cl = find_cli(probe)) == NULL) {
			cl = add_cli(probe);
		}
		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (UNLIKELY(tag != UDPC_TYPE_STR || cl == 0)) {
			break;
		}
		/* fuck error checking */
		udpc_seria_des_str_into(CL(cl)->sym, sizeof(CL(cl)->sym), ser);

		/* bother the ib factory to get us a contract */
		if (CL(cl)->ins) {
			tws_free_cont(CL(cl)->ins);
		}
		/* we used to build an instrument here from the name via
		 *   tws_assemble_instr(CL(cl)->sym);
		 * but this can't go on like that */
		{
			tws_cont_t try = tws_make_cont();
			const char *symstr = CL(cl)->sym;

			if (tws_cont_x(try, TX_NS_SYMSTR, 0, symstr) < 0) {
				/* we fucked it */
				tws_free_cont(try);
				try = NULL;
			}
			CL(cl)->ins = try;
		}
		CL(cl)->ch = c;
	}
	return;
}

static void
send_order(tws_t tws, ox_oq_item_t i)
{
	tws_order_t o = NULL;

	OX_DEBUG("ORDER %p %u\n", i, i->oid);
	if (!(i->oid = tws_gen_order(tws, i->cl->ins, o))) {
		OX_DEBUG("unusable: %p %p\n", i->cl->ins, o);
	}
	OX_DEBUG("ORDER %p <-> oid %u\n", i, i->oid);
	return;
}

static void
flush_queue(tws_t tws)
{
	size_t nsnt = 0;

	for (ox_oq_item_t ip; (ip = oq_pop_head(oq.unpr)); nsnt++) {
		send_order(tws, ip);
		oq_push_tail(oq.sent, ip);
	}

	/* assume it's possible to write */
	if (nsnt) {
		tws_send(tws);
	}
	return;
}

static MAYBE_NOINLINE void
flush_cncd(void)
{
/* cancels need no re-confirmation, do they? */
	static char rpl[UDPC_PKTLEN];
	static char sta[UDPC_PKTLEN];
	struct udpc_seria_s ser[1];
	struct udpc_seria_s scs[1];

#define PKT(x)		((ud_packet_t){sizeof(x), x})
#define MAKE_PKT(ser, cmd, x)						\
	udpc_make_pkt(PKT(x), 0, umm_pno++, cmd);			\
	udpc_set_data_pkt(PKT(x));					\
	udpc_seria_init(ser, UDPC_PAYLOAD(x), UDPC_PAYLLEN(sizeof(x)))

	for (ox_oq_item_t ip; (ip = oq.cncd->i1st);) {
		struct umm_uno_s umu[1];
		ud_chan_t ch = ip->cl->ch;

		MAKE_PKT(ser, UMU, rpl);
		MAKE_PKT(scs, UTE_RPL, sta);
		for (; ip; ip = ip->next) {
			/* skip messages not meant for this channel */
			if (ip->cl->ch != ch) {
				continue;
			}

			/* pop the item */
			oq_pop_item(oq.cncd, ip);

			prep_umm_cncd(umu, ip);
			udpc_seria_add_uno(ser, umu);

			/* also prepare a tick reply message */
			udpc_seria_add_sl1t(scs, ip->l1t);

			/* make sure we free this guy */
			OX_DEBUG("freeing %p\n", ip);
			push_free(ip);
		}
		/* and off we go */
		ud_chan_send_ser(ch, ser);
		ud_chan_send_ser(ch, scs);
	}
#undef PKT
#undef MAKE_PKT
	return;
}

static MAYBE_NOINLINE void
flush_flld(void)
{
/* disseminate match messages */
	static char rpl[UDPC_PKTLEN];
	static char sta[UDPC_PKTLEN];
	struct udpc_seria_s ser[1];
	struct udpc_seria_s scs[1];

#define PKT(x)		((ud_packet_t){sizeof(x), x})
#define MAKE_PKT(ser, cmd, x)						\
	udpc_make_pkt(PKT(x), 0, umm_pno++, cmd);			\
	udpc_set_data_pkt(PKT(x));					\
	udpc_seria_init(ser, UDPC_PAYLOAD(x), UDPC_PAYLLEN(sizeof(x)))

	for (ox_oq_item_t ip; (ip = oq.flld->i1st);) {
		struct umm_pair_s mmp[1];
		ud_chan_t ch = ip->cl->ch;

		MAKE_PKT(ser, UMM, rpl);
		MAKE_PKT(scs, UTE_RPL, sta);
		for (; ip; ip = ip->next) {
			/* skip messages not meant for this channel */
			if (ip->cl->ch != ch) {
				continue;
			}

			/* pop the item */
			oq_pop_item(oq.flld, ip);

			prep_umm_flld(mmp, ip);
			udpc_seria_add_umm(ser, mmp);

			/* also prepare a TRA message */
			udpc_seria_add_sl1t(scs, ip->l1t);

			/* make sure we free this guy */
			OX_DEBUG("freeing %p\n", ip);
			push_free(ip);
		}
		/* and off we go */
		ud_chan_send_ser(ch, ser);
		ud_chan_send_ser(ch, scs);
	}
#undef PKT
#undef MAKE_PKT
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
	case QMETA_RPL:
		snarf_meta(j, w->data);
		break;

	case UTE:
		snarf_data(j, w->data);
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
			/* nice one, tws */
			goto del_cake;
		}
	}
	if (revents & EV_WRITE) {
		if (tws_send(tws) < 0) {
			/* fuck this */
			goto del_cake;
		}
	}
	return;
del_cake:
	ev_io_stop(EV_A_ w);
	w->fd = -1;
	w->data = NULL;
	OX_DEBUG("cake stopped\n");
	return;
}

static void
reco_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
	ctx_t p = w->data;
	int s;

	if ((s = tws_connect(p->tws, p->host, p->port, p->client)) < 0) {
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
	OX_DEBUG("reco stopped\n");
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	static ev_io cake[1] = {{0}};
	static ev_timer tm_reco[1] = {{0}};
	ctx_t ctx = w->data;
	tws_t tws = ctx->tws;

	/* check if the tws is there */
	if (cake->fd <= 0 && ctx->tws_sock <= 0 && tm_reco->data == NULL) {
		/* uh oh! */
		ev_io_stop(EV_A_ cake);
		cake->data = NULL;

		/* start the reconnection timer */
		tm_reco->data = ctx;
		ev_timer_init(tm_reco, reco_cb, 0.0, 2.0/*option?*/);
		ev_timer_start(EV_A_ tm_reco);
		OX_DEBUG("reco started\n");

	} else if (cake->fd <= 0 && ctx->tws_sock <= 0) {
		/* great, no connection yet */
		cake->data = NULL;
		OX_DEBUG("no cake yet\n");

	} else if (cake->fd <= 0) {
		/* ah, connection is back up, init the watcher */
		cake->data = ctx->tws;
		ev_io_init(cake, cake_cb, ctx->tws_sock, EV_READ);
		ev_io_start(EV_A_ cake);
		OX_DEBUG("cake started\n");

		/* clear tws_sock */
		ctx->tws_sock = -1;

	} else {
		/* check the queue integrity */
		check_oq();

		/* maybe we've got something up our sleeve */
		flush_queue(tws);
		/* inform everyone about fills */
		flush_flld();
		/* check cancellation list */
		flush_cncd();
	}

	/* and check the queue's integrity again */
	check_oq();

	OX_DEBUG("queue %zu\n", oq.q->nitems / sizeof(struct ox_oq_item_s));
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
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
#include "ox-tws-clo.h"
#include "ox-tws-clo.c"
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
		OX_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/ox-tws.log", "a");
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
	struct ox_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_prepare prp[1];
	/* our beef channels */
	size_t nbeef = 0;
	ev_io *beef = NULL;
	/* tws stuff */
	struct tws_s tws[1] = {{0}};
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (ox_parser(argc, argv, argi)) {
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

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	if ((beef = malloc(nbeef * sizeof(*beef))) == NULL) {
		res = 1;
		goto out;
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

		beef->data = x.p;
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		union __chan_u x = {ud_chan_init(argi->beef_arg[i])};
		int s = ud_chan_init_mcast(x.c);

		beef[i + 1].data = x.p;
		ev_io_init(beef + i + 1, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	if (init_tws(tws, -1, ctx->client) < 0) {
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

	/* first off, get rid of the tws intrinsics */
	OX_DEBUG("finalising tws guts\n");
	(void)fini_tws(ctx->tws);

	/* finish the order queue */
	check_oq();
	fini_gq(oq.q);
	fini_gq(cq.q);

	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_chan_t c = beef[i].data;

		ev_io_stop(EV_A_ beef + i);
		ud_chan_fini(c);
	}
	/* free beef resources */
	free(beef);

unroll:
	/* destroy the default evloop */
	ev_default_destroy();
out:
	ox_parser_free(argi);
	return res;
}

/* ox-tws.c ends here */
