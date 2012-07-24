/*** tws-xml.h -- conversion between IB/API structs and xml
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
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "iso4217.h"
#include "nifty.h"
#include "gq.h"


#if defined DEBUG_FLAG
# define CCY_DEBUG(args...)	fprintf(stderr, args)
#else
# define CCY_DEBUG(args...)
#endif	/* DEBUG_FLAG */


typedef struct graph_s *graph_t;
typedef struct gpair_s *gpair_t;
typedef struct gccy_s *gccy_t;

struct graph_s {
	struct gq_s pool[1];
	struct gq_ll_s pairs[1];
	struct gq_ll_s bass[1];
	struct gq_ll_s trms[1];
};

struct pair_s {
	const_iso_4217_t bas;
	const_iso_4217_t trm;
};

struct gccy_s {
	struct gq_item_s i;

	const_iso_4217_t ccy;
};

struct gpair_s {
	struct gq_item_s i;

	union {
		struct {
			/* the fx pair we're talking */
			struct pair_s p;

			struct sl1t_s b;
			struct sl1t_s a;
		};

		struct {
			const_iso_4217_t ccy;
			struct gccy_s x;
		} counter;

		struct {
			const_iso_4217_t bas;
			struct gccy_s trm;
		} bas;

		struct {
			const_iso_4217_t trm;
			struct gccy_s bas;
		} trm;
	};
};


#include "gq.c"


static gpair_t
make_gpair(graph_t g)
{
	gpair_t res;

	if (g->pool->free->i1st == NULL) {
		size_t nitems = g->pool->nitems / sizeof(*res);
		ptrdiff_t df;

		CCY_DEBUG("G RESIZE -> %zu\n", nitems + 64);
		df = init_gq(g->pool, sizeof(*res), nitems + 64);
		gq_rbld_ll(g->pairs, df);
		gq_rbld_ll(g->bass, df);
		gq_rbld_ll(g->trms, df);
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(g->pool->free);
	memset(res, 0, sizeof(*res));
	return res;
}

static void
free_gpair(graph_t g, gpair_t o)
{
	gq_push_tail(g->pool->free, (gq_item_t)o);
	return;
}

static gpair_t
find_pair(graph_t g, struct pair_s p)
{
	for (gq_item_t i = g->pairs->i1st; i; i = i->next) {
		gpair_t gp = (void*)i;
		if (gp->p.bas == p.bas && gp->p.trm == p.trm) {
			return gp;
		}
	}
	return NULL;
}

static gpair_t
find_bas(graph_t g, const_iso_4217_t bas)
{
	for (gq_item_t i = g->bass->i1st; i; i = i->next) {
		gpair_t gp = (void*)i;
		if (gp->bas.bas == bas) {
			return gp;
		}
	}
	return NULL;
}

static gpair_t
find_trm(graph_t g, const_iso_4217_t trm)
{
	for (gq_item_t i = g->trms->i1st; i; i = i->next) {
		gpair_t gp = (void*)i;
		if (gp->trm.trm == trm) {
			return gp;
		}
	}
	return NULL;
}

static gpair_t
find_counter(gpair_t g, const_iso_4217_t ccy)
{
	return NULL;
}

static void
add_pair(graph_t g, struct pair_s p)
{
	gpair_t tmp;
	gpair_t counter;

	if ((tmp = find_pair(g, p)) == NULL) {
		/* create a new pair */
		CCY_DEBUG("ctor'ing %s%s\n", p.bas->sym, p.trm->sym);
		tmp = make_gpair(g);
		tmp->p = p;
		gq_push_tail(g->pairs, (gq_item_t)tmp);
	}

	/* add a pointer to the bas list */
	if ((tmp = find_bas(g, p.bas)) == NULL) {
		CCY_DEBUG("ctor'ing bas %s\n", p.bas->sym);
		tmp = make_gpair(g);
		tmp->bas.bas = p.bas;
		gq_push_tail(g->bass, (gq_item_t)tmp);
	}
	/* leave a not about the trm */
	if ((counter = find_counter(tmp, p.trm)) == NULL) {
		CCY_DEBUG("ctor'ing counter-bas %s\n", p.trm->sym);
		counter = make_gpair(g);
	}

	/* add a pointer to the trm list */
	if ((tmp = find_trm(g, p.trm)) == NULL) {
		CCY_DEBUG("ctor'ing trm %s\n", p.trm->sym);
		tmp = make_gpair(g);
		tmp->trm.trm = p.trm;
		gq_push_tail(g->trms, (gq_item_t)tmp);
	}
	/* and counter struct again */
	if ((counter = find_counter(tmp, p.bas)) == NULL) {
		CCY_DEBUG("ctor'ing counter-trm %s\n", p.bas->sym);
		counter = make_gpair(g);
	}
	return;
}


#if defined STANDALONE
static struct pair_s EURUSD = {
	ISO_4217_EUR,
	ISO_4217_USD,
};

static struct pair_s GBPUSD = {
	ISO_4217_GBP,
	ISO_4217_USD,
};

static struct pair_s EURGBP = {
	ISO_4217_EUR,
	ISO_4217_GBP,
};

int
main(int argc, char *argv[])
{
	struct graph_s g[1] = {{0}};

	add_pair(g, EURUSD);
	add_pair(g, GBPUSD);
	add_pair(g, EURGBP);

	/* free resources */
	for (gq_item_t i; (i = gq_pop_head(g->pairs));) {
		gpair_t gp = (void*)i;
		free_gpair(g, gp);
	}

	fini_gq(g->pool);
	return 0;
}
#endif	/* STANDALONE */

/* ccy-graph.c ends here */
