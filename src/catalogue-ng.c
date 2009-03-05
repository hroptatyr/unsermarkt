/*** catalogue-ng.c -- new generation catalogue
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "catalogue-ng.h"
#include "protocore.h"
#include "catalogue.h"
/* other external stuff */
#include <pfack/instruments.h>
#include <ffff/hashtable.h>
#include <ffff/ratio.h>

extern void *instruments;

/* ctor, dtor */
catng_t
make_catalogue(void)
{
	struct ase_dict_options_s opts = {
		.initial_size = 64,
		.worst_case_constant_lookup_p = true,
		.two_power_sizes = true,
		.arity = 4,
	};
	return (void*)ase_make_htable(&opts);
}

void
free_catalogue(catng_t cat)
{
	ase_free_htable(cat);
	return;
}

/* modifiers */
void
catalogue_add_instr(catng_t cat, const instr_t instr)
{
	unsigned int cod = instr_general_group(instr)->ga_id;
	void *key = (void*)(long unsigned int)cod;
	ase_htable_put(cat, cod, key/*val-only?*/, instr);
	return;
}


/* helpers */
static inline uint8_t
ud_tlv_size(ud_tlv_t tlv)
{
	switch (tlv->tag) {
	case UD_TAG_CLASS:
	case UD_TAG_ATTR:
	case UD_TAG_NAME:

	case UD_TAG_GROUP0_NAME:
		return tlv->data[0];

	case UD_TAG_GROUP0_CFI:
		return sizeof(pfack_10962_t);

	case UD_TAG_GROUP0_OPOL:
		return sizeof(pfack_10383_t);

	case UD_TAG_GROUP0_GAID:
		return sizeof(unsigned int);

	case UD_TAG_PADDR:
	case UD_TAG_UNDERLYING:
		return sizeof(void*);

	case UD_TAG_UNK:
	default:
		return 0;
	}
}

static inline signed char __attribute__((always_inline, gnu_inline))
tlv_cmp_f(const ud_tlv_t t1, const ud_tlv_t t2)
{
/* returns -1 if t1 < t2, 0 if t1 == t2 and 1 if t1 > t2 */
#if 1
	if (t1->tag < t2->tag) {
		return -1;
	} else if (t1->tag == t2->tag) {
		return 0;
	} else /*if (t1->tag > t2->tag)*/ {
		return 1;
	}
#else
	uint8_t t1s = ud_tlv_size(t1);
	uint8_t t2s = ud_tlv_size(t2);
	uint8_t sz = t1s < t2s ? t1s : t2s;
	return memcmp((const char*)t1, (const char*)t2, sz);
#endif
}

static inline unsigned int
ud_write_instr_uid(char *restrict buf, ud_tag_t t, instr_uid_t uid)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&uid, sizeof(instr_uid_t));
	return 2 + sizeof(uid);
}

static inline unsigned int
ud_write_date_dse(char *restrict buf, ud_tag_t t, ffff_date_dse_t d)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&d, sizeof(d));
	return sizeof(d) + 2;
}

static inline unsigned int
ud_write_monetary32(char *restrict buf, ud_tag_t t, monetary32_t m)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&m, sizeof(m));
	return sizeof(m) + 2;
}

static inline unsigned int
ud_write_short(char *restrict buf, ud_tag_t t, short int s)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&s, sizeof(s));
	return sizeof(s) + 2;
}

static inline unsigned int
ud_write_ratio(char *restrict buf, ud_tag_t t, ratio16_t s)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&s, sizeof(s));
	return sizeof(s) + 2;
}

static inline unsigned int
ud_write_uint32(char *restrict buf, ud_tag_t t, uint32_t i)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = t;
	memcpy(buf + 2, (char*)&i, sizeof(i));
	return sizeof(i) + 2;
}


static unsigned int
ud_write_g0_name(char *restrict buf, const void *grp)
{
	size_t len = strlen(general_name(grp));
	if (UNLIKELY(len == 0)) {
		return 0;
	}
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_NAME;
	buf[2] = (uint8_t)len;
	memcpy(buf + 3, general_name(grp), len);
	return len + 3;
}

static unsigned int
ud_write_g0_cfi(char *restrict buf, const void *grp)
{
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_CFI;
	memcpy(buf + 2, general_cfi(grp), sizeof(pfack_10962_t));
	return sizeof(pfack_10962_t) + 2;
}

static unsigned int
ud_write_g0_opol(char *restrict buf, const void *grp)
{
	/* otherwise */
	buf[0] = UDPC_TYPE_KEYVAL;
	buf[1] = UD_TAG_GROUP0_OPOL;
	memcpy(buf + 2, general_opol(grp), sizeof(pfack_10383_t));
	return sizeof(pfack_10383_t) + 2;
}

static unsigned int
ud_write_g0_gaid(char *restrict buf, const void *grp)
{
	return ud_write_instr_uid(buf, UD_TAG_GROUP0_GAID, general_ga_id(grp));
}

static unsigned int
ud_write_g2_fund_instr(char *restrict buf, const void *grp)
{
	return ud_write_instr_uid(
		buf, UD_TAG_GROUP2_FUND_INSTR, funding_fund_instr(grp));
}

static unsigned int
ud_write_g2_setd_instr(char *restrict buf, const void *grp)
{
	return ud_write_instr_uid(
		buf, UD_TAG_GROUP2_SETD_INSTR, funding_setd_instr(grp));
}

static unsigned int
ud_write_g3_issue(char *restrict buf, const void *grp)
{
	return ud_write_date_dse(buf, UD_TAG_GROUP3_ISSUE, delivery_issue(grp));
}

static unsigned int
ud_write_g3_expiry(char *restrict buf, const void *grp)
{
	return ud_write_date_dse(
		buf, UD_TAG_GROUP3_EXPIRY, delivery_expiry(grp));
}

static unsigned int
ud_write_g3_settle(char *restrict buf, const void *grp)
{
	return ud_write_date_dse(
		buf, UD_TAG_GROUP3_SETTLE, delivery_settle(grp));
}

static unsigned int
ud_write_g4_underlyer(char *restrict buf, const void *grp)
{
	return ud_write_instr_uid(
		buf, UD_TAG_GROUP4_UNDERLYER, referent_underlyer(grp));
}

static unsigned int
ud_write_g4_strike(char *restrict buf, const void *grp)
{
	return ud_write_monetary32(
		buf, UD_TAG_GROUP4_STRIKE, referent_strike(grp));
}

static unsigned int
ud_write_g4_ratio(char *restrict buf, const void *grp)
{
	return ud_write_ratio(buf, UD_TAG_GROUP4_RATIO, referent_ratio(grp));
}

static unsigned int
ud_write_g5_barrier(char *restrict buf, uint8_t idx, const void *grp)
{
	return ud_write_uint32(
		buf, UD_TAG_GROUP5_BARRIER, barrier_barrier(grp, idx));
}


/* unserding serialiser */
static unsigned int
serialise_catobj(char *restrict buf, const_instr_t instr)
{
	unsigned int idx;
	short unsigned int grpset = instr->grps;
	char *p;
	const void *tmp;

	/* we are a UDPC_TYPE_CATOBJ */
	buf[0] = (udpc_type_t)UDPC_TYPE_PFINSTR;
	/* write the group set */
	buf[1] = (udpc_type_t)UDPC_TYPE_WORD;
	idx = 2;
	p = (char*)&grpset;
	buf[idx++] = *p++;
	buf[idx++] = *p++;

	/* encode the groups now */
	if ((tmp = instr_general_group(instr)) != NULL) {
		/* write group 0, general group */
		/* :name, :cfi, :opol, :gaid */
		idx += ud_write_g0_name(&buf[idx], tmp);
		idx += ud_write_g0_cfi(&buf[idx], tmp);
		idx += ud_write_g0_opol(&buf[idx], tmp);
		idx += ud_write_g0_gaid(&buf[idx], tmp);
	}
	if ((tmp = instr_funding_group(instr)) != NULL) {
		/* write group 2, funding group */
		/* :fund-instr, :set-instr */
		idx += ud_write_g2_fund_instr(&buf[idx], tmp);
		idx += ud_write_g2_setd_instr(&buf[idx], tmp);
	}
	if ((tmp = instr_delivery_group(instr)) != NULL) {
		/* write group 3, delivery group */
		/* :start, :expiry, :settle */
		idx += ud_write_g3_issue(&buf[idx], tmp);
		idx += ud_write_g3_expiry(&buf[idx], tmp);
		idx += ud_write_g3_settle(&buf[idx], tmp);
	}
	if ((tmp = instr_referent_group(instr)) != NULL) {
		/* write group 4, referent group */
		/* :underlyer, :strike, :ratio */
		idx += ud_write_g4_underlyer(&buf[idx], tmp);
		idx += ud_write_g4_strike(&buf[idx], tmp);
		idx += ud_write_g4_ratio(&buf[idx], tmp);
	}
	if ((tmp = instr_barrier_group(instr)) != NULL) {
		/* write group 5, barrier group */
		/* 4 kikos */
		idx += ud_write_g5_barrier(&buf[idx], 0, tmp);
		idx += ud_write_g5_barrier(&buf[idx], 1, tmp);
		idx += ud_write_g5_barrier(&buf[idx], 2, tmp);
		idx += ud_write_g5_barrier(&buf[idx], 3, tmp);
	}
	return idx;
}


static unsigned int
sort_params(ud_tlv_t *tlvs, char *restrict wrkspc, job_t j)
{
	uint8_t idx = j->buf[9];

	if (UNLIKELY(j->buf[8] != UDPC_TYPE_SEQOF)) {
		return 0;
	} else if (UNLIKELY(idx == 0)) {
		return 0;
	} else if (idx == 1) {
		ud_tlv_t tlv = (ud_tlv_t)&j->buf[10];
		memcpy(wrkspc, tlv, 1 + ud_tlv_size(tlv));
		tlvs[0] = (ud_tlv_t)wrkspc;
		return 1;
	}

#if 1
	/* copy everything */
	memcpy(wrkspc, &j->buf[10], j->blen - 10);
	idx = 0;
	for (uint8_t p = 0; p < j->blen - 10; ) {
		ud_tlv_t tlv = (ud_tlv_t)&wrkspc[p];
		tlvs[idx++] = tlv;
		p += sizeof(tlv->tag) + ud_tlv_size(tlv);
	}

#else
/* bullshit in this mode */
	/* do a primitive slection sort, do me properly! */
	/* we traverse the list once to find the minimum element */
	tmin = (ud_tlv_t)&j->buf[10];

	for (ud_tlv_t t = (ud_tlv_t)
		     ((char*)tmin + 2 + ud_tlv_size(tmin));
	     (char*)t < j->buf + j->blen;
	     t = (ud_tlv_t)((char*)t + 2 + ud_tlv_size(t))) {

		if (tlv_cmp_f(tmin, t) > 0) {
			tmin = t;
		}
	}
	/* copy the stuff over to the work space */
	tlvs[0] = (ud_tlv_t)wrkspc;
	memcpy(wrkspc, tmin, sz = 2 + ud_tlv_size(tmin));
	wrkspc += sz;

	/* using this as new maximum */
	last = tmin;

	for (uint8_t k = 1; k < idx; k++) {
		/* just choose a tmin */
		tmin = (ud_tlv_t)&j->buf[10];

		for (ud_tlv_t t = (ud_tlv_t)
			     ((char*)tmin + 2 + ud_tlv_size(tmin));
		     (char*)t < j->buf + j->blen;
		     t = (ud_tlv_t)((char*)t + 2 + ud_tlv_size(t))) {

			if (tlv_cmp_f(tmin, last) <= 0) {
				tmin = t;
				continue;
			}
			if (tlv_cmp_f(t, last) <= 0) {
				continue;
			}
			if (tlv_cmp_f(tmin, t) > 0) {
				tmin = t;
			}
		}
		tlvs[k] = (ud_tlv_t)wrkspc;
		memcpy(wrkspc, tmin, sz = 2 + ud_tlv_size(tmin));
		wrkspc += sz;

		/* using this as new maximum */
		last = tmin;
	}
#endif
	return idx;
}

static inline bool
catobj_filter_one(const_instr_t instr, ud_tlv_t tlv)
{
	if (UNLIKELY(tlv->tag < UD_TAG_INSTRFILT_FIRST ||
		     tlv->tag > UD_TAG_INSTRFILT_LAST)) {
		return false;
	}
	/* switch by tags now */
	switch ((uint8_t)tlv->tag) {
	case UD_TAG_GROUP0_NAME: {
		/* allows substring search automagically */
		size_t len = (unsigned char)tlv->data[0];
		const char *name = instr_general_name(instr);
		return memcmp(tlv->data + 1, name, len) == 0;
	}

	case UD_TAG_GROUP0_CFI: {
		const char *cfi = instr_general_cfi(instr);
		return pfack_10962_isap(tlv->data, cfi);
	}

	case UD_TAG_GROUP0_OPOL: {
		const char *opol = instr_general_opol(instr);
		return pfack_10383_eqp(tlv->data, opol);
	}

	case UD_TAG_GROUP0_GAID: {
		instr_id_t tmp = *(const instr_id_t*const)tlv->data;
		if (instr_general_ga_id(instr) == tmp) {
			return true;
		}
		break;
	}

	default:
		break;
	}
	return false;
}

static bool
catobj_filter(const_instr_t instr, ud_tlv_t *sub, uint8_t slen)
{
	/* traverse all the filter properties */
	for (uint8_t j = 0; j < slen; j++) {
		if (!catobj_filter_one(instr, sub[j])) {
			return false;
		}
	}
	return true;
}

/* another browser */
extern bool ud_cat_lc_job(job_t j);
bool
ud_cat_lc_job(job_t j)
{
	unsigned int idx = 10;
	unsigned int slen = 0;
	char tmp[UDPC_SIMPLE_PKTLEN];
	ud_tlv_t sub[8];
	struct ase_dict_iter_s iter;
	const void *key;
	void *val;
	uint8_t ninstrs = 0;

	UD_DEBUG_CAT("lc job\n");
	/* filter what the luser sent us */
	slen = sort_params(sub, tmp, j);
	/* we are a seqof(UDPC_TYPE_CATOBJ) */
	j->buf[8] = (udpc_type_t)UDPC_TYPE_SEQOF;

	/* we should have a query planner here to determine whether
	 * or not a seqscan is feasible or if there's a more direct way
	 * to access the bugger */

	ht_iter_init_ll(instruments, &iter);
	while (ht_iter_next(&iter, &key, &val)) {
		const_instr_t instr = val;
		/* only include matching instruments */
		if (catobj_filter(instr, sub, slen)) {
			idx += serialise_catobj(&j->buf[idx], instr);
			ninstrs++;
		}
	}
	ht_iter_fini_ll(&iter);

	/* the number of instruments output */
	j->buf[9] = ninstrs;
	/* the total length of the packet
	 * CHECKME prone to overflows */
	j->blen = idx;
	return false;
}

/* catalogue-ng.c ends here */
