/*** gen-tws-cont.c -- generic tws c api contract builder
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
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#if defined HAVE_EXPAT_H
# include <expat.h>
#endif	/* HAVE_EXPAT_H */
#include "nifty.h"

#include "gen-tws-cont.h"
#include "gen-tws-cont-glu.h"

#include "proto-twsxml-tag.h"
#include "proto-twsxml-attr.h"
#include "proto-twsxml-reqtyp.h"
#include "proto-tx-ns.h"
#include "proto-fixml-tag.h"
#include "proto-fixml-attr.h"

#if defined DEBUG_FLAG
# define TX_DEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define TX_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct __ctx_s *__ctx_t;
typedef struct ptx_ns_s *ptx_ns_t;
typedef struct ptx_ctxcb_s *ptx_ctxcb_t;

typedef union tx_tid_u tx_tid_t;

typedef struct tws_xml_req_s *tws_xml_req_t;

union tx_tid_u {
	unsigned int u;
	tws_xml_tid_t tx;
	fixml_tid_t fix;
};

struct ptx_ns_s {
	char *pref;
	char *href;
	tx_nsid_t nsid;
};

/* contextual callbacks */
struct ptx_ctxcb_s {
	/* for a linked list */
	ptx_ctxcb_t next;

	/* navigation info, stores the context */
	tx_tid_t otype;
	tx_nsid_t nsid;
	union {
		void *object;
		long int objint;
	};
	ptx_ctxcb_t old_state;
};

struct __ctx_s {
	struct ptx_ns_s ns[16];
	size_t nns;
	/* stuff buf */
#define INITIAL_STUFF_BUF_SIZE	(4096)
	char *sbuf;
	size_t sbsz;
	size_t sbix;
	/* parser state, for contextual callbacks */
	ptx_ctxcb_t state;
	/* pool of context trackers, implies maximum parsing depth */
	struct ptx_ctxcb_s ctxcb_pool[16];
	ptx_ctxcb_t ctxcb_head;

	/* results will be built incrementally */
	int(*cont_cb)(tws_cont_t, void*);
	void *cbclo;
};


#if defined HAVE_GPERF
/* all the generated stuff */
#if defined __INTEL_COMPILER
# pragma warning (disable:869)
#elif defined __GNUC_STDC_INLINE__
# define HAD_STDC_INLINE
# undef __GNUC_STDC_INLINE__
#endif	/* __INTEL_COMPILER || __GNUC_STDC_INLINE__ */

#include "proto-twsxml-tag.c"
#include "proto-twsxml-attr.c"
#include "proto-tx-ns.c"
#include "proto-twsxml-reqtyp.c"

#include "proto-fixml-tag.c"
#include "proto-fixml-attr.c"

#if defined __INTEL_COMPILER
# pragma warning (default:869)
#elif defined HAD_STDC_INLINE
/* redefine the guy again */
# define __GNUC_STDC_INLINE__
#endif	/* __INTEL_COMPILER || __GNUC_STDC_INLINE__ */
#endif	/* HAVE_GPERF */


#if defined HAVE_EXPAT_H
static const char*
tag_massage(const char *tag)
{
/* return the real part of a (ns'd) tag or attribute,
 * i.e. foo:that_tag becomes that_tag */
	const char *p = strchr(tag, ':');

	if (p) {
		/* skip over ':' */
		return p + 1;
	}
	/* otherwise just return the tag as is */
	return tag;
}

static ptx_ns_t
__pref_to_ns(__ctx_t ctx, const char *pref, size_t pref_len)
{
	if (UNLIKELY(ctx->ns[0].nsid == TX_NS_UNK)) {
		/* bit of a hack innit? */
		return ctx->ns;

	} else if (LIKELY(pref_len == 0 && ctx->ns[0].pref == NULL)) {
		/* most common case when people use the default ns */
		return ctx->ns;
	}
	/* special service for us because we're lazy:
	 * you can pass pref = "foo:" and say pref_len is 4
	 * easier to deal with when strings are const etc. */
	if (pref[pref_len - 1] == ':') {
		pref_len--;
	}
	for (size_t i = (ctx->ns[0].pref == NULL); i < ctx->nns; i++) {
		if (strncmp(ctx->ns[i].pref, pref, pref_len) == 0) {
			return ctx->ns + i;
		}
	}
	return NULL;
}

#if defined HAVE_GPERF
static __attribute__((unused)) tws_xml_req_typ_t
parse_req_typ(const char *typ)
{
	const struct tws_xml_rtcell_s *rtc = __rtiddify(typ, strlen(typ));

	return rtc->rtid;
}

static tx_nsid_t
__tx_nsid_from_href(const char *href)
{
	size_t hlen = strlen(href);
	const struct tx_nsuri_s *n = __nsiddify(href, hlen);
	return n != NULL ? n->nsid : TX_NS_UNK;
}

static tws_xml_tid_t
sax_tx_tid_from_tag(const char *tag)
{
	size_t tlen = strlen(tag);
	const struct tws_xml_tag_s *t = __tiddify(tag, tlen);
	return t ? t->tid : TX_TAG_UNK;
}

static tws_xml_aid_t
sax_tx_aid_from_attr(const char *attr)
{
	size_t alen = strlen(attr);
	const struct tws_xml_attr_s *a = __aiddify(attr, alen);
	return a ? a->aid : TX_ATTR_UNK;
}

static tws_xml_aid_t
__tx_aid_from_attr_l(const char *attr, size_t len)
{
	const struct tws_xml_attr_s *a = __aiddify(attr, len);
	return a ? a->aid : TX_ATTR_UNK;
}

static fixml_tid_t
sax_fix_tid_from_tag(const char *tag)
{
	size_t tlen = strlen(tag);
	const struct fixml_tag_s *t = __fix_tiddify(tag, tlen);
	return t ? t->tid : FIX_TAG_UNK;
}

static fixml_aid_t
sax_fix_aid_from_attr(const char *attr)
{
	size_t alen = strlen(attr);
	const struct fixml_attr_s *a = __fix_aiddify(attr, alen);
	return a ? a->aid : FIX_ATTR_UNK;
}

static fixml_aid_t
__fix_aid_from_attr_l(const char *attr, size_t len)
{
	const struct fixml_attr_s *a = __fix_aiddify(attr, len);
	return a ? a->aid : FIX_ATTR_UNK;
}

#else  /* !HAVE_GPERF */
static __attribute__((unused)) tws_xml_req_typ_t
parse_req_typ(const char *UNUSED(typ))
{
	return TWS_XML_REQ_TYP_UNK;
}

static tx_nsid_t
__tx_nsid_from_href(const char *UNUSED(href))
{
	return TX_NS_UNK;
}

static tws_xml_tid_t
sax_tx_tid_from_tag(const char *UNUSED(tag))
{
	return TX_TAG_UNK;
}

static tws_xml_aid_t
sax_tx_aid_from_attr(const char *UNUSED(attr))
{
	return TX_ATTR_UNK;
}

static tws_xml_aid_t
__tx_aid_from_attr_l(const char *UNUSED(attr), size_t UNUSED(len))
{
	return TX_ATTR_UNK;
}

static fixml_tid_t
sax_fix_tid_from_tag(const char *UNUSED(tag))
{
	return FIX_TAG_UNK;
}

static fixml_aid_t
sax_fix_aid_from_attr(const char *UNUSED(attr))
{
	return FIX_ATTR_UNK;
}

static fixml_aid_t
__fix_aid_from_attr_l(const char *UNUSED(attr), size_t UNUSED(len))
{
	return FIX_ATTR_UNK;
}
#endif	/* HAVE_GPERF */

static void* __attribute__((unused))
get_state_object(__ctx_t ctx)
{
	return ctx->state->object;
}

static void __attribute__((unused))
set_state_object(__ctx_t ctx, void *z)
{
	ctx->state->object = z;
	return;
}

static void
init_ctxcb(__ctx_t ctx)
{
	memset(ctx->ctxcb_pool, 0, sizeof(ctx->ctxcb_pool));
	for (size_t i = 0; i < countof(ctx->ctxcb_pool) - 1; i++) {
		ctx->ctxcb_pool[i].next = ctx->ctxcb_pool + i + 1;
	}
	ctx->ctxcb_head = ctx->ctxcb_pool;
	return;
}

static ptx_ctxcb_t
pop_ctxcb(__ctx_t ctx)
{
	ptx_ctxcb_t res = ctx->ctxcb_head;

	if (LIKELY(res != NULL)) {
		ctx->ctxcb_head = res->next;
		memset(res, 0, sizeof(*res));
	}
	return res;
}

static void
push_ctxcb(__ctx_t ctx, ptx_ctxcb_t ctxcb)
{
	ctxcb->next = ctx->ctxcb_head;
	ctx->ctxcb_head = ctxcb;
	return;
}

static void*
pop_state(__ctx_t ctx)
{
/* restore the previous current state */
	ptx_ctxcb_t curr = ctx->state;
	void *obj = get_state_object(ctx);

	ctx->state = curr->old_state;
	/* queue him in our pool */
	push_ctxcb(ctx, curr);
	return obj;
}

static ptx_ctxcb_t
push_state(__ctx_t ctx, tx_nsid_t nsid, tx_tid_t otype, void *object)
{
	ptx_ctxcb_t res = pop_ctxcb(ctx);

	/* stuff it with the object we want to keep track of */
	res->object = object;
	res->nsid = nsid;
	res->otype = otype;
	/* fiddle with the states in our context */
	res->old_state = ctx->state;
	ctx->state = res;
	return res;
}

static void
ptx_init(__ctx_t ctx)
{
	/* initialise the ctxcb pool */
	init_ctxcb(ctx);
	return;
}

static void
ptx_reg_ns(__ctx_t ctx, const char *pref, const char *href)
{
	if (ctx->nns >= countof(ctx->ns)) {
		fputs("too many name spaces\n", stderr);
		return;
	}

	if (UNLIKELY(href == NULL)) {
		/* bollocks, user MUST be a twat */
		return;
	}

	/* get us those lovely ns ids */
	{
		const tx_nsid_t nsid = __tx_nsid_from_href(href);

		switch (nsid) {
			size_t i;
		case TX_NS_TWSXML_0_1:
		case TX_NS_FIXML_5_0:
		case TX_NS_FIXML_4_4:
			if (UNLIKELY(ctx->ns[0].href != NULL)) {
				i = ctx->nns++;
				ctx->ns[i] = ctx->ns[0];
			}
			/* oh, it's our fave, make it the naught-th one */
			ctx->ns[0].pref = (pref ? strdup(pref) : NULL);
			ctx->ns[0].href = strdup(href);
			ctx->ns[0].nsid = nsid;
			break;

		case TX_NS_UNK:
		default:
			i = ctx->nns++;
			ctx->ns[i].pref = pref ? strdup(pref) : NULL;
			ctx->ns[i].href = strdup(href);
			ctx->ns[i].nsid = nsid;
			break;
		}
	}
	return;
}

static bool
ptx_pref_p(__ctx_t ctx, const char *pref, size_t pref_len)
{
	/* we sorted our namespaces so that ptx is always at index 0 */
	if (UNLIKELY(ctx->ns[0].href == NULL)) {
		return false;

	} else if (LIKELY(ctx->ns[0].pref == NULL)) {
		/* prefix must not be set here either */
		return pref == NULL || pref_len == 0;

	} else if (UNLIKELY(pref_len == 0)) {
		/* this node's prefix is "" but we expect a prefix of
		 * length > 0 */
		return false;

	} else {
		/* special service for us because we're lazy:
		 * you can pass pref = "foo:" and say pref_len is 4
		 * easier to deal with when strings are const etc. */
		if (pref[pref_len - 1] == ':') {
			pref_len--;
		}
		return memcmp(pref, ctx->ns[0].pref, pref_len) == 0;
	}
}

static tws_xml_aid_t
check_tx_attr(__ctx_t ctx, const char *attr)
{
	const char *rattr = tag_massage(attr);
	const tws_xml_aid_t aid = sax_tx_aid_from_attr(rattr);

	if (!ptx_pref_p(ctx, attr, rattr - attr)) {
		/* dont know what to do */
		TX_DEBUG("unknown namespace %s\n", attr);
		return TX_ATTR_UNK;
	}
	return aid;
}

static fixml_aid_t
check_fix_attr(__ctx_t ctx, const char *attr)
{
	const char *rattr = tag_massage(attr);
	const fixml_aid_t aid = sax_fix_aid_from_attr(rattr);

	if (!ptx_pref_p(ctx, attr, rattr - attr)) {
		/* dont know what to do */
		TX_DEBUG("unknown namespace %s\n", attr);
		return FIX_ATTR_UNK;
	}
	return aid;
}


static void
proc_TX_xmlns(__ctx_t ctx, const char *pref, const char *value)
{
	TX_DEBUG("reg'ging name space %s <- %s\n", pref, value);
	ptx_reg_ns(ctx, pref, value);
	return;
}

static void
proc_UNK_attr(__ctx_t ctx, const char *attr, const char *value)
{
	const char *rattr = tag_massage(attr);
	tws_xml_aid_t aid;

	if (UNLIKELY(rattr > attr && !ptx_pref_p(ctx, attr, rattr - attr))) {
		aid = __tx_aid_from_attr_l(attr, rattr - attr - 1);
	} else {
		aid = sax_tx_aid_from_attr(rattr);
	}

	switch (aid) {
	case TX_ATTR_XMLNS:
		proc_TX_xmlns(ctx, rattr == attr ? NULL : rattr, value);
		break;
	default:
		break;
	}
	return;
}

static void
proc_TWSXML_attr(__ctx_t ctx, const char *attr, const char *value)
{
	const char *rattr = tag_massage(attr);
	tws_xml_aid_t aid;

	if (UNLIKELY(rattr > attr && !ptx_pref_p(ctx, attr, rattr - attr))) {
		aid = __tx_aid_from_attr_l(attr, rattr - attr - 1);
	} else {
		aid = sax_tx_aid_from_attr(rattr);
	}

	switch (aid) {
	case TX_ATTR_XMLNS:
		proc_TX_xmlns(ctx, rattr == attr ? NULL : rattr, value);
		break;
	default:
		TX_DEBUG("WARN: unknown attr %s\n", attr);
		break;
	}
	return;
}

static void
proc_FIXML_attr(__ctx_t ctx, const char *attr, const char *value)
{
	const char *rattr = tag_massage(attr);
	fixml_aid_t aid;

	if (UNLIKELY(rattr > attr && !ptx_pref_p(ctx, attr, rattr - attr))) {
		aid = __fix_aid_from_attr_l(attr, rattr - attr - 1);
	} else {
		aid = sax_fix_aid_from_attr(rattr);
	}

	switch (aid) {
	case FIX_ATTR_XMLNS:
		proc_TX_xmlns(ctx, rattr == attr ? NULL : rattr, value);
		break;
	case FIX_ATTR_S:
	case FIX_ATTR_R:
		/* we're so not interested in version mumbo jumbo */
		break;
	case FIX_ATTR_V:
		break;
	default:
		TX_DEBUG("WARN: unknown attr %s\n", attr);
		break;
	}
	return;
}

static void
proc_REQCONTRACT_attr(
	tws_cont_t ins, tx_nsid_t ns, tws_xml_aid_t aid, const char *value)
{
	tws_cont_x(ins, ns, aid, value);
	return;
}

static void
proc_INSTRMT_attr(tws_cont_t ins, tx_nsid_t ns, fixml_aid_t aid, const char *v)
{
	tws_cont_x(ins, ns, aid, v);
	return;
}

static void
proc_SECDEF_attr(tws_cont_t ins, tx_nsid_t ns, fixml_aid_t aid, const char *v)
{
	tws_cont_x(ins, ns, aid, v);
	return;
}


static void
sax_bo_TWSXML_elt(__ctx_t ctx, const char *elem, const char **attr)
{
	const tws_xml_tid_t tid = sax_tx_tid_from_tag(elem);

	/* all the stuff that needs a new sax handler */
	switch (tid) {
	case TX_TAG_TWSXML:
		ptx_init(ctx);

		if (UNLIKELY(attr == NULL)) {
			break;
		}

		for (const char **ap = attr; ap && *ap; ap += 2) {
			proc_TWSXML_attr(ctx, ap[0], ap[1]);
		}
		break;

	case TX_TAG_REQUEST:
		push_state(ctx, TX_NS_TWSXML_0_1, (tx_tid_t)tid, NULL);
		break;

	case TX_TAG_QUERY:
	case TX_TAG_RESPONSE:
		push_state(ctx, TX_NS_TWSXML_0_1, (tx_tid_t)tid, NULL);
		break;

	case TX_TAG_REQCONTRACT: {
		tws_cont_t ins = tws_make_cont();

		/* get all them contract specs */
		for (const char **ap = attr; ap && *ap; ap += 2) {
			const tws_xml_aid_t aid = check_tx_attr(ctx, ap[0]);

			proc_REQCONTRACT_attr(
				ins, TX_NS_TWSXML_0_1, aid, ap[1]);
		}
		push_state(ctx, TX_NS_TWSXML_0_1, (tx_tid_t)tid, ins);
		break;
	}

	default:
		break;
	}
	return;
}

static void
sax_eo_TWSXML_elt(__ctx_t ctx, const char *elem)
{
	tws_xml_tid_t tid = sax_tx_tid_from_tag(elem);

	/* stuff that needed to be done, fix up state etc. */
	switch (tid) {
		/* top-levels */
	case TX_TAG_TWSXML:
		break;

	case TX_TAG_REQUEST:
	case TX_TAG_QUERY:
	case TX_TAG_RESPONSE:
		(void)pop_state(ctx);
		break;

		/* non top-levels without children */
	case TX_TAG_REQCONTRACT: {
		tws_cont_t ins = pop_state(ctx);

		if (UNLIKELY(ins == NULL)) {
			TX_DEBUG("internal parser error, cont is NULL\n");
			break;
		} else if (ctx->cont_cb == NULL ||
			   ctx->cont_cb(ins, ctx->cbclo) < 0) {
			tws_free_cont(ins);
		}
		break;
	}

	default:
		break;
	}
	return;
}

static void
sax_bo_FIXML_elt(__ctx_t ctx, const char *elem, const char **attr)
{
	const fixml_tid_t tid = sax_fix_tid_from_tag(elem);

	/* all the stuff that needs a new sax handler */
	switch (tid) {
	case FIX_TAG_FIXML:
		ptx_init(ctx);

		if (UNLIKELY(attr == NULL)) {
			break;
		}

		for (const char **ap = attr; ap && *ap; ap += 2) {
			proc_FIXML_attr(ctx, ap[0], ap[1]);
		}
		break;

	case FIX_TAG_BATCH:
		break;

	case FIX_TAG_SECDEF: {
		tws_cont_t ins = tws_make_cont();

		for (const char **ap = attr; ap && *ap; ap += 2) {
			const fixml_aid_t aid = check_fix_attr(ctx, ap[0]);

			proc_SECDEF_attr(ins, TX_NS_FIXML_5_0, aid, ap[1]);
		}
		push_state(ctx, TX_NS_FIXML_5_0, (tx_tid_t)tid, ins);
		break;
	}

	case FIX_TAG_INSTRMT: {
		tws_cont_t ins = get_state_object(ctx);

		for (const char **ap = attr; ap && *ap; ap += 2) {
			const fixml_aid_t aid = check_fix_attr(ctx, ap[0]);

			proc_INSTRMT_attr(ins, TX_NS_FIXML_5_0, aid, ap[1]);
		}
		push_state(ctx, TX_NS_FIXML_5_0, (tx_tid_t)tid, ins);
		break;
	}

	default:
		break;
	}
	return;
}

static void
sax_eo_FIXML_elt(__ctx_t ctx, const char *elem)
{
	fixml_tid_t tid = sax_fix_tid_from_tag(elem);

	/* stuff that needed to be done, fix up state etc. */
	switch (tid) {
		/* top-levels */
	case FIX_TAG_FIXML:
	case FIX_TAG_BATCH:
		break;

	case FIX_TAG_SECDEF: {
		tws_cont_t ins = pop_state(ctx);

		if (UNLIKELY(ins == NULL)) {
			TX_DEBUG("internal parser error, cont is NULL\n");
			break;
		} else if (ctx->cont_cb == NULL ||
			   ctx->cont_cb(ins, ctx->cbclo) < 0) {
			tws_free_cont(ins);
		}
		break;
	}

	case FIX_TAG_INSTRMT:
		pop_state(ctx);
		break;

	default:
		break;
	}
	return;
}


static void
el_sta(void *clo, const char *elem, const char **attr)
{
	__ctx_t ctx = clo;
	/* where the real element name starts, sans ns prefix */
	const char *relem = tag_massage(elem);
	ptx_ns_t ns = __pref_to_ns(ctx, elem, relem - elem);

	if (UNLIKELY(ns == NULL)) {
		TX_DEBUG("unknown prefix in tag %s\n", elem);
		return;
	}

retry:
	switch (ns->nsid) {
	case TX_NS_TWSXML_0_1:
		sax_bo_TWSXML_elt(ctx, relem, attr);
		break;

	case TX_NS_FIXML_4_4:
	case TX_NS_FIXML_5_0:
		sax_bo_FIXML_elt(ctx, relem, attr);
		break;

	case TX_NS_UNK:
		for (const char **ap = attr; ap && *ap; ap += 2) {
			proc_UNK_attr(ctx, ap[0], ap[1]);
		}
		ns = ctx->ns;
		goto retry;

	default:
		TX_DEBUG("unknown namespace %s (%s)\n", elem, ns->href);
		break;
	}
	return;
}

static void
el_end(void *clo, const char *elem)
{
	__ctx_t ctx = clo;
	/* where the real element name starts, sans ns prefix */
	const char *relem = tag_massage(elem);
	ptx_ns_t ns = __pref_to_ns(ctx, elem, relem - elem);

	switch (ns->nsid) {
	case TX_NS_TWSXML_0_1:
		sax_eo_TWSXML_elt(ctx, relem);
		break;

	case TX_NS_FIXML_4_4:
	case TX_NS_FIXML_5_0:
		sax_eo_FIXML_elt(ctx, relem);
		break;

	case TX_NS_UNK:
	default:
		TX_DEBUG("unknown namespace %s (%s)\n", elem, ns->href);
		break;
	}
	return;
}
#endif	/* HAVE_EXPAT_H */


/* public funs */
#if defined HAVE_EXPAT_H
static int
priv_cont_cb(tws_cont_t c, void *clo)
{
	tws_cont_t *res = clo;

	if (*res) {
		/* great */
		tws_free_cont(*res);
	}
	*res = c;
	return 0;
}

tws_cont_t
tws_cont(const char *xml, size_t len)
{
	struct __ctx_s clo = {0};
	XML_Parser hdl;
	tws_cont_t res = NULL;

	if ((hdl = XML_ParserCreate(NULL)) == NULL) {
		return NULL;
	}
	/* register the callback */
	clo.cont_cb = priv_cont_cb;
	clo.cbclo = &res;

	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, &clo);

	if (XML_Parse(hdl, xml, len, XML_TRUE) == XML_STATUS_ERROR) {
		return NULL;
	}

	/* get rid of resources */
	XML_ParserFree(hdl);
	return res;
}

int
tws_batch_cont(
	const char *xml, size_t len,
	int(*cb)(tws_cont_t, void*), void *cbclo)
{
	XML_Parser hdl;
	struct __ctx_s clo = {0};

	if ((hdl = XML_ParserCreate(NULL)) == NULL) {
		return -1;
	}
	/* register our callback */
	clo.cont_cb = cb;
	clo.cbclo = cbclo;

	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, &clo);

	if (XML_Parse(hdl, xml, len, XML_TRUE) == XML_STATUS_ERROR) {
		return -1;
	}

	/* get rid of resources */
	XML_ParserFree(hdl);
	return 0;
}

#else  /* HAVE_EXPAT_H */
tws_cont_t
tws_cont(const char *UNUSED(xml), size_t UNUSED(len))
{
	return NULL;
}

int
tws_batch_cont(
	const char *UNUSED(xml), size_t UNUSED(len),
	UNUSED(int(*cb)(tws_cont_t, void*)), void *UNUSED(cbclo))
{
	return -1;
}
#endif	/* HAVE_EXPAT_H */


/* serialising to XML or FIX strings */
static ssize_t
__sdef_xml(char *restrict buf, size_t bsz, ssize_t(*cb)(), const void *data)
{
	static char hdr[] = "\
<?xml version=\"1.0\"?>\n\
<FIXML xmlns=\"http://www.fixprotocol.org/FIXML-5-0-SP2\"/>\
";
	static char ftr[] = "\
</FIXML>\n\
";
	char *restrict p;

	if (bsz < sizeof(hdr)) {
		/* completely fucked */
		return -1;
	}

	/* always start out with the hdr,
	 * which for efficiency contains the empty case already */
	strncpy(p = buf, hdr, bsz);

	if (data == NULL) {
		/* this is convenience for lazy definitions in the higher
		 * level, undocumented though */
		return sizeof(hdr) - 1;
	}

	/* modify the contents so far */
	p[sizeof(hdr) - 3] = '>';
	p[sizeof(hdr) - 2] = '\n';
	/* 1 for the / we discarded, one for the \0 */
	p += sizeof(hdr) - 1 - 1;

	/* it's just one contract, yay
	 * we quickly give an estimate of the space left
	 * we used to count in the space we need for the footer,
	 * but that would give us a headache when we switch to incremental
	 * string building. */
	{
		size_t spc_left = bsz - (p - buf);
		ssize_t tmp;

		if ((tmp = cb(p, spc_left, TX_NS_FIXML_5_0, data)) < 0) {
			/* grrrr */
			return -1;
		}
		/* otherwise */
		p += tmp;
	}

	/* and the footer now */
	if (p + sizeof(ftr) < buf + bsz) {
		memcpy(p, ftr, sizeof(ftr));
		p += sizeof(ftr) - 1;
	}
	return p - buf;
}

ssize_t
tws_cont_xml(char *restrict buf, size_t bsz, tws_const_cont_t c)
{
	return __sdef_xml(buf, bsz, tws_cont_y, c);
}

/* gen-tws-cont.c ends here */
