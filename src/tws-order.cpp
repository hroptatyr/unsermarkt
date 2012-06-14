/*** tws-order.cpp -- listen to beef channels and submit orders
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
#endif	// HAVE_CONFIG_H
#include <stdio.h>
// for gettimeofday()
#include <sys/time.h>
// for epoll
#include <sys/epoll.h>

// for twsdl class
#include "twstools/twsdo.h"
#include "twstools/tws_meta.h"
#include "twstools/tws_query.h"

#include <unserding/unserding.h>
#include <unserding/protocore.h>
#include <uterus.h>
#include <m30.h>

#include "iso4217.h"

extern "C" {
/* libtool needs C symbols */
extern void init(void*);
extern void fini(void*);
extern void work(void*);
}

#if !defined UNLIKELY
# define UNLIKELY(x)	__builtin_expect((x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(x)	__attribute__((unused)) x
#endif	/* UNUSED */


// unserding guts
static int mcfd = -1;
static int epfd = -1;

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

// glue
typedef struct level_s *level_t;
typedef long int oid_t;

struct level_s {
	double p;
	double q;
};

typedef long unsigned int *bitset_t;

static level_t mkt_bid = NULL;
static level_t mkt_ask = NULL;
static char *syms = NULL;
static size_t *offs = NULL;
static bitset_t change = NULL;
static size_t npos = 0U;
// ib contracts
static IB::Contract **ibcntr = NULL;
static oid_t *oid_b = NULL;
static oid_t *oid_a = NULL;

static inline void
bitset_set(bitset_t bs, unsigned int bit)
{
	unsigned int div = bit / (sizeof(*bs) * CHAR_BIT);
	unsigned int rem = bit % (sizeof(*bs) * CHAR_BIT);
	bs[div] |= (1UL << rem);
	return;
}

static inline void
bitset_unset(bitset_t bs, unsigned int bit)
{
	unsigned int div = bit / (sizeof(*bs) * CHAR_BIT);
	unsigned int rem = bit % (sizeof(*bs) * CHAR_BIT);
	bs[div] &= ~(1UL << rem);
	return;
}

static inline int
bitset_get(bitset_t bs, unsigned int bit)
{
	unsigned int div = bit / (sizeof(*bs) * CHAR_BIT);
	unsigned int rem = bit % (sizeof(*bs) * CHAR_BIT);
	return (bs[div] >> rem) & 1;
}

static void
bitset_clear(bitset_t bs, size_t nbits)
{
	if (nbits == 0) {
		return;
	}
	memset(bs, 0, ((nbits / (sizeof(*bs) * CHAR_BIT)) ?: 1) * sizeof(*bs));
	return;
}

static inline __attribute__((const)) long unsigned int
__roundup_2pow(long unsigned int n)
{
#define ROUNDUP_LEAST	(64UL)
	n /= ROUNDUP_LEAST;
	return ROUNDUP_LEAST << ffsl((n - 1) & -(n - 1));
}

static void
check_resz(uint16_t idx)
{
	// check for resizes
	if (idx >= npos) {
		size_t nu = __roundup_2pow(idx + 1);

#define DIFF		(nu - npos)
#define REALL(x, y)	x = (typeof(x))realloc(x, (nu / (y)) * sizeof(*x))
#define RINSE(x, y)	memset(x + npos / (y), 0, (DIFF / (y)) * sizeof(*x))

		// make the stuff bigger
		REALL(mkt_bid, 1);
		REALL(mkt_ask, 1);
		REALL(offs, 1);
		REALL(change, sizeof(*change) * CHAR_BIT);
		REALL(ibcntr, 1);
		REALL(oid_b, 1);
		REALL(oid_a, 1);

		// rinse
		RINSE(mkt_bid, 1);
		RINSE(mkt_ask, 1);
		RINSE(offs, 1);
		RINSE(change, sizeof(*change) * CHAR_BIT);
		RINSE(ibcntr, 1);
		RINSE(oid_b, 1);
		RINSE(oid_a, 1);

		// reassign npos
		npos = nu;
	}
	return;
}

static void
party(const char *buf, size_t bsz)
{
	// start with a clean sheet
	bitset_clear(change, npos);

	for (scom_t sp = (scom_t)buf, ep = sp + bsz / sizeof(*ep);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t idx = scom_thdr_tblidx(sp);
		uint16_t ttf = scom_thdr_ttf(sp);
		m30_t p = {((const_sl1t_t)sp)->v[0]};
		m30_t q = {((const_sl1t_t)sp)->v[1]};

		// check for resizes
		check_resz(idx);

		switch (ttf) {
		case SL1T_TTF_BID:
			mkt_bid[idx].p = ffff_m30_d(p);
			mkt_bid[idx].q = ffff_m30_d(q);
			break;
		case SL1T_TTF_ASK:
			mkt_ask[idx].p = ffff_m30_d(p);
			mkt_ask[idx].q = ffff_m30_d(q);
			break;
		case SBAP_FLAVOUR:
			mkt_bid[idx].p = ffff_m30_d(p);
			mkt_ask[idx].p = ffff_m30_d(q);
			break;
		default:
			continue;
		}

		// mark fuckerage
		bitset_set(change, idx);
	}
	return;
}

static void
asm_ibcntr(IB::Contract **con, const char *sym, size_t UNUSED(ssz))
{
	// assume BASTRM
	const_iso_4217_t bas =
		find_iso_4217_by_name(sym);
	const_iso_4217_t trm =
		find_iso_4217_by_name(sym + 3);
	IB::Contract *tmp = new IB::Contract();

	tmp->symbol = bas ? std::string(bas->sym) : NULL;
	tmp->currency = trm ? std::string(trm->sym) : NULL;
	tmp->secType = std::string("CASH");
	tmp->exchange = std::string("IDEALPRO");
	*con = tmp;
	return;
}

static void
disasm_ibcntr(IB::Contract *con)
{
	delete con;
	return;
}

static void
pmeta(char *buf, size_t bsz)
{
	struct udpc_seria_s ser[1];
	uint8_t tag;

	udpc_seria_init(ser, buf, bsz);
	while (ser->msgoff < bsz && (tag = udpc_seria_tag(ser))) {
		const char *p;
		uint16_t idx;
		size_t sz;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		idx = udpc_seria_des_ui16(ser);

		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (UNLIKELY(tag != UDPC_TYPE_STR)) {
			break;
		}
		// deser the string
		sz = udpc_seria_des_str(ser, &p);

		if (idx) {
			static size_t syms_alloc_sz = 0;
			size_t rdsz;

			// check for resizes
			check_resz(idx);
			// this assumes that QMETA stuff will be sent in order!
			// i.e. we break if we get index 5 before index 2
			offs[idx] = sz + 1 + offs[idx - 1];

			// bang the info into syms array
			rdsz = __roundup_2pow(offs[idx] + 64);
			if (rdsz > syms_alloc_sz) {
				syms = (char*)realloc(syms, rdsz);
				syms_alloc_sz = rdsz;
			}
			strncpy(syms + offs[idx - 1], p, sz);

			// assemble a contract
			if (memmem(p, sz, "|rexfo_rt", 9) ||
			    memmem(p, sz, "|netdania", 9)) {
				asm_ibcntr(ibcntr + idx, p, sz);
			}
		}
	}
	return;
}

static oid_t
adapt_b(TwsDL *tws, const IB::Contract &cntr, oid_t oid, struct level_s b)
{
	PlaceOrder o;
	const double qdist = 0.0001;

	o.contract = cntr;
	o.order.orderType = "LMT";
	o.order.totalQuantity = 100000;

	// new bid that we're ready to risk
	b.p = round(b.p * 2.0 * 10000.0) / 2.0 / 10000.0 - qdist;

	if (tws->p_orders.find(oid) == tws->p_orders.end()) {
		/* new buy order */
		oid = tws->fetch_inc_order_id();
	} else {
		/* modify buy order */
		PacketPlaceOrder *ppo = tws->p_orders[oid];
		const PlaceOrder &po = ppo->getRequest();

		if (po.order.lmtPrice == b.p) {
			return oid;
		}
	}
	// otherwise, business as usual
	o.orderId = oid;
	o.order.action = "BUY";
	o.order.lmtPrice = b.p;
	tws->workTodo->placeOrderTodo()->add(o);
	return oid;
}

static oid_t
adapt_a(TwsDL *tws, const IB::Contract &cntr, oid_t oid, struct level_s a)
{
	PlaceOrder o;
	const double qdist = 0.0001;

	o.contract = cntr;
	o.order.orderType = "LMT";
	o.order.totalQuantity = 100000;

	// new bid that we're ready to risk
	a.p = round(a.p * 2.0 * 10000.0) / 2.0 / 10000.0 + qdist;

	if (tws->p_orders.find(oid) == tws->p_orders.end()) {
		/* new buy order */
		oid = tws->fetch_inc_order_id();
	} else {
		/* modify buy order */
		PacketPlaceOrder *ppo = tws->p_orders[oid];
		const PlaceOrder &po = ppo->getRequest();

		if (po.order.lmtPrice == a.p) {
			return oid;
		}
	}
	// business as usual
	o.orderId = oid;
	o.order.action = "SELL";
	o.order.lmtPrice = a.p;
	tws->workTodo->placeOrderTodo()->add(o);
	return oid;
}

static void
cancel_o(TwsDL *tws, oid_t oid)
{
	PlaceOrder o;

	o.orderId = oid;
	o.order.orderType = "MKT";
	o.order.action = "CANCEL";
	o.order.totalQuantity = 0;
	tws->workTodo->placeOrderTodo()->add(o);
	return;
}

static void
adapt(TwsDL *tws, size_t idx)
{
	if (ibcntr[idx] == NULL) {
		return;
	}

	// adapt the order
	oid_b[idx] = adapt_b(tws, *ibcntr[idx], oid_b[idx], mkt_bid[idx]);
	oid_a[idx] = adapt_a(tws, *ibcntr[idx], oid_a[idx], mkt_bid[idx]);
	return;
}

static void
cancel(TwsDL *tws, size_t idx)
{
	if (oid_b[idx]) {
		cancel_o(tws, oid_b[idx]);
	}
	if (oid_a[idx]) {
		cancel_o(tws, oid_a[idx]);
	}
	return;
}


/* public exposure for the DSO, C linkage */
void init(void *UNUSED(clo))
{
	/* yep, one handle please */
	if ((mcfd = ud_mcast_init(7868/*ND*/)) >= 0) {
		struct epoll_event ev[1];

		ev->events = EPOLLIN;
		ev->data.fd = mcfd;
		if ((epfd = epoll_create(1)) >= 0) {
			epoll_ctl(epfd, EPOLL_CTL_ADD, mcfd, ev);
		}
	}
	return;
}

void fini(void *clo)
{
	if (epfd >= 0) {
		epoll_ctl(epfd, EPOLL_CTL_DEL, mcfd, NULL);
		close(epfd);
	}
	if (mcfd >= 0) {
		ud_mcast_fini(mcfd);
	}
	if (npos > 0) {
		// clean up
		for (size_t i = 0; i < npos; i++) {
			if (ibcntr[i]) {
				// see if there's orders and whatnot
				cancel((TwsDL*)clo, i);
				disasm_ibcntr(ibcntr[i]);
			}
		}

		free(mkt_bid);
		free(mkt_ask);
		free(offs);
		free(change);
		free(oid_a);
		free(oid_b);
		free(ibcntr);

		mkt_bid = NULL;
		mkt_ask = NULL;
		offs = NULL;
		change = NULL;
		ibcntr = NULL;
		oid_a = NULL;
		oid_b = NULL;
		npos = 0U;
	}
	return;
}

void work(void *clo)
{
	char buf[UDPC_PKTLEN];
	union ud_sockaddr_u sa;
	socklen_t salen = sizeof(sa);
	struct epoll_event ev[1];
	ssize_t nrd;

	if (epoll_wait(epfd, ev, 1, 1) == 0) {
		;
	} else if ((ev->events & EPOLLIN) == 0) {
		;
	} else if (ev->data.fd != mcfd) {
		;
	} else if ((nrd = recvfrom(
			    mcfd, buf, sizeof(buf), 0, &sa.sa, &salen)) <= 0) {
		;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, buf})) {
		;
	} else {
		// YAAAY
		char ia[INET6_ADDRSTRLEN];
		const char *a = inet_ntop(
			sa.sa.sa_family, &sa.sa6.sin6_addr, ia, sizeof(ia));
		short unsigned int port = ntohs(sa.sa6.sin6_port);

		fprintf(stderr, "[%s]:%hu -> %d\t%zd\n", a, port, mcfd, nrd);

		switch (udpc_pkt_cmd((ud_packet_t){nrd, buf})) {
		case UDPC_PKT_RPL(UTE_QMETA):
			pmeta(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
			break;
		case UTE_CMD:
			// that's what we need!
			party(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));

			for (size_t i = 1; i < npos; i++) {
				if (bitset_get(change, i)) {
					if (offs[i]) {
						fprintf(stderr, "\
%s %f %f\n", syms + offs[i - 1], mkt_bid[i].p, mkt_ask[i].p);
					} else {
						fprintf(stderr, "\
%zu %f %f\n", i, mkt_bid[i].p, mkt_ask[i].p);
					}
					// adapt our orders
					adapt((TwsDL*)clo, i);
				}
			}
		default:
			break;
		}
	}
	return;
}

/* tws-order.cpp ends here */