/*** tws-snarf.cpp -- push tws quotes to unserding beef channels
 *
 * Copyright (C) 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of twstools.
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
// for twsdl class
#include "twstools/twsdo.h"
#include "twstools/tws_meta.h"
#include "twstools/tws_query.h"
#include <assert.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>
#include <uterus.h>
#include <m30.h>

#if defined __INTEL_COMPILER
# pragma warning (disable:1419)
#endif	// __INTEL_COMPILER

extern "C" {
/* libtool needs C symbols */
extern void init(void*);
extern void fini(void*);
extern void work(void*);
}

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
static ud_chan_t hdl = NULL;
static char buf[UDPC_PKTLEN];
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
static inline void
udpc_seria_add_scom(udpc_seria_t sctx, scom_t s, size_t len)
{
	memcpy(sctx->msg + sctx->msgoff, s, len);
	sctx->msgoff += len;
	return;
}

static size_t nact = 0;
static struct act_s *act = NULL;


/* public exposure for the DSO, C linkage */
void init(void *UNUSED(clo))
{
	/* yep, one handle please */
	hdl = ud_chan_init(8584/*UT*/);
	return;
}

void fini(void *UNUSED(clo))
{
	/* free that activity counter */
	if (act) {
		free(act);
	}

	/* and off we fuck, kill the handle */
	if (hdl) {
		ud_chan_fini(hdl);
	}
	return;
}

void work(void *clo)
{
	TwsDL *parent = (TwsDL*)clo;
	const MktDataTodo &mtodo = parent->workTodo->getMktDataTodo();
	Quotes *q = parent->quotes;
	ud_packet_t pkt = {0, buf};
	struct udpc_seria_s ser[1];
	struct sl1t_s l1t[2];
	struct timeval now[1];
	static timeval last_brag[1];

	if (hdl == NULL) {
		return;
	} else if (gettimeofday(now, NULL) < 0) {
		/* bingo, time is fucked, what shall we do? */
		return;
	}

	// check for activity
	if (mtodo.mktDataRequests.size() > nact) {
		size_t nu = mtodo.mktDataRequests.size();
		act = (struct act_s*)realloc(act, nu * sizeof(struct act_s));
		memset(act + nact, 0, (nu - nact) * sizeof(struct act_s));
		nact = nu;
	}

#define PKT(x)		(ud_packet_t){ sizeof(x), x }
#define MAKE_PKT							\
	udpc_make_pkt(PKT(buf), -1, pno++, UDPC_PKT_RPL(UTE_QMETA));	\
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)))
#define SEND_PKT							\
	if (udpc_seria_msglen(ser)) {					\
		size_t __l = UDPC_HDRLEN + udpc_seria_msglen(ser);	\
		ud_packet_t p = {__l, buf};				\
		ud_chan_send(hdl, p);					\
	}

	if (now->tv_sec - last_brag->tv_sec > BRAG_INTV) {
		MAKE_PKT;
		for (size_t i = 0; i < mtodo.mktDataRequests.size(); i++) {
			char tmp[64];
			const IB::Contract &c =
				mtodo.mktDataRequests[i].ibContract;
			const char *sym = c.symbol.c_str();
			const char *ccy = c.currency.c_str();
			size_t len;

			len = snprintf(tmp, sizeof(tmp), "%s.%s", sym, ccy);

			if (udpc_seria_msglen(ser) + len + 2 + 4 > UDPC_PLLEN) {
				// send off the old guy
				SEND_PKT;
				// and make a new one
				MAKE_PKT;
			}
			// add the new guy
			udpc_seria_add_ui16(ser, i + 1);
			udpc_seria_add_str(ser, tmp, len);
		}
		// send remainder also
		SEND_PKT;
		// keep track of last brag date
		*last_brag = *now;
	}
#undef MAKE_PKT
#undef SEND_PKT

	// preset some fields
	sl1t_set_stmp_sec(l1t + 0, now->tv_sec);
	sl1t_set_stmp_msec(l1t + 0, now->tv_usec / 1000);
	sl1t_set_ttf(l1t + 0, SL1T_TTF_BID);
	// same for l1t + 1
	sl1t_set_stmp_sec(l1t + 1, now->tv_sec);
	sl1t_set_stmp_msec(l1t + 1, now->tv_usec / 1000);
	sl1t_set_ttf(l1t + 1, SL1T_TTF_ASK);

	udpc_make_pkt(pkt, 0, pno++, UTE_CMD);
	udpc_seria_init(ser, UDPC_PAYLOAD(pkt.pbuf), UDPC_PLLEN);
	for (unsigned int i = 0; i < mtodo.mktDataRequests.size(); i++) {
		IB::Contract c = mtodo.mktDataRequests[i].ibContract;
		double bid = q->at(i).val[IB::BID];
		double bsz = q->at(i).val[IB::BID_SIZE];
		double ask = q->at(i).val[IB::ASK];
		double asz = q->at(i).val[IB::ASK_SIZE];

		if ((l1t[0].bid = ffff_m30_get_d(bid).u) != act[i].bid.u ||
		    (l1t[0].bsz = ffff_m30_get_d(bsz).u) != act[i].bsz.u) {
			sl1t_set_tblidx(l1t + 0, i + 1);
			// and shove it up the seria
			udpc_seria_add_scom(
				ser, AS_SCOM(l1t + 0), sizeof(l1t[0]));
			// keep a note about activity
			act[i].bid.u = l1t[0].bid;
			act[i].bsz.u = l1t[0].bsz;
		}

		if ((l1t[1].ask = ffff_m30_get_d(ask).u) != act[i].ask.u ||
		    (l1t[1].asz = ffff_m30_get_d(asz).u) != act[i].asz.u) {
			sl1t_set_tblidx(l1t + 1, i + 1);
			// and shove it up the seria
			udpc_seria_add_scom(
				ser, AS_SCOM(l1t + 1), sizeof(l1t[1]));
			// keep track of activity
			act[i].ask.u = l1t[1].ask;
			act[i].asz.u = l1t[1].asz;
		}
	}

	if ((pkt.plen = udpc_seria_msglen(ser)) > 0) {
		pkt.plen += UDPC_HDRLEN;
		ud_chan_send(hdl, pkt);
	}
	return;
}

/* tws-snarf.cpp ends here */
