/*** xdr-instr-seria.h -- unserding serialisation for xdr instruments
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#if !defined INCLUDED_xdr_instr_seria_h_
#define INCLUDED_xdr_instr_seria_h_

#include <stdbool.h>
#include <pfack/tick.h>
#include <time.h>
#include "seria.h"

/**
 * Time series (per instrument) and market snapshots (per point in time)
 * need the best of both worlds, low latency on the one hand and small
 * memory footprint on the other, yet allowing for a rich variety of
 * gatherable information.
 * Specifically we want to answer questions like:
 * -
 * -
 **/

/* points to the tick-type of the day */
#define sl1tick_s		sl1tp_s
#define sl1tick_t		sl1tp_t
#define fill_sl1tick_shdr	fill_sl1tp_shdr
#define fill_sl1tick_tick	fill_sl1tp_tick
#define sl1tick_value		sl1tp_value
#define sl1tick_tick_type	sl1tp_tt
#define sl1tick_timestamp	sl1tp_ts
#define sl1tick_msec		sl1tp_msec
#define sl1tick_instr		sl1tp_inst
#define sl1tick_unit		sl1tp_unit
#define sl1tick_pot		sl1tp_exch

typedef struct secu_s *secu_t;
typedef struct tick_by_ts_hdr_s *tick_by_ts_hdr_t;
typedef struct tick_by_instr_hdr_s *tick_by_instr_hdr_t;
typedef struct sl1tp_s *sl1tp_t;
typedef struct sl1t_s *sl1t_t;

struct tick_by_ts_hdr_s {
	time_t ts;
	uint32_t types;
};

struct tick_by_instr_hdr_s {
	gaid_t instr;
	uint32_t types;
};

struct secu_s {
	gaid_t instr;
	gaid_t unit;
	gaid_t pot;
};

struct sl1t_s {
	struct secu_s secu;
	struct l1tick_s tick;
};

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
 * Level 1 tick security header. */
struct l1t_shdr_s {
	uint32_t inst;
	uint32_t unit;
	uint16_t exch;
};	

/**
 * Dense level 1 ticks, packed. */
struct dl1tp_s {
	l1t_auxinfo_t auxinfo;
	uint32_t ts;
	uint32_t val;
};

/**
 * Sparse level 1 ticks, packed. */
struct sl1tp_s {
	struct l1t_shdr_s shdr;
	/* actually a union of l1t_auxinfo_t and this */
	struct dl1tp_s tick;
};


/* type (de)muxers */
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
fill_sl1tp_shdr(sl1tp_t l1t, gaid_t secu, gaid_t fund, gaid_t exch)
{
	l1t->shdr.inst = secu;
	l1t->shdr.unit = fund;
	l1t->shdr.exch = exch;
	return;
}

static inline void
fill_sl1tp_tick(sl1tp_t l1t, time_t ts, uint16_t msec, uint8_t tt, uint32_t v)
{
	l1t->tick.auxinfo = l1t_auxinfo(msec, tt);
	l1t->tick.ts = ts;
	l1t->tick.val = v;
	return;
}

static inline uint32_t
sl1tp_value(sl1tp_t t)
{
	return t->tick.val;
}

static inline uint8_t
sl1tp_tt(sl1tp_t t)
{
	return l1t_auxinfo_tt(t->tick.auxinfo);
}

static inline uint16_t
sl1tp_msec(sl1tp_t t)
{
	return l1t_auxinfo_msec(t->tick.auxinfo);
}

static inline uint32_t
sl1tp_ts(sl1tp_t t)
{
	return t->tick.ts;
}

static inline uint32_t
sl1tp_inst(sl1tp_t t)
{
	return t->shdr.inst;
}

static inline uint32_t
sl1tp_unit(sl1tp_t t)
{
	return t->shdr.unit;
}

static inline uint16_t
sl1tp_exch(sl1tp_t t)
{
	return t->shdr.exch;
}

/* them old sl1t ticks, consumes 28 bytes */
static inline void
fill_sl1t_secu(sl1t_t l1t, gaid_t secu, gaid_t fund, gaid_t exch)
{
	l1t->secu.instr = secu;
	l1t->secu.unit = fund;
	l1t->secu.pot = exch;
	return;
}

static inline void
fill_sl1t_tick(sl1t_t l1t, time_t ts, uint16_t msec, uint8_t tt, uint32_t v)
{
	l1t->tick.ts = ts;
	l1t->tick.nsec = msec * 1000000;
	l1t->tick.tt = tt;
	l1t->tick.value = v;
	return;
}


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
udpc_seria_add_secu(udpc_seria_t sctx, secu_t secu)
{
	udpc_seria_add_ui32(sctx, secu->instr);
	udpc_seria_add_ui32(sctx, secu->unit);
	udpc_seria_add_ui32(sctx, secu->pot);
	return;
}

static inline bool
udpc_seria_des_secu(secu_t t, udpc_seria_t sctx)
{
	if ((t->instr = udpc_seria_des_ui32(sctx)) == 0) {
		/* no gaid? fuck off early */
		return false;
	}
	/* currency */
	t->unit = udpc_seria_des_ui32(sctx);
	/* exchange */	
	t->pot = udpc_seria_des_ui32(sctx);
	return true;
}

static inline void
udpc_seria_add_sl1tick(udpc_seria_t sctx, sl1tick_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_sl1tick(sl1tick_t t, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(t, sizeof(*t), sctx) > 0;
}

#endif	/* INCLUDED_seria_h_ */
