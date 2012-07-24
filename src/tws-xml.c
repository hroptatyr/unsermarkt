/*** tws-xml.c -- conversion between IB/API structs and xml
 *
 * Copyright (C) 2011-2012 Ruediger Meier
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
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <expat.h>
#include "prchunk.h"
#include "tws-xml.h"

/* gperf goodness */
#include "proto-twsxml-tag.c"
#include "proto-twsxml-attr.c"
#include "proto-twsxml-ns.c"

#if !defined NDEBUG
# define TX_DEBUG(args...)	fprintf(stderr, args)
#else  /* NDEBUG */
# define TX_DEBUG(args...)
#endif	/* !NDEBUG */

#if !defined UNLIKELY
# define UNLIKELY(x)		__builtin_expect(!!(x), 0)
#endif	/* !UNLIKELY */
#if !defined LIKELY
# define LIKELY(x)		__builtin_expect(!!(x), 1)
#endif	/* !LIKELY */
#if !defined countof
# define countof(x)		(sizeof(x) / sizeof(*x))
#endif	/* !countof */

typedef struct __ctx_s *__ctx_t;
typedef struct ptx_ns_s *ptx_ns_t;
typedef struct ptx_ctxcb_s *ptx_ctxcb_t;

struct ptx_ns_s {
	char *pref;
	char *href;
	tws_xml_nsid_t nsid;
};

/* contextual callbacks */
struct ptx_ctxcb_s {
	/* for a linked list */
	ptx_ctxcb_t next;

	/* navigation info, stores the context */
	tws_xml_tid_t otype;
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

	void(*cb)(tws_xml_req_t, void*);
	void *clo;
	/* the request we're building */
	struct tws_xml_req_s req[1];
};


static tws_xml_req_typ_t
parse_req_typ(const char *typ)
{
	if (!strcmp(typ, "market_data")) {
		return TWS_XML_REQ_TYP_MKT_DATA;
	}
	return TWS_XML_REQ_TYP_UNK;
}

static tws_cont_t
el_req_con(void *clo, const char **attr)
{
/* this is actually glue code between tws-cont and tws-xml */
	tws_cont_t x = make_cont();

	TX_DEBUG("building %p\n", x);
	for (const char **p = attr; p && *p; p += 2) {
		tws_cont_build(x, p[0], p[1]);
	}
	return x;
}

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
		ctx->ns->nsid = TX_NS_TWSXML_0_1;
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

static tws_xml_tid_t
sax_tid_from_tag(const char *tag)
{
	size_t tlen = strlen(tag);
	const struct tws_xml_tag_s *t = __tiddify(tag, tlen);
	return t ? t->tid : TX_TAG_UNK;
}

static tws_xml_aid_t
sax_aid_from_attr(const char *attr)
{
	size_t alen = strlen(attr);
	const struct tws_xml_attr_s *a = __aiddify(attr, alen);
	return a ? a->aid : TX_ATTR_UNK;
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

static void
pop_state(__ctx_t ctx)
{
/* restore the previous current state */
	ptx_ctxcb_t curr = ctx->state;

	ctx->state = curr->old_state;
	/* queue him in our pool */
	push_ctxcb(ctx, curr);
	return;
}

static ptx_ctxcb_t
push_state(__ctx_t ctx, tws_xml_tid_t otype, void *object)
{
	ptx_ctxcb_t res = pop_ctxcb(ctx);

	/* stuff it with the object we want to keep track of */
	res->object = object;
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
		size_t ulen = strlen(href);
		const struct tws_xml_nsuri_s *n = __nsiddify(href, ulen);
		const tws_xml_nsid_t nsid = n ? n->nsid : TX_NS_UNK;

		switch (nsid) {
			size_t i;
		case TX_NS_TWSXML_0_1:
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
check_attr(__ctx_t ctx, const char *attr)
{
	const char *rattr = tag_massage(attr);
	const tws_xml_aid_t aid = sax_aid_from_attr(rattr);

	if (!ptx_pref_p(ctx, attr, rattr - attr)) {
		/* dont know what to do */
		TX_DEBUG("unknown namespace %s\n", attr);
		return TX_ATTR_UNK;
	}
	return aid;
}


static void
proc_TWSXML_xmlns(__ctx_t ctx, const char *pref, const char *value)
{
	TX_DEBUG("reg'ging name space %s <- %s\n", pref, value);
	ptx_reg_ns(ctx, pref, value);
	return;
}

static void
proc_TWSXML_attr(__ctx_t ctx, const char *attr, const char *value)
{
	const char *rattr = tag_massage(attr);
	tws_xml_aid_t aid;

	if (UNLIKELY(rattr > attr && !ptx_pref_p(ctx, attr, rattr - attr))) {
		const struct tws_xml_attr_s *a =
			__aiddify(attr, rattr - attr - 1);
		aid = a ? a->aid : TX_ATTR_UNK;
	} else {
		aid = sax_aid_from_attr(rattr);
	}

	switch (aid) {
	case TX_ATTR_XMLNS:
		proc_TWSXML_xmlns(ctx, rattr == attr ? NULL : rattr, value);
		break;
	default:
		TX_DEBUG("WARN: unknown attr %s\n", attr);
		break;
	}
	return;
}

static void
sax_bo_TWSXML_elt(__ctx_t ctx, const char *elem, const char **attr)
{
	const tws_xml_tid_t tid = sax_tid_from_tag(elem);

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
		/* obtain the type */
		for (const char **ap = attr; ap && *ap; ap += 2) {
			const tws_xml_aid_t aid = check_attr(ctx, ap[0]);

			switch (aid) {
			case TX_ATTR_TYPE:
				ctx->req->typ = parse_req_typ(ap[1]);
			default:
				break;
			}
		}
	case TX_TAG_QUERY:
	case TX_TAG_RESPONSE:
		push_state(ctx, tid, NULL);
		break;

	case TX_TAG_REQCONTRACT:
		break;

	default:
		break;
	}
	return;
}

static void
sax_eo_TWSXML_elt(__ctx_t ctx, const char *elem)
{
	tws_xml_tid_t tid = sax_tid_from_tag(elem);

	/* stuff that needed to be done, fix up state etc. */
	switch (tid) {
		/* top-levels */
	case TX_TAG_TWSXML:
		break;
	case TX_TAG_REQUEST:
		TX_DEBUG("/req %p\n", ctx->req);
		if (ctx->cb) {
			/* callback */
			ctx->cb(ctx->req, ctx->clo);
		}
	case TX_TAG_QUERY:
	case TX_TAG_RESPONSE:
		(void)pop_state(ctx);
		break;

		/* non top-levels without children */
	case TX_TAG_REQCONTRACT:
		/* noone dare push this */
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

	switch (ns->nsid) {
	case TX_NS_TWSXML_0_1:
		sax_bo_TWSXML_elt(ctx, relem, attr);
		break;

	case TX_NS_UNK:
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

	case TX_NS_UNK:
	default:
		TX_DEBUG("unknown namespace %s (%s)\n", elem, ns->href);
		break;
	}
	return;
}

static int
proc_line(XML_Parser hdl, const char *line, size_t llen)
{
	struct xml_clo_s *clo = XML_GetUserData(hdl);

	if (XML_Parse(hdl, line, llen, XML_FALSE) == XML_STATUS_ERROR) {
		return -1;
#if 0
	} else if (clo->level < 0) {
		XML_StopParser(hdl, XML_TRUE);
#endif
	}
	return 0;
}


int
tws_xml_parse(const char *fn, void(*cb)(tws_xml_req_t, void*), void *cb_clo)
{
	XML_Parser hdl;
	void *pctx;
	int fd;
	struct __ctx_s clo = {
		.cb = cb,
		.clo = cb_clo,
	};
	int res = 0;

	if (fn == NULL) {
		fd = STDIN_FILENO;
	} else if ((fd = open(fn, O_RDONLY)) < 0) {
		res = -1;
		goto super_fucked;
	}

	/* using the prchunk reader now */
	if ((pctx = init_prchunk(fd)) == NULL) {
		res = -1;
		goto fucked;
	}

	/* parser init */
	if ((hdl = XML_ParserCreate(NULL)) == NULL) {
		res = -1;
		goto fucked;
	}
	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, &clo);

	while (prchunk_fill(pctx) >= 0) {
		for (char *line; prchunk_haslinep(pctx);) {
			size_t llen;

			llen = prchunk_getline(pctx, &line);
			proc_line(hdl, line, llen);
		}
	}
	/* get rid of resources */
	free_prchunk(pctx);
	XML_ParserFree(hdl);

fucked:
	close(fd);
super_fucked:
	return res;
}


#if defined STANDALONE
static void
me_req(tws_xml_req_t req, void *clo)
{
	TX_DEBUG("request type %u\n", req->typ);
	switch (req->typ) {
	case TWS_XML_REQ_TYP_MKT_DATA:
		TX_DEBUG("aaah mkt data %p\n", req->qry.mkt_data.ins);
		break;
	default:
		break;
	}
	return;
}

int
main(int argc, char *argv[])
{
	int res = 0;

	if (tws_xml_parse(argv[1], me_req, NULL) < 0) {
		res = 1;
	}
	return res;
}
#endif	/* STANDALONE */

/* tws-xml.c ends here */
