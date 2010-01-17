/*** ud-tick.c -- convenience tool to obtain ticks by instruments
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

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"
/* should be included somewhere */
#include <sushi/m30.h>

#include "ud-time.h"
#include "clihelper.c"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

static char
ttc(scom_t t)
{
	switch (scom_thdr_ttf(t)) {
	case SCOM_FLAG_LM | SL1T_TTF_BID:
	case SL1T_TTF_BID:
		return 'b';
	case SCOM_FLAG_LM | SL1T_TTF_ASK:
	case SL1T_TTF_ASK:
		return 'a';
	case SCOM_FLAG_LM | SL1T_TTF_TRA:
	case SL1T_TTF_TRA:
		return 't';
	case SCOM_FLAG_LM | SL1T_TTF_STL:
	case SL1T_TTF_STL:
		return 'x';
	case SCOM_FLAG_LM | SL1T_TTF_FIX:
	case SL1T_TTF_FIX:
		return 'f';
	default:
		return 'u';
	}
}

static void
t1(scom_t t)
{
	const_sl1t_t tv = (const void*)t;
	double v0 = ffff_m30_d(ffff_m30_get_ui32(tv->v[0]));
	double v1 = ffff_m30_d(ffff_m30_get_ui32(tv->v[1]));

	fputc(' ', stdout);
	fputc(' ', stdout);
	fputc(ttc(t), stdout);

	fprintf(stdout, ":%2.4f %2.4f\n", v0, v1);
	return;
}

static void
t1c(scom_t t)
{
	const_scdl_t tv = (const void*)t;
	double o = ffff_m30_d(ffff_m30_get_ui32(tv->o));
	double h = ffff_m30_d(ffff_m30_get_ui32(tv->h));
	double l = ffff_m30_d(ffff_m30_get_ui32(tv->l));
	double c = ffff_m30_d(ffff_m30_get_ui32(tv->c));
	int32_t v = tv->cnt;
	int32_t ets = tv->end_ts;

	fprintf(stdout, " o:%2.4f h:%2.4f l:%2.4f c:%2.4f v:%i  e:%i\n",
		o, h, l, c, v, ets);
	return;
}

static void
t1s(scom_t t)
{
	const_ssnap_t tv = (const void*)t;
	double bp = ffff_m30_d(ffff_m30_get_ui32(tv->bp));
	double ap = ffff_m30_d(ffff_m30_get_ui32(tv->ap));
	double bq = ffff_m30_d(ffff_m30_get_ui32(tv->bq));
	double aq = ffff_m30_d(ffff_m30_get_ui32(tv->aq));
	double tvpr = ffff_m30_d(ffff_m30_get_ui32(tv->tvpr));
	double tq = ffff_m30_d(ffff_m30_get_ui32(tv->tq));

	fprintf(stdout,
		" b:%2.4f bs:%2.4f  a:%2.4f as:%2.4f "
		" tvpr:%2.4f tq:%2.4f\n",
		bp, bq, ap, aq, tvpr, tq);
	return;
}

static void
ne(scom_t UNUSED(t))
{
	fputs("  v:does not exist\n", stdout);
	return;
}

static void
oh(scom_t UNUSED(t))
{
	fputs("  v:deferred\n", stdout);
	return;
}

static void
t_cb(su_secu_t s, scom_t t, void *UNUSED(clo))
{
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
	uint16_t ttf = scom_thdr_ttf(t);
	char ttfc = ttc(t);
	int32_t ts = scom_thdr_sec(t);
	uint16_t ms = scom_thdr_msec(t);
	char tss[32];

	print_ts_into(tss, sizeof(tss), ts);
	fprintf(stdout, "tick storm, ticks:1 ii:%u/%i@%hu tt:%c  ts:%s.%03hu",
		qd, qt, p, ttfc, tss, ms);

	if (scom_thdr_nexist_p(t)) {
		ne(t);
	} else if (scom_thdr_onhold_p(t)) {
		oh(t);
	} else if (!scom_thdr_linked(t)) {
		t1(t);
	} else if (ttf == SSNP_FLAVOUR) {
		t1s(t);
	} else if (ttf > SCDL_FLAVOUR) {
		t1c(t);
	}
	return;
}

static time_t
parse_time(const char *t)
{
	struct tm tm;
	char *on;

	memset(&tm, 0, sizeof(tm));
	on = strptime(t, "%Y-%m-%d", &tm);
	if (on == NULL) {
		return 0;
	}
	if (on[0] == ' ' || on[0] == 'T' || on[0] == '\t') {
		on++;
	}
	(void)strptime(on, "%H:%M:%S", &tm);
	return timegm(&tm);
}

int
main(int argc, const char *argv[])
{
	/* vla */
	su_secu_t cid;
	int n = 0;
	time_t ts[argc-1];
	uint32_t bs =
		(1 << SL1T_TTF_BID) |
		(1 << SL1T_TTF_ASK) |
		(1 << SL1T_TTF_TRA) |
		(1 << SL1T_TTF_STL) |
		(1 << SL1T_TTF_FIX);

	if (argc <= 1) {
		fprintf(stderr, "Usage: ud-tick instr [date] [date] ...\n");
		exit(1);
	}

	if (argc == 2) {
		ts[0] = time(NULL);
	}

	for (int i = 2; i < argc; i++) {
		if ((ts[n] = parse_time(argv[i])) == 0) {
			fprintf(stderr, "invalid date format \"%s\", "
				"must be YYYY-MM-DDThh:mm:ss\n", argv[i]);
			exit(1);
		}
		n++;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* just a test */
	cid = secu_from_str(hdl, argv[1]);
	/* now kick off the finder */
	ud_find_ticks_by_instr(hdl, t_cb, NULL, cid, bs, ts, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
