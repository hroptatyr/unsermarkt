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

struct act_s {
	m30_t bid;
	m30_t ask;
	m30_t bsz;
	m30_t asz;
};


// unserding guts
static int mcfd = -1;
static int epfd = -1;
// conversation number
static unsigned int pno = 0;

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
typedef struct {
	double p;
	double q;
} *level_t;

typedef long unsigned int *bitset_t;

static level_t mkt_bid = NULL;
static level_t mkt_ask = NULL;
static char *syms = NULL;
static size_t *offs = NULL;
static bitset_t change = NULL;
static size_t npos = 0U;

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

		// rinse
		RINSE(mkt_bid, 1);
		RINSE(mkt_ask, 1);
		RINSE(offs, 1);
		RINSE(change, sizeof(*change) * CHAR_BIT);

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
		// fuck error checking, just bang the info we've got
		sz = udpc_seria_des_str(ser, &p);

		// check for resizes
		check_resz(idx);
		offs[idx] = sz + 1 + offs[idx - 1];

		{
			size_t rdsz = __roundup_2pow(offs[idx]);
			syms = (char*)realloc(syms, rdsz);
			strncpy(syms + offs[idx - 1], p, sz);
		}
	}
	return;
}


/* public exposure for the DSO, C linkage */
void init(void *UNUSED(clo))
{
	/* yep, one handle please */
	if ((mcfd = ud_mcast_init(8584/*UT*/)) >= 0) {
		struct epoll_event ev[1];

		ev->events = EPOLLIN;
		ev->data.fd = mcfd;
		if ((epfd = epoll_create(1)) >= 0) {
			epoll_ctl(epfd, EPOLL_CTL_ADD, mcfd, ev);
		}
	}
	return;
}

void fini(void *UNUSED(clo))
{
	if (epfd >= 0) {
		epoll_ctl(epfd, EPOLL_CTL_DEL, mcfd, NULL);
		close(epfd);
	}
	if (mcfd >= 0) {
		ud_mcast_fini(mcfd);
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

			for (size_t i = 0; i < npos; i++) {
				if (bitset_get(change, i)) {
					if (offs[i]) {
						fprintf(stderr, "\
%s %f %f\n", syms + offs[i - 1], mkt_bid[i].p, mkt_ask[i].p);
					} else {
						fprintf(stderr, "\
%d %f %f\n", i, mkt_bid[i].p, mkt_ask[i].p);
					}
				}
			}
		default:
			break;
		}
	}
	return;
}

/* tws-order.cpp ends here */
