/*** web.c -- quote snapper web services
 *
 * Copyright (C) 2012-2013 Sebastian Freundt
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#if defined HAVE_LIBFIXC_FIX_H
# include <libfixc/fix.h>
# include <libfixc/fixml-msg.h>
# include <libfixc/fixml-comp.h>
# include <libfixc/fixml-attr.h>
#endif	/* HAVE_LIBFIXC_FIX_H */
#define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
# include <uterus/m62.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
# include <m62.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#if defined WEB_ASP_QUOTREQ
# define WEB_ASP_SECDEF
# include "um-quod.h"
# include "quod-cache.h"
#endif	/* WEB_ASP_QUOTREQ */
#if defined WEB_ASP_REQFORPOSS
# include "um-apfd.h"
# include "apfd-cache.h"
#endif	/* WEB_ASP_REQFORPOSS */
#include "web.h"
#include "nifty.h"

#if defined DEBUG_FLAG
# define WEB_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define WEB_DEBUG(args...)
#endif	/* DEBUG_FLAG */


/* helpers */
static uint16_t
__find_idx(const char *str)
{
	if ((str = strstr(str, "idx="))) {
		long int idx = strtol(str + 4, NULL, 10);
		if (idx > 0 && idx < 65536) {
			return (uint16_t)idx;
		}
		return 0;
	}
#define MASS_QUOT	(0xffff)
	return MASS_QUOT;
}

static size_t
__find_ac(const char **tgt, const char *str)
{
	char *p;

	if ((p = strstr(str, "ac="))) {
		*tgt = p + 3;
		if ((p = strchr(*tgt, '&')) != NULL) {
			;
		} else if ((p = strchr(*tgt, ' ')) != NULL) {
			;
		} else if ((p = strchr(*tgt, '\n')) != NULL) {
			if (p[-1] == '\r') {
				p--;
			}
		} else {
			goto bugger;
		}
		*p = '\0';
		return p - *tgt;
	}
bugger:
	return 0UL;
}

/* date and time funs, could use libdut from dateutils */
static int
__leapp(unsigned int y)
{
	return !(y % 4);
}

static void
ffff_gmtime(struct tm *tm, const time_t t)
{
#define UTC_SECS_PER_DAY	(86400)
	static uint16_t __mon_yday[] = {
		/* cumulative, first element is a bit set of leap days to add */
		0xfff8, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};
	register int days;
	register int secs;
	register unsigned int yy;
	const uint16_t *ip;


	/* just go to day computation */
	days = (int)(t / UTC_SECS_PER_DAY);
	/* time stuff */
	secs = (int)(t % UTC_SECS_PER_DAY);

	/* gotta do the date now */
	yy = 1970;
	/* stolen from libc */
#define DIV(a, b)		((a) / (b))
/* we only care about 1901 to 2099 and there are no bullshit leap years */
#define LEAPS_TILL(y)		(DIV(y, 4))
	while (days < 0 || days >= (!__leapp(yy) ? 365 : 366)) {
		/* Guess a corrected year, assuming 365 days per year. */
		register unsigned int yg = yy + days / 365 - (days % 365 < 0);

		/* Adjust DAYS and Y to match the guessed year.  */
		days -= (yg - yy) * 365 +
			LEAPS_TILL(yg - 1) - LEAPS_TILL(yy - 1);
		yy = yg;
	}
	/* set the year */
	tm->tm_year = (int)yy - 1900;

	ip = __mon_yday;
	/* unrolled */
	yy = 13;
	if (days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy]) {
		yy = 1;
	}
	/* set the rest of the tm structure */
	tm->tm_mday = days - ip[yy] + 1;
	tm->tm_yday = days;
	tm->tm_mon = (int)yy - 1;

	tm->tm_sec = secs % 60;
	secs /= 60;
	tm->tm_min = secs % 60;
	secs /= 60;
	tm->tm_hour = secs;

	/* fix up leap years */
	if (UNLIKELY(__leapp(tm->tm_year))) {
		if ((ip[0] >> (yy)) & 1) {
			if (UNLIKELY(tm->tm_yday == 59)) {
				tm->tm_mon = 1;
				tm->tm_mday = 29;
			} else if (UNLIKELY(tm->tm_yday == ip[yy])) {
				tm->tm_mday = tm->tm_yday - ip[tm->tm_mon--];
			} else {
				tm->tm_mday--;
			}
		}
	}
	return;
}

static __attribute__((unused)) size_t
ffff_strfdtu(char *restrict buf, size_t bsz, time_t sec, unsigned int usec)
{
	struct tm tm[1];

	ffff_gmtime(tm, sec);
	/* libdut? */
	strftime(buf, bsz, "%FT%T", tm);
	buf[19] = '.';
	return 20U + snprintf(buf + 20, bsz - 20, "%06u+0000", usec);
}


/* unknown service */
static size_t
websvc_unk(char **restrict tgt, size_t tsz, struct websvc_s UNUSED(sd))
{
	static const char rsp[] = "\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <title>um-quosnp overview</title>\n\
  </head>\n\
\n\
  <body>\n\
    <section>\n\
      <header>\n\
        <h1>Services</h1>\n\
      </header>\n\
\n\
      <footer>\n\
        <hr/>\n\
        <address>\n\
          <a href=\"https://github.com/hroptatyr/unsermarkt/\">unsermarkt</a>\n\
        </address>\n\
      </footer>\n\
    </section>\n\
  </body>\n\
</html>\n\
";
	if (tsz < sizeof(rsp)) {
		return 0;
	}
	memcpy(*tgt, rsp, sizeof(rsp));
	return sizeof(rsp) - 1;
}


/* secdef service */
#if defined WEB_ASP_SECDEF
# if defined HAVE_LIBFIXC_FIX_H
static void
__secdef1(fixc_msg_t msg, uint16_t idx)
{
	static const struct fixc_fld_s msgtyp = {
		.tag = FIXC_MSG_TYPE,
		.typ = FIXC_TYP_MSGTYP,
		.mtyp = (fixc_msgt_t)FIXML_MSG_SecurityDefinition,
	};
	fixc_msg_t cchmsg = quod_cache[idx - 1].msg;

	if (UNLIKELY(cchmsg == NULL)) {
		return;
	}

	/* the message type */
	fixc_add_fld(msg, msgtyp);

	/* add all fields */
	for (size_t i = 0; i < cchmsg->nflds; i++) {
		struct fixc_fld_s cchfld = cchmsg->flds[i];
		struct fixc_tag_data_s d = fixc_get_tag_data(cchmsg, i);
		size_t j = msg->nflds;

		fixc_add_tag(msg, (fixc_attr_t)cchfld.tag, d.s, d.z);
		/* copy the .tpc and .cnt */
		msg->flds[j].tpc = cchfld.tpc;
		msg->flds[j].cnt = cchfld.cnt;
	}
	return;
}

static size_t
websvc_secdef(char **restrict tgt, size_t UNUSED(tsz), struct websvc_s sd)
{
	size_t nsy = ute_nsyms(uctx);
	fixc_msg_t msg;
	struct fixc_rndr_s r;

	WEB_DEBUG("printing secdef idx %hu\n", sd.secdef.idx);

	if (!sd.secdef.idx) {
		return 0;
	}


	/* start a fix msg for that */
	msg = make_fixc_msg((fixc_msgt_t)FIXC_MSGT_UNK);

	if (sd.secdef.idx <= nsy) {
		__secdef1(msg, sd.secdef.idx);
	} else if (sd.quotreq.idx == MASS_QUOT) {
		/* loop over instruments */
		msg->f35.mtyp = FIXC_MSGT_BATCH;
		for (size_t i = 1; i <= nsy; i++) {
			__secdef1(msg, i);
		}
	}

	/* render the whole shebang */
	r = fixc_render_fixml_rndr(msg);
	free_fixc(msg);
	/* get ready for the harvest */
	*tgt = r.str;
	return r.len;
}

# else  /* !HAVE_LIBFIXC_FIX_H */
static const char fixml_pre[] = "\
<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\
<FIXML xmlns=\"http://www.fixprotocol.org/FIXML-5-0-SP2\">\n\
";
static const char fixml_post[] = "\
</FIXML>\n\
";
static const char fixml_batch_pre[] = "<Batch>\n";
static const char fixml_batch_post[] = "</Batch>\n";

static size_t
__secdef1(char *restrict tgt, size_t tsz, uint16_t idx)
{
	size_t res = 0UL;

	if (quod_cache[idx - 1].sd && tsz) {
		res = quod_cache[idx - 1].sdsz;

		if (tsz < (size_t)res) {
			res = tsz - 1;
		}
		memcpy(tgt, quod_cache[idx - 1].sd, res);
		tgt[res] = '\0';
	}
	return res;
}

static size_t
websvc_secdef(char **restrict tgt, size_t tsz, struct websvc_s sd)
{
	size_t idx = 0;
	size_t nsy = ute_nsyms(uctx);

	WEB_DEBUG("printing secdef idx %hu\n", sd.secdef.idx);

	if (!sd.secdef.idx) {
		return 0;
	}

	/* copy pre */
	memcpy(tgt + idx, fixml_pre, sizeof(fixml_pre));
	idx += sizeof(fixml_pre) - 1;

	if (sd.secdef.idx <= nsy) {
		idx += __secdef1(*tgt + idx, tsz - idx, sd.secdef.idx);
	} else if (sd.quotreq.idx == MASS_QUOT) {
		memcpy(tgt + idx, fixml_batch_pre, sizeof(fixml_batch_pre));
		idx += sizeof(fixml_batch_pre) - 1;
		/* loop over instruments */
		for (size_t i = 1; i <= nsy; i++) {
			idx += __secdef1(*tgt + idx, tsz - idx, i);
		}
		memcpy(tgt + idx, fixml_batch_post, sizeof(fixml_batch_post));
		idx += sizeof(fixml_batch_post) - 1;
	}

	/* copy post */
	memcpy(tgt + idx, fixml_post, sizeof(fixml_post));
	idx += sizeof(fixml_post) - 1;
	return idx;
}
# endif	/* HAVE_LIBFIXC_FIX_H */
#endif	/* WEB_ASP_SECDEF */


/* quotreq service */
#if defined WEB_ASP_QUOTREQ
# if defined HAVE_LIBFIXC_FIX_H
static void
__quotreq1(fixc_msg_t msg, uint16_t idx, struct timeval now)
{
	static size_t qid = 0;
	static char p[32];
	static char vtm[32];
	static char txn[32];
	static size_t txz, vtz;
	static const struct fixc_fld_s msgtyp = {
		.tag = 35,
		.typ = FIXC_TYP_MSGTYP,
		.mtyp = (fixc_msgt_t)FIXML_MSG_Quote,
	};
	static struct timeval now_cch;
	const_sl1t_t b = quod_cache[idx - 1].bid;
	const_sl1t_t a = quod_cache[idx - 1].ask;
	size_t z;

	/* find the more recent quote out of bid and ask */
	{
		time_t bs = sl1t_stmp_sec(b);
		unsigned int bms = sl1t_stmp_msec(b);
		time_t as = sl1t_stmp_sec(a);
		unsigned int ams = sl1t_stmp_msec(a);

		if (bs <= as) {
			bs = as;
			bms = ams;
		}
		if (UNLIKELY(bs == 0)) {
			return;
		}

		txz = ffff_strfdtu(txn, sizeof(txn), bs, bms * 1000);
	}

	if (now_cch.tv_sec != now.tv_sec) {
		vtz = ffff_strfdtu(vtm, sizeof(vtm), now.tv_sec, now.tv_usec);
		now_cch = now;
	}

	/* the message type */
	fixc_add_fld(msg, msgtyp);

	fixc_add_fld(msg, (struct fixc_fld_s){.tag = FIXML_ATTR_QuoteID/*QID*/,
				     .typ = FIXC_TYP_INT, .i32 = ++qid});
	z = ffff_m30_s(p, (m30_t)b->pri);
	fixc_add_tag(msg, (fixc_attr_t)132/*BidPx*/, p, z);

	z = ffff_m30_s(p, (m30_t)a->pri);
	fixc_add_tag(msg, (fixc_attr_t)133/*OfrPx*/, p, z);

	z = ffff_m30_s(p, (m30_t)b->qty);
	fixc_add_tag(msg, (fixc_attr_t)134/*BidSz*/, p, z);

	z = ffff_m30_s(p, (m30_t)a->qty);
	fixc_add_tag(msg, (fixc_attr_t)135/*OfrSz*/, p, z);

	fixc_add_tag(msg, (fixc_attr_t)60/*TxnTm*/, txn, txz);
	fixc_add_tag(msg, (fixc_attr_t)62/*ValidUntilTm*/, vtm, vtz);

	/* see if there's an instrm block */
	if (quod_cache[idx - 1].ins) {
		/* fixc_add_msg(msg, cache[idx - 1].ins); */
		fixc_msg_t ins = quod_cache[idx - 1].ins;

		for (size_t i = 0; i < ins->nflds; i++) {
			struct fixc_fld_s fld = ins->flds[i];
			struct fixc_tag_data_s d = fixc_get_tag_data(ins, i);
			size_t mi = msg->nflds;

			fixc_add_tag(msg, (fixc_attr_t)fld.tag, d.s, d.z);
			/* bang .cnt and .tpc */
			msg->flds[mi].tpc = fld.tpc;
			msg->flds[mi].cnt = fld.cnt;
		}
	} else {
		/* have to mimick the instr somehow */
		static char buf[8];
		const char *sym = ute_idx2sym(uctx, idx);
		size_t ssz = strlen(sym);
		size_t mi;

		mi = msg->nflds;
		fixc_add_tag(msg, (fixc_attr_t)FIXML_ATTR_Symbol, sym, ssz);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 0;

		mi = msg->nflds;
		ssz = snprintf(buf, sizeof(buf), "%hu", idx);
		fixc_add_tag(msg, (fixc_attr_t)FIXML_ATTR_SecurityID, buf, ssz);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 1;

		mi = msg->nflds;
		fixc_add_tag(
			msg, (fixc_attr_t)FIXML_ATTR_SecurityIDSource,
			"100", 3);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 2;
	}
	return;
}

static size_t
websvc_quotreq(char **restrict tgt, size_t UNUSED(tsz), struct websvc_s sd)
{
	size_t nsy = ute_nsyms(uctx);
	struct timeval now[1];
	fixc_msg_t msg;
	struct fixc_rndr_s r;

	WEB_DEBUG("printing quotreq idx %hu\n", sd.quotreq.idx);

	if (!sd.quotreq.idx) {
		return 0;
	}

	/* get current time */
	gettimeofday(now, NULL);

	/* start a fix msg for that */
	msg = make_fixc_msg((fixc_msgt_t)FIXC_MSGT_UNK);

	if (sd.quotreq.idx < nsy) {
		__quotreq1(msg, sd.quotreq.idx, *now);
	} else if (sd.quotreq.idx == MASS_QUOT) {
		/* loop over instruments */
		msg->f35.mtyp = FIXC_MSGT_BATCH;
		for (size_t i = 1; i <= nsy; i++) {
			__quotreq1(msg, i, *now);
		}
	}

	/* render the whole shebang */
	r = fixc_render_fixml_rndr(msg);
	free_fixc(msg);
	/* get ready for the harvest */
	*tgt = r.str;
	return r.len;
}

# else  /* !HAVE_LIBFIXC_FIX_H */
static size_t
__quotreq1(char *restrict tgt, size_t tsz, uint16_t idx, struct timeval now)
{
	static char eoquot[] = "</Quot>\n";
	static size_t qid = 0;
	static char bp[16], ap[16], bq[16], aq[16];
	static char vtm[32];
	static char txn[32];
	static struct timeval now_cch;
	const char *sym = ute_idx2sym(uctx, idx);
	const_sl1t_t b = quod_cache[idx - 1].bid;
	const_sl1t_t a = quod_cache[idx - 1].ask;
	const char *instrmt = quod_cache[idx - 1].instrmt;
	size_t instrmtsz = quod_cache[idx - 1].instrmtsz;
	int len;

	/* find the more recent quote out of bid and ask */
	{
		time_t bs = sl1t_stmp_sec(b);
		unsigned int bms = sl1t_stmp_msec(b);
		time_t as = sl1t_stmp_sec(a);
		unsigned int ams = sl1t_stmp_msec(a);

		if (bs <= as) {
			bs = as;
			bms = ams;
		}
		if (UNLIKELY(bs == 0)) {
			return 0;
		}

		ffff_strfdtu(txn, sizeof(txn), bs, bms * 1000);
	}

	ffff_m30_s(bp, (m30_t)b->pri);
	ffff_m30_s(bq, (m30_t)b->qty);
	ffff_m30_s(ap, (m30_t)a->pri);
	ffff_m30_s(aq, (m30_t)a->qty);

	if (now_cch.tv_sec != now.tv_sec) {
		ffff_strfdtu(vtm, sizeof(vtm), now.tv_sec, now.tv_usec);
		now_cch = now;
	}

	len = snprintf(
		tgt, tsz, "\
  <Quot QID=\"%zu\" \
BidPx=\"%s\" OfrPx=\"%s\" BidSz=\"%s\" OfrSz=\"%s\" \
TxnTm=\"%s\" ValidUntilTm=\"%s\">",
		++qid, bp, ap, bq, aq, txn, vtm);

	/* see if there's an instrm block */
	if (instrmt) {
		if (instrmtsz > tsz - len) {
			instrmtsz = tsz - len;
		}
		memcpy(tgt + len, instrmt, instrmtsz);
		len += instrmtsz;
	} else {
		len += snprintf(
			tgt + len, tsz - len, "\
<Instrmt Sym=\"%s\" ID=\"%hu\" Src=\"100\"/>",
			sym, idx);
	}

	/* close the Quot tag */
	if (sizeof(eoquot) < tsz - len) {
		memcpy(tgt + len, eoquot, sizeof(eoquot));
		len += sizeof(eoquot) - 1;
	}
	return len;
}

static size_t
websvc_quotreq(char **restrict tgt, size_t tsz, struct websvc_s sd)
{
	size_t idx = 0;
	size_t nsy = ute_nsyms(uctx);
	struct timeval now[1];

	WEB_DEBUG("printing quotreq idx %hu\n", sd.quotreq.idx);

	if (!sd.quotreq.idx) {
		return 0;
	}

	/* get current time */
	gettimeofday(now, NULL);

	/* copy pre */
	memcpy(tgt + idx, fixml_pre, sizeof(fixml_pre));
	idx += sizeof(fixml_pre) - 1;

	if (sd.quotreq.idx < nsy) {
		idx += __quotreq1(*tgt + idx, tsz - idx, sd.quotreq.idx, *now);
	} else if (sd.quotreq.idx == MASS_QUOT) {
		memcpy(tgt + idx, fixml_batch_pre, sizeof(fixml_batch_pre));
		idx += sizeof(fixml_batch_pre) - 1;
		/* loop over instruments */
		for (size_t i = 1; i <= nsy; i++) {
			idx += __quotreq1(*tgt + idx, tsz - idx, i, *now);
		}
		memcpy(tgt + idx, fixml_batch_post, sizeof(fixml_batch_post));
		idx += sizeof(fixml_batch_post) - 1;
	}

	/* copy post */
	memcpy(tgt + idx, fixml_post, sizeof(fixml_post));
	idx += sizeof(fixml_post) - 1;
	return idx;
}
# endif	/* HAVE_LIBFIXC_FIX_H */
#endif	/* WEB_ASP_QUOTREQ */


/* reqforposs service */
#if defined WEB_ASP_REQFORPOSS
# if defined HAVE_LIBFIXC_FIX_H
static void
__posrpt1(fixc_msg_t msg, uint16_t idx, struct timeval now)
{
	static char p[32];
	static char vtm[32];
	static char txn[32];
	static char sbuf[64U + 64U];
	static size_t txz, vtz;
	static const struct fixc_fld_s msgtyp = {
		.tag = 35,
		.typ = FIXC_TYP_MSGTYP,
		.mtyp = (fixc_msgt_t)FIXML_MSG_PositionReport,
	};
	static struct timeval now_cch;
	const_sl1t_t l = apfd_cache[idx].lng;
	const_sl1t_t s = apfd_cache[idx].shrt;
	size_t z;
	const char *ac;
	size_t az;
	const char *sym;
	size_t ssz;

	/* massage the sym name */
	sym = ute_idx2sym(uctx, idx);
	ssz = strlen(sym);
	memcpy(sbuf, sym, ssz);
	sbuf[ssz] = '\0';
	{
		char *q;
		if ((q = strchr(sbuf, '/')) != NULL) {
			*q = '\0';
			ac = sbuf;
			az = q++ - sbuf;
			sym = q;
			ssz -= q - sbuf;
		} else {
			ac = NULL;
			az = 0UL;
			sym = sbuf;
		}
	}

	/* find the more recent quote out of bid and ask */
	{
		time_t ls = sl1t_stmp_sec(l);
		unsigned int lms = sl1t_stmp_msec(l);
		time_t ss = sl1t_stmp_sec(s);
		unsigned int sms = sl1t_stmp_msec(s);

		if (ls <= ss) {
			ls = ss;
			lms = sms;
		}
		if (UNLIKELY(ls == 0)) {
			return;
		}

		txz = ffff_strfdtu(txn, sizeof(txn), ls, lms * 1000);
	}

	if (now_cch.tv_sec != now.tv_sec) {
		vtz = ffff_strfdtu(vtm, sizeof(vtm), now.tv_sec, now.tv_usec);
		now_cch = now;
	}

	/* the message type */
	fixc_add_fld(msg, msgtyp);

	/* rpt id */
	fixc_add_tag(msg, (fixc_attr_t)721/*RptID*/, vtm, vtz);
	/* business day we're talking */
	fixc_add_tag(msg, (fixc_attr_t)715/*BizDT*/, txn, 10);

	/* nopartyid */
	fixc_add_tag(msg, (fixc_attr_t)453/*NoPartyID*/, "1", 1);
	fixc_add_tag(msg, (fixc_attr_t)448/*PtyID*/, ac, az);
	fixc_add_tag(msg, (fixc_attr_t)447/*PtyIDSrc*/, "D", 1);
	fixc_add_tag(msg, (fixc_attr_t)452/*PtyIDRole*/, "27", 2);

	/* see if there's an instrm block */
	if (apfd_cache[idx].ins) {
		/* fixc_add_msg(msg, cache[idx].ins); */
		fixc_msg_t ins = apfd_cache[idx].ins;

		for (size_t i = 0; i < ins->nflds; i++) {
			struct fixc_fld_s fld = ins->flds[i];
			struct fixc_tag_data_s d = fixc_get_tag_data(ins, i);
			size_t mi = msg->nflds;

			fixc_add_tag(msg, (fixc_attr_t)fld.tag, d.s, d.z);
			/* bang .cnt and .tpc */
			msg->flds[mi].tpc = fld.tpc;
			msg->flds[mi].cnt = fld.cnt;
		}
	} else {
		/* have to mimick the instr somehow */
		static char buf[8];
		size_t mi;

		mi = msg->nflds;
		fixc_add_tag(msg, (fixc_attr_t)FIXML_ATTR_Symbol, sym, ssz);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 0;

		mi = msg->nflds;
		ssz = snprintf(buf, sizeof(buf), "%hu", idx);
		fixc_add_tag(msg, (fixc_attr_t)FIXML_ATTR_SecurityID, buf, ssz);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 1;

		mi = msg->nflds;
		fixc_add_tag(
			msg, (fixc_attr_t)FIXML_ATTR_SecurityIDSource,
			"100", 3);
		msg->flds[mi].tpc = FIXML_COMP_Instrument;
		msg->flds[mi].cnt = 2;
	}

	/* quantities */
	fixc_add_tag(msg, (fixc_attr_t)702/*NoPositions*/, "2", 1);
	fixc_add_tag(msg, (fixc_attr_t)703/*PosType*/, "TOT", 3);

	z = ffff_m62_s(p, (m62_t)l->w[0]);
	fixc_add_tag(msg, (fixc_attr_t)704/*LongQty*/, p, z);
	z = ffff_m62_s(p, (m62_t)s->w[0]);
	fixc_add_tag(msg, (fixc_attr_t)705/*ShortQty*/, p, z);

	/* also bang the more recent of long and short as QtyDt */
	fixc_add_tag(msg, (fixc_attr_t)976/*QtyDt*/, txn, txz);
	return;
}

static size_t
websvc_reqforposs(char **restrict tgt, size_t UNUSED(tsz), struct websvc_s sd)
{
	struct timeval now[1];
	size_t nsy = ute_nsyms(uctx);
	fixc_msg_t msg;
	struct fixc_rndr_s r;

	WEB_DEBUG("printing reqforposs ac %s\n", sd.reqforposs.ac);

	/* get current time */
	gettimeofday(now, NULL);

	/* start a fix msg for that */
	msg = make_fixc_msg((fixc_msgt_t)FIXC_MSGT_BATCH);

	/* loop over positions */
	for (size_t i = 1; i <= nsy; i++) {
		__posrpt1(msg, i, *now);
	}

	/* render the whole shebang */
	r = fixc_render_fixml_rndr(msg);
	free_fixc(msg);
	/* get ready for the harvest */
	*tgt = r.str;
	return r.len;
}

# else  /* !HAVE_LIBFIXC_FIX_H */
static size_t
websvc_reqforposs(char **restrict tgt, size_t tsz, struct websvc_s sd)
{
	return websvc_unk(tgt, tsz, sd);
}
# endif	/* HAVE_LIBFIXC_FIX_H */
#endif	/* WEB_ASP_REQFORPOSS */


static void
paste_clen(char *restrict buf, size_t len)
{
/* print ascii repr of LEN at BUF. */
	buf[0] = ' ';
	buf[1] = ' ';
	buf[2] = ' ';
	buf[3] = ' ';

	buf[4] = (len % 10U) + '0';
	if ((len /= 10U)) {
		buf[3] = (len % 10U) + '0';
		if ((len /= 10U)) {
			buf[2] = (len % 10U) + '0';
			if ((len /= 10U)) {
				buf[1] = (len % 10U) + '0';
				if ((len /= 10U)) {
					buf[0] = (len % 10U) + '0';
				}
			}
		}
	}
	return;
}


struct websvc_s
websvc(const char *req, size_t UNUSED(len))
{
	static const char get_slash[] = "GET /";
	struct websvc_s res = {WEBSVC_F_UNK};
	const char *p;

	if ((p = strstr(req, get_slash))) {
		p += sizeof(get_slash) - 1;

#define TAG_MATCHES_P(p, x)				\
		(strncmp(p, x, sizeof(x) - 1) == 0)

#define SECDEF_TAG	"secdef"
		if (TAG_MATCHES_P(p, SECDEF_TAG)) {
			WEB_DEBUG("secdef query\n");
			res.ty = WEBSVC_F_SECDEF;
			res.secdef.idx =
				__find_idx(p + sizeof(SECDEF_TAG) - 1);

#define QUOTREQ_TAG	"quotreq"
		} else if (TAG_MATCHES_P(p, QUOTREQ_TAG)) {
			WEB_DEBUG("quotreq query\n");
			res.ty = WEBSVC_F_QUOTREQ;
			res.quotreq.idx =
				__find_idx(p + sizeof(QUOTREQ_TAG) - 1);

#define REQFORPOSS_TAG	"reqforposs"
		} else if (TAG_MATCHES_P(p, REQFORPOSS_TAG)) {
			WEB_DEBUG("reqforposs query\n");
			res.ty = WEBSVC_F_REQFORPOSS;
			res.reqforposs.acz =
				__find_ac(
					&res.reqforposs.ac,
					p + sizeof(REQFORPOSS_TAG) - 1);
		}
	}
	return res;
}

struct webrsp_s
web(struct websvc_s ws)
{
	/* the final \n will be subst'd later on */
#define HDR		"\
HTTP/1.1 200 OK\r\n\
Server: um-quod\r\n\
Content-Length: "
#define CLEN_SPEC	"% 5zu"
#define BUF_INIT	HDR CLEN_SPEC "\r\n\r\n"
	/* hdr is a format string and hdr_len is as wide as the result printed
	 * later on */
#if defined HAVE_LIBFIXC_FIX_H
	static char __rsp[4096] = BUF_INIT;
#else  /* !HAVE_LIBFIXC_FIX_H */
	static char __rsp[65536] = BUF_INIT;
#endif	/* HAVE_LIBFIXC_FIX_H */
	char *rsp = __rsp + sizeof(BUF_INIT) - 1;
	const size_t rsp_len = sizeof(__rsp) - (sizeof(BUF_INIT) - 1);
	size_t cont_len;
	struct webrsp_s res;

	switch (ws.ty) {
	default:
	case WEBSVC_F_UNK:
		cont_len = websvc_unk(&rsp, rsp_len, ws);
		break;

	case WEBSVC_F_SECDEF:
#if defined WEB_ASP_SECDEF
		cont_len = websvc_secdef(&rsp, rsp_len, ws);
#else  /* !WEB_ASP_SECDEF */
		cont_len = websvc_unk(&rsp, rsp_len, ws);
#endif	/* WEB_ASP_SECDEF */
		break;
	case WEBSVC_F_QUOTREQ:
#if defined WEB_ASP_QUOTREQ
		cont_len = websvc_quotreq(&rsp, rsp_len, ws);
#else  /* !WEB_ASP_QUOTREQ */
		cont_len = websvc_unk(&rsp, rsp_len, ws);
#endif	/* WEB_ASP_QUOTREQ */
		break;
	case WEBSVC_F_REQFORPOSS:
#if defined WEB_ASP_REQFORPOSS
		cont_len = websvc_reqforposs(&rsp, rsp_len, ws);
#else  /* !WEB_ASP_REQFORPOSS */
		cont_len = websvc_unk(&rsp, rsp_len, ws);
#endif	/* WEB_ASP_REQFORPOSS */
		break;
	}

	/* prepare the header */
	paste_clen(__rsp + sizeof(HDR) - 1, cont_len);

	res.hdr = __rsp;
	res.hdz = sizeof(BUF_INIT) - 1;
	/* in the libfixc case this will be an exploded struct fixc_rndr_s */
	res.cnt = rsp;
	res.cnz = cont_len;
	return res;
}

#if defined HAVE_LIBFIXC_FIX_H
static inline void*
unconst(union {const void *c; void *p;} __attribute__((transparent_union)) p)
{
	return p.p;
}
#endif	/* HAVE_LIBFIXC_FIX_H */

void
free_webrsp(struct webrsp_s rsp)
{
#if defined HAVE_LIBFIXC_FIX_H
	if (rsp.hdr + rsp.hdz != rsp.cnt) {
		/* must come from a fixc alloc'ing renderer */
		fixc_free_rndr((struct fixc_rndr_s){unconst(rsp.cnt), rsp.cnz});
	}
#else  /* HAVE_LIBFIXC_FIX_H */
	(void)rsp;
#endif	/* HAVE_LIBFIXC_FIX_H */
	return;
}

/* web.c ends here */
