/*** tseries.h -- stuff that is soon to be replaced by ffff's tseries
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#if !defined INCLUDED_tseries_h_
#define INCLUDED_tseries_h_

#include <stdbool.h>
#include <time.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"
/* comes from sushi */
#include <sushi/secu.h>
#include <sushi/scommon.h>
#include <sushi/sl1t.h>
#include <sushi/scdl.h>

/* tick services */
#if !defined index_t
# define index_t	size_t
#endif	/* !index_t */

/**
 * Time series (per instrument) and market snapshots (per point in time)
 * need the best of both worlds, low latency on the one hand and small
 * memory footprint on the other, yet allowing for a rich variety of
 * gatherable information.
 * Specifically we want to answer questions like:
 * -
 * -
 **/

/**
 * Service 4220:
 * Find ticks of one time stamp over instruments (market snapshot).
 * This service can be used to get a succinct set of ticks, usually the
 * last ticks before a given time stamp, for several instruments.
 * The ticks to return can be specified in a bitset.
 *
 * sig: 4220(ui32 ts, ui32 types, (ui32 secu, ui32 fund, ui32 exch)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h */
#define UD_SVC_TICK_BY_TS	0x4220

/**
 * Service 4222:
 * Find ticks of one instrument at several given points in time.
 *
 * sig: 4222(ui32 secu, ui32 fund, ui32 exch, ui32 types, (ui32 ts)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h
 * This service is best used in conjunction with `ud_find_ticks_by_instr'
 * because several optimisation strategies apply. */
#define UD_SVC_TICK_BY_INSTR	0x4222

/**
 * Service 4224:
 * Get URNs by secu.
 *
 * sig: 4224(ui32 secu, ui32 fund, ui32 exch)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The answer will be a rpl4224_s object, which is a tseries_s in disguise. */
#define UD_SVC_GET_URN		0x4224
/* just a convenience thingie */
#define rpl4224_t	tseries_t
#define rpl4224_s	tseries_s

/**
 * Service 4226:
 * Refetch URNs. */
#define UD_SVC_FETCH_URN	0x4226


/* migrate to ffff tseries */
typedef struct tser_pkt_s *tser_pkt_t;

typedef struct tslab_s *tslab_t;

typedef struct tick_by_ts_hdr_s *tick_by_ts_hdr_t;
typedef struct tick_by_instr_hdr_s *tick_by_instr_hdr_t;
typedef struct sl1tp_s *sl1tp_t;

typedef uint32_t date_t;

/**
 * Condensed version of:
 *   // upper 10 bits
 *   uint10_t millisec_flags;
 *   // lower 6 bits
 *   uint6_t tick_type;
 * where the millisec_flags slot uses the values 0 to 999 if it
 * is a valid available tick and denotes the milliseconds part
 * of the timestamp and special values 1000 to 1023 if it's not,
 * whereby:
 * - 1023 TICK_NA denotes a tick that is not available, as in it
 *   is unknown to the system whether or not it exists
 * - 1022 TICK_NE denotes a tick that is known not to exist
 * - 1021 TICK_OLD denotes a tick that is too old and hence
 *   meaningless in the current context
 * - 1020 TICK_SOON denotes a tick that is known to exist but
 *   is out of reach at the moment, a packet retransmission will
 *   be necessary
 * In either case the actual timestamp and value slot of the tick
 * structure has become meaningless, therefore it is possible to
 * transfer even shorter, denser versions of the packet in such
 * cases, saving 64 bits, at the price of non-uniformity.
 **/
typedef uint16_t l1t_auxinfo_t;

#define TICK_NA		((uint16_t)1023)
#define TICK_NE		((uint16_t)1022)
#define TICK_OLD	((uint16_t)1021)
#define TICK_SOON	((uint16_t)1020)


/**
 * Sparse level 1 ticks, packed. */
struct sl1tp_s {
	uint32_t inst;
	uint32_t unit;
	uint16_t exch;
	l1t_auxinfo_t auxinfo;
	/* these are actually optional */
	uint32_t ts;
	uint32_t val;
};

struct tick_by_ts_hdr_s {
	time_t ts;
	uint32_t types;
};

struct tick_by_instr_hdr_s {
	su_secu_t secu;
	uint32_t types;
};

/**
 * Tslabs, roughly metadata that describe the slabs of tseries. */
struct tslab_s {
	su_secu_t secu;
	time_t from, till;
	uint32_t types;
};

#if defined INCLUDED_uterus_h_
/* packet of 16 sparse level1 ticks, still fuck ugly */
struct tser_pkt_s {
	struct sl1t_s t[16];
};

/* although we have stored uterus blocks in our tseries, the stuff that
 * gets sent is slightly different.
 * We send sparsely (ohlcv_p_s + admin) candles where then uterus macros
 * can be used to bang these into uterus blocks again. */

static inline void
spDute_bang_secu(spDute_t tgt, su_secu_t s, uint8_t tt, dse16_t pivot)
{
	tgt->instr = su_secu_quodi(s);
	tgt->unit = su_secu_quoti(s);
	tgt->mux = ((su_secu_pot(s) & 0x3ff) << 6) | (tt & 0x3f);
	tgt->pivot = pivot;
	return;
}

static inline void
spDute_bang_tser(
	spDute_t tgt, su_secu_t s, tt_t tt,
	dse16_t t, tser_pkt_t pkt, uint8_t idx)
{
	spDute_bang_secu(tgt, s, tt, t);
#if 0
	/* pkt should consist of uterus blocks */
	switch (tt) {
	case PFTT_BID:
	case PFTT_ASK:
	case PFTT_TRA:
		ute_frob_ohlcv_p(&tgt->ohlcv, tt, &pkt->t[idx]);
		break;
	case PFTT_STL:
		tgt->pri = pkt->t[idx].x.p;
		break;
	case PFTT_FIX:
		tgt->pri = pkt->t[idx].f.p;
		break;
	default:
		break;
	}
#endif
	return;
}

static inline void
spDute_bang_nexist(spDute_t tgt, su_secu_t s, uint8_t tt, dse16_t t)
{
	spDute_bang_secu(tgt, s, tt, t);
#if 0
	switch (tt) {
	case PFTT_BID:
	case PFTT_ASK:
	case PFTT_TRA:
		ute_fill_ohlcv_p_nexist(&tgt->ohlcv);
		break;
	default:
		tgt->pri = UTE_NEXIST;
		break;
	}
#endif
	return;
}

static inline void
spDute_bang_onhold(spDute_t tgt, su_secu_t s, uint8_t tt, dse16_t t)
{
	spDute_bang_secu(tgt, s, tt, t);
#if 0
	switch (tt) {
	case PFTT_BID:
	case PFTT_ASK:
	case PFTT_TRA:
		ute_fill_ohlcv_p_onhold(&tgt->ohlcv);
		break;
	default:
		tgt->pri = UTE_ONHOLD;
		break;
	}
#endif
	return;
}
#endif	/* INCLUDED_uterus_h_ */


/**
 * Deliver a tick packet for S at TS.
 * \param hdl the unserding handle to use
 * \param tgt a buffer that holds the tick packet, should be at least
 *   UDPC_PLLEN bytes long
 * \param s the security to look up
 * \param bs a bitset to filter for ticks
 * \param ts the timestamp at which S shall be valuated
 * \return the length of the tick packet
 **/
extern size_t
ud_find_one_price(ud_handle_t, char *tgt, su_secu_t s, uint32_t bs, time_t ts);

/**
 * Deliver a packet storm of ticks for instruments specified by S at TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(scom_t, void *clo), void *clo,
	su_secu_t *s, size_t slen,
	uint32_t bs, time_t ts);

#if defined INCLUDED_uterus_h_
typedef void(*ud_find_ticks_by_instr_cb_f)(spDute_t, void *clo);

/**
 * Deliver a packet storm of ticks for S at specified times TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	ud_find_ticks_by_instr_cb_f cb, void *clo,
	su_secu_t s, uint32_t bs,
	time_t *ts, size_t tslen);
#endif	/* INCLUDED_uterus_h_ */


/* inlines, type (de)muxers */
static inline l1t_auxinfo_t
l1t_auxinfo(uint16_t msec, uint8_t tt)
{
	return (msec << 6) | (tt & 0x3f);
}

static inline l1t_auxinfo_t
l1t_auxinfo_set_msec(l1t_auxinfo_t src, uint16_t msec)
{
	return (src & 0x3f) | (msec << 6);
}

static inline l1t_auxinfo_t
l1t_auxinfo_set_tt(l1t_auxinfo_t src, uint8_t tt)
{
	return (src & 0xffc0) | (tt & 0x3f);
}

static inline uint16_t
l1t_auxinfo_msec(l1t_auxinfo_t ai)
{
	return (ai >> 6);
}

static inline uint8_t
l1t_auxinfo_tt(l1t_auxinfo_t ai)
{
	return (ai & 0x3f);
}

/* the sl1tp packed tick, consumes 12 or 20 bytes */
static inline void
fill_sl1tp_shdr(sl1tp_t l1t, uint32_t secu, uint32_t fund, uint16_t exch)
{
	l1t->inst = secu;
	l1t->unit = fund;
	l1t->exch = exch;
	return;
}

static inline void
fill_sl1tp_tick(sl1tp_t l1t, time_t ts, uint16_t msec, uint8_t tt, uint32_t v)
{
	l1t->auxinfo = l1t_auxinfo(msec, tt);
	l1t->ts = ts;
	l1t->val = v;
	return;
}

static inline uint32_t
sl1tp_value(sl1tp_t t)
{
	return t->val;
}

static inline void
sl1tp_set_value(sl1tp_t tgt, uint32_t v)
{
	tgt->val = v;
	return;
}

static inline uint8_t
sl1tp_tt(sl1tp_t t)
{
	return l1t_auxinfo_tt(t->auxinfo);
}

static inline void
sl1tp_set_tt(sl1tp_t t, uint8_t tt)
{
	l1t_auxinfo_set_tt(t->auxinfo, tt);
	return;
}

static inline uint16_t
sl1tp_msec(sl1tp_t t)
{
	return l1t_auxinfo_msec(t->auxinfo);
}

static inline uint32_t
sl1tp_ts(sl1tp_t t)
{
	return t->ts;
}

static inline void
sl1tp_set_stamp(sl1tp_t t, uint32_t ts, uint16_t msec)
{
	t->ts = ts;
	l1t_auxinfo_set_msec(t->auxinfo, msec);
	return;
}

static inline uint32_t
sl1tp_inst(sl1tp_t t)
{
	return t->inst;
}

static inline void
sl1tp_set_inst(sl1tp_t t, uint32_t inst)
{
	t->inst = inst;
	return;
}

static inline uint32_t
sl1tp_unit(sl1tp_t t)
{
	return t->unit;
}

static inline void
sl1tp_set_unit(sl1tp_t t, uint32_t unit)
{
	t->unit = unit;
	return;
}

static inline uint16_t
sl1tp_exch(sl1tp_t t)
{
	return t->exch;
}

static inline void
sl1tp_set_exch(sl1tp_t t, uint16_t exch)
{
	t->exch = exch;
	return;
}

#if defined INCLUDED_uterus_h_ && 0
/* them old sl1t ticks, consumes 28 bytes */
static inline void
fill_sl1t_secu(sl1t_t l1t, uint32_t secu, int32_t fund, uint16_t exch)
{
	l1t->secu = su_secu(secu, fund, exch);
	return;
}

static inline void
fill_sl1t_tick(sl1t_t l1t, time_t ts, uint16_t msec, tt_t tt, uint32_t v)
{
	l1t->tick.ts = ts;
	l1t->tick.nsec = msec * 1000000;
	l1t->tick.tt = tt;
	l1t->tick.value = v;
	return;
}

static inline uint8_t
index_in_pkt(dse16_t dse)
{
/* find the index of the date encoded in dse inside a tick bouquet */
	dse16_t anchor = time_to_dse(442972800);
	uint8_t res = (dse - anchor) % 14;
	static uint8_t offs[14] = {
		-1, 0, 1, 2, 3, 4, -1, -1, 5, 6, 7, 8, 9, -1
	};
	return offs[res];
}

static inline dse16_t
tser_pkt_beg_dse(dse16_t dse)
{
	uint8_t sub = index_in_pkt(dse);
	return dse - sub;
}
#endif	/* INCLUDED_uterus_h_ */


/* (de)serialisers */
static inline void
udpc_seria_add_tick_by_ts_hdr(udpc_seria_t sctx, tick_by_ts_hdr_t t)
{
	udpc_seria_add_ui32(sctx, t->ts);
	udpc_seria_add_ui32(sctx, t->types);
	return;
}

static inline void
udpc_seria_des_tick_by_ts_hdr(tick_by_ts_hdr_t t, udpc_seria_t sctx)
{
	t->ts = udpc_seria_des_ui32(sctx);
	t->types = udpc_seria_des_ui32(sctx);
	return;
}

static inline void
udpc_seria_add_secu(udpc_seria_t sctx, su_secu_t secu)
{
/* we assume sizeof(su_secu_t) == 8 */
	udpc_seria_add_ui64(sctx, su_secu_ui64(secu));
	return;
}

static inline su_secu_t
udpc_seria_des_secu(udpc_seria_t sctx)
{
/* we assume sizeof(su_secu_t) == 8 */
	uint64_t wire = udpc_seria_des_ui64(sctx);
	return su_secu_set_ui64(wire);
}

static inline void
udpc_seria_add_tick_by_instr_hdr(udpc_seria_t sctx, tick_by_instr_hdr_t h)
{
	udpc_seria_add_secu(sctx, h->secu);
	udpc_seria_add_ui32(sctx, h->types);
	return;
}

static inline void
udpc_seria_des_tick_by_instr_hdr(tick_by_instr_hdr_t h, udpc_seria_t sctx)
{
	h->secu = udpc_seria_des_secu(sctx);
	h->types = udpc_seria_des_ui32(sctx);
	return;
}

static inline void
udpc_seria_add_sl1t(udpc_seria_t sctx, const_sl1t_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_sl1t(sl1t_t t, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(t, sizeof(*t), sctx) > 0;
}

#if defined INCLUDED_uterus_h_ && 0
static inline void
udpc_seria_add_spDute(udpc_seria_t sctx, spDute_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_spDute(spDute_t t, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(t, sizeof(*t), sctx) > 0;
}
#endif	/* INCLUDED_uterus_h_ */

/* Attention, a tseries_t object gets transferred as tslab_t,
 * the corresponding udpc_seria_add_tseries() is in tscoll.h. */
static inline bool
udpc_seria_des_tslab(tslab_t ts, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(ts, sizeof(*ts), sctx) > 0;
}

#endif	/* INCLUDED_tseries_h_ */
