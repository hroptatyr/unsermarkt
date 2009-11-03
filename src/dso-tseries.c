/*** dso-tseries.c -- ticks of instruments
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "unserding-private.h"

#include "xdr-instr-seria.h"
#include "xdr-instr-private.h"

/* later to be decoupled from the actual source */
#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */
/* tseries stuff, to be replaced with ffff */
#include "tscache.h"
#include "tscoll.h"
#include "tseries.h"
#include "tseries-private.h"

tscache_t tscache = NULL;

#if defined DEBUG_FLAG
# define UD_DEBUG_TSER(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */


typedef struct spitfire_ctx_s *spitfire_ctx_t;
typedef enum spitfire_res_e spitfire_res_t;

struct spitfire_ctx_s {
	secu_t secu;
	size_t slen;
	index_t idx;
	time_t ts;
	uint32_t types;
};

enum spitfire_res_e {
	NO_TICKS,
	OUT_OF_SPACE,
	OUT_OF_TICKS,
};

static spitfire_res_t
spitfire(spitfire_ctx_t sfctx, udpc_seria_t sctx)
{
	struct sl1tick_s t;
	size_t trick = 1;

	/* start out with one tick per instr */
	while (sfctx->idx < sfctx->slen &&
	       sctx->msgoff < sctx->len - /*yuck*/7*8) {
		secu_t s = &sfctx->secu[sfctx->idx];

		if (trick && (1 << (PFTT_EOD))/* bollocks */ & sfctx->types) {
			gaid_t i = s->instr;
			gaid_t u = s->unit ? s->unit : 73380;
			gaid_t p = s->pot ? s->pot : 4;

			fill_sl1tick_shdr(&t, i, u, p);
			fill_sl1tick_tick(&t, sfctx->ts, 0, PFTT_EOD, 10000);
			udpc_seria_add_sl1tick(sctx, &t);
		}
		if (!trick && (1 << (PFTT_STL))/* bollocks */ & sfctx->types) {
			gaid_t i = s->instr;
			gaid_t u = s->unit ? s->unit : 73380;
			gaid_t p = s->pot ? s->pot : 4;

			fill_sl1tick_shdr(&t, i, u, p);
			fill_sl1tick_tick(&t, sfctx->ts, 0, PFTT_STL, 15000);
			udpc_seria_add_sl1tick(sctx, &t);
		}
		sfctx->idx += (trick ^= 1);
	}
	/* return false if this packet is meant to be the last one */
	return sfctx->idx < sfctx->slen;
}

static void
init_spitfire(spitfire_ctx_t ctx, secu_t secu, size_t slen, tick_by_ts_hdr_t t)
{
	ctx->secu = secu;
	ctx->slen = slen;
	ctx->idx = 0;
	ctx->ts = t->ts;
	ctx->types = t->types;
	return;
}

static void
instr_tick_by_ts_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	/* in args */
	struct tick_by_ts_hdr_s hdr;
	/* allow to filter for 64 instruments at once */
	struct secu_s filt[64];
	unsigned int nfilt = 0;
	struct spitfire_ctx_s sfctx;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_tick_by_ts_hdr(&hdr, &sctx);

	/* triples of instrument identifiers */
	while (udpc_seria_des_secu(&filt[nfilt], &sctx) &&
	       ++nfilt < countof(filt));

	UD_DEBUG("0x4220: ts:%d filtered for %u instrs\n", (int)hdr.ts, nfilt);
	/* initialise the spit state context */
	init_spitfire(&sfctx, filt, nfilt, &hdr);
	copy_pkt(&rplj, j);
	for (bool moar = true; moar;) {
		/* prepare the reply packet ... */
		clear_pkt(&rplsctx, &rplj);
		/* serialise some ticks */
		if ((moar = spitfire(&sfctx, &rplsctx))) {
			udpc_set_immed_frag_pkt(JOB_PACKET(&rplj));
		}
		/* send what we've got */
		send_pkt(&rplsctx, &rplj);
	}
	return;
}


/* number of stamps we can process at once */
#define NFILT	64

typedef struct oadt_ctx_s *oadt_ctx_t;
struct oadt_ctx_s {
	udpc_seria_t sctx;
	secu_t secu;
	tscoll_t coll;
	time_t filt[NFILT];
	size_t nfilt;
};

/* we sort the list of requested time stamps to collapse contiguous ones */
static index_t
selsort_minidx(time_t arr[], size_t narr, index_t offs)
{
	index_t minidx = offs;

	for (index_t i = offs+1; i < narr; i++) {
		if (arr[i] < arr[minidx]) {
			minidx = i;
		}
	}
	return minidx;
}

static inline void
selsort_swap(time_t arr[], index_t i1, index_t i2)
{
	time_t tmp;
	tmp = arr[i1];
	arr[i1] = arr[i2];
	arr[i2] = tmp;
	return;
}

static void
selsort_in_situ(time_t arr[], size_t narr)
{
	for (index_t i = 0; i < narr-1; i++) {
		index_t minidx;
		minidx = selsort_minidx(arr, narr, i);
		selsort_swap(arr, i, minidx);
	}
	return;
}

static size_t
snarf_times(udpc_seria_t sctx, time_t ts[], size_t nts)
{
	size_t nfilt = 0;
	while ((ts[nfilt] = udpc_seria_des_ui32(sctx)) && ++nfilt < nts);
	if (LIKELY(nfilt > 0)) {
		selsort_in_situ(ts, nfilt);
	}
	return nfilt;
}

static const char*
tsbugger(time_t ts)
{
	static char buf[32];
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	(void)gmtime_r(&ts, &tm);
	(void)strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &tm);
	return buf;
}

static void
spDute_bang_all(oadt_ctx_t octx, dse16_t refts, tser_pkt_t pkt, uint8_t idx)
{
	struct sparse_Dute_s tgt;

	spDute_bang_tser(&tgt, octx->secu, PFTT_BID, refts, pkt, idx);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_tser(&tgt, octx->secu, PFTT_ASK, refts, pkt, idx);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_tser(&tgt, octx->secu, PFTT_TRA, refts, pkt, idx);
	udpc_seria_add_spDute(octx->sctx, &tgt);

	spDute_bang_tser(&tgt, octx->secu, PFTT_STL, refts, pkt, idx);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_tser(&tgt, octx->secu, PFTT_FIX, refts, pkt, idx);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	return;
}

static void
spDute_bang_all_nexist(oadt_ctx_t octx, dse16_t refts)
{
	struct sparse_Dute_s tgt;

	spDute_bang_nexist(&tgt, octx->secu, PFTT_BID, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_nexist(&tgt, octx->secu, PFTT_ASK, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_nexist(&tgt, octx->secu, PFTT_TRA, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);

	spDute_bang_nexist(&tgt, octx->secu, PFTT_STL, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_nexist(&tgt, octx->secu, PFTT_FIX, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	return;
}

static void
spDute_bang_all_onhold(oadt_ctx_t octx, dse16_t refts)
{
	struct sparse_Dute_s tgt;

	spDute_bang_onhold(&tgt, octx->secu, PFTT_BID, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_onhold(&tgt, octx->secu, PFTT_ASK, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_onhold(&tgt, octx->secu, PFTT_TRA, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);

	spDute_bang_onhold(&tgt, octx->secu, PFTT_STL, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	spDute_bang_onhold(&tgt, octx->secu, PFTT_FIX, refts);
	udpc_seria_add_spDute(octx->sctx, &tgt);
	return;
}

static void
proc_one(oadt_ctx_t octx, time_t ts)
{
	tseries_t tser;
	tser_pkt_t pkt;
	dse16_t refts = time_to_dse(ts);
	uint8_t idx;

	/* this is the 10-ticks per 2 weeks fragment, so dont bother
	 * looking up saturdays and sundays */
#define TICKS_PER_FORTNIGHT	10
	if ((idx = index_in_pkt(refts)) >= TICKS_PER_FORTNIGHT) {
		/* leave a note in the packet? */
		UD_DEBUG_TSER("week end tick (%i %s)\n",
			      octx->secu->instr, tsbugger(ts));
		spDute_bang_all_nexist(octx, refts);

	} else if ((tser = tscoll_find_series(octx->coll, ts)) == NULL) {
		/* no way of obtaining ticks */
		UD_DEBUG_TSER("No suitable URN found (%i %s)\n",
			      octx->secu->instr, tsbugger(ts));
		spDute_bang_all_nexist(octx, refts);

	} else if ((pkt = tseries_find_pkt(tser, refts)) == NULL) {
		UD_DEBUG_TSER("URN not cached, deferring (%i %s)\n",
			      octx->secu->instr, tsbugger(ts));
		defer_frob(tser, refts - idx, false);
		spDute_bang_all_onhold(octx, refts);

	} else {
		/* bother the cache */
		UD_DEBUG("yay, cached\n");
		spDute_bang_all(octx, refts, pkt, idx);
	}
	return;
}

static inline bool
one_moar_p(oadt_ctx_t octx)
{
	size_t cur = udpc_seria_msglen(octx->sctx);
	size_t add = (sizeof(struct sparse_Dute_s) + 2) * 5;
	return cur + add < UDPC_PLLEN;
}

static index_t
proc_some(oadt_ctx_t octx, index_t i)
{
	for (; i < octx->nfilt && one_moar_p(octx); i++) {
		proc_one(octx, octx->filt[i]);
	}
	return i;
}

static void
instr_tick_by_instr_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct oadt_ctx_s oadtctx;
	struct job_s rplj;
	/* in args */
	struct tick_by_instr_hdr_s hdr;
	/* allow to filter for 64 time stamps at once */
	unsigned int nfilt = 0;
	tscoll_t tsc;
	bool moarp = true;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_tick_by_instr_hdr(&hdr, &sctx);
	/* get all the timestamps */
	if ((nfilt = snarf_times(&sctx, oadtctx.filt, NFILT)) == 0) {
		return;
	}
	UD_DEBUG("0x4222: %u/%u@%u filtered for %u time stamps\n",
		 hdr.secu.instr, hdr.secu.unit, hdr.secu.pot, nfilt);

	/* get us the tseries we're talking about */
	if ((tsc = find_tscoll_by_secu(tscache, &hdr.secu)) == NULL) {
		/* means we have no means of fetching */
		/* we could issue a packet saying so */
		UD_DEBUG("No way of fetching stuff\n");
		return;
	}

	/* initialise our context */
	oadtctx.sctx = &rplsctx;
	oadtctx.secu = &hdr.secu;
	oadtctx.coll = tsc;
	oadtctx.nfilt = nfilt;

	for (index_t i = 0; moarp;) {
		/* prepare the reply packet ... */
		copy_pkt(&rplj, j);
		clear_pkt(&rplsctx, &rplj);
		/* process some time stamps, this fills the packet */
		moarp = (i = proc_some(&oadtctx, i)) < oadtctx.nfilt;
		/* send what we've got so far */
		send_pkt(&rplsctx, &rplj);
	} while (moarp);

	/* we cater for any frobnication desires now */
	frobnicate();
	return;
}


/* urn getter */
static void
get_urn_cb(uint32_t lo, uint32_t hi, void *data, void *clo)
{
	tseries_t tser = data;
	char los[32], his[32];
	/* debugging mumbo jumbo */
	print_ts_into(los, sizeof(los), lo);
	print_ts_into(his, sizeof(his), hi);
	UD_DEBUG("found %s..%s %p %p\n", los, his, tser, clo);
	udpc_seria_add_data(clo, tser, sizeof(*tser));
	return;
}

static void
instr_urn_svc(job_t j)
{
	struct udpc_seria_s sctx;
	/* in args */
	struct secu_s secu;
	tscoll_t tsc;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_secu(&secu, &sctx);
	UD_DEBUG("0x%04x (UD_SVC_GET_URN): %u/%u@%u\n",
		 UD_SVC_GET_URN, secu.instr, secu.unit, secu.pot);

	/* get us the tseries we're talking about */
	if ((tsc = find_tscoll_by_secu(tscache, &secu)) == NULL) {
		/* means we have no means of fetching */
		/* we could issue a packet saying so */
		UD_DEBUG("No way of fetching stuff\n");
		return;
	}
	/* reuse buf and sctx, just traverse tscoll and send off the bugger */
	clear_pkt(&sctx, j);
	tscoll_trav_series(tsc, get_urn_cb, &sctx);
	send_pkt(&sctx, j);
	return;
}


/* fetch urn svc */
static void
fetch_urn_svc(job_t UNUSED(j))
{
	UD_DEBUG("0x%04x (UD_SVC_FETCH_URN)\n", UD_SVC_FETCH_URN);
	ud_set_service(UD_SVC_FETCH_URN, NULL, NULL);
#if defined HAVE_MYSQL
	fetch_urn_mysql();
#endif	/* HAVE_MYSQL */
	ud_set_service(UD_SVC_FETCH_URN, fetch_urn_svc, NULL);
	return;
}


static void*
cfgspec_get_source(void *ctx, void *spec)
{
#define CFG_SOURCE	"source"
	return udcfg_tbl_lookup(ctx, spec, CFG_SOURCE);
}

typedef enum {
	CST_UNK,
	CST_MYSQL,
} cfgsrc_type_t;

static cfgsrc_type_t
cfgsrc_type(void *ctx, void *spec)
{
#define CFG_TYPE	"type"
	const char *type = NULL;
	udcfg_tbl_lookup_s(&type, ctx, spec, CFG_TYPE);

	UD_DEBUG("type %s %p\n", type, spec);
	if (type == NULL) {
		return CST_UNK;
	} else if (memcmp(type, "mysql", 5) == 0) {
		return CST_MYSQL;
	}
	return CST_UNK;
}

static void
load_ticks_fetcher(void *clo, void *spec)
{
	void *src = cfgspec_get_source(clo, spec);

	/* pass along the src settings */
	udctx_set_setting(clo, src);

	/* find out about its type */
	switch (cfgsrc_type(clo, src)) {
	case CST_MYSQL:
#if defined HAVE_MYSQL
		/* fetch some instruments by sql */
		dso_tseries_mysql_LTX_init(clo);
#endif	/* HAVE_MYSQL */
		break;

	case CST_UNK:
	default:
		/* do fuckall */
		break;
	}

	/* also load the frobber in this case */
	dso_tseries_frobq_LTX_init(clo);

	/* clean up */
	udctx_set_setting(clo, NULL);
	udcfg_tbl_free(clo, src);
	return;
}

static void
unload_ticks_fetcher(void *UNUSED(clo))
{
#if defined HAVE_MYSQL
	/* fetch some instruments by sql */
	dso_tseries_mysql_LTX_deinit(clo);
#endif	/* HAVE_MYSQL */
	return;
}


void
dso_tseries_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	UD_DEBUG("mod/tseries: loading ...");
	/* create the catalogue */
	tscache = make_tscache();
	/* tick service */
	ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_by_ts_svc, NULL);
	ud_set_service(UD_SVC_TICK_BY_INSTR, instr_tick_by_instr_svc, NULL);
	ud_set_service(UD_SVC_GET_URN, instr_urn_svc, NULL);
	ud_set_service(UD_SVC_FETCH_URN, fetch_urn_svc, NULL);
	UD_DBGCONT("done\n");

	if ((settings = udctx_get_setting(ctx)) != NULL) {
		load_ticks_fetcher(clo, settings);
		/* be so kind as to unref the settings */
		udcfg_tbl_free(ctx, settings);
	}
	/* clean up */
	udctx_set_setting(ctx, NULL);
	return;
}

void
dso_tseries_LTX_deinit(void *clo)
{
	free_tscache(tscache);
	unload_ticks_fetcher(clo);
	return;
}

/* dso-tseries.c */
