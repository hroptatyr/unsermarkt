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
#include <sys/mman.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "iso4217.h"
#include "nifty.h"


#if defined DEBUG_FLAG
# define CCY_DEBUG(args...)	fprintf(stderr, args)
#else
# define CCY_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef size_t gpair_t;
typedef size_t gnode_t;
typedef size_t gedge_t;
typedef union graph_u *graph_t;

struct pair_s {
	const_iso_4217_t bas;
	const_iso_4217_t trm;
};

struct gpair_s {
	struct pair_s p;

	/* offsets into the edges array */
	gnode_t off;
	uint32_t len_aux;
	uint32_t len;

	struct sl1t_s b;
	struct sl1t_s a;
};

struct gnode_s {
	gpair_t x;
};

union graph_u {
	struct {
		size_t npairs;
		size_t nedges;
		
		size_t alloc_pairs;
		size_t alloc_sz;

		/* one page for this */
		struct gnode_s *e;
	};

	/* gpairs first */
	struct gpair_s p[];
};

#define NULL_PAIR	((gpair_t)0)
#define NULL_EDGE	((gedge_t)0)
#define P(g, x)		(g->p[x])
#define E(g, x)		(g->e[x])


static size_t pgsz = 0;

static gpair_t
make_gpair(graph_t g)
{
	gpair_t r = ++g->npairs;

	if (UNLIKELY(r >= g->alloc_pairs)) {
		return NULL_PAIR;
	}
	return r;
}

static void
free_gpair(graph_t g, gpair_t o)
{
	memset(g->p + o, 0, sizeof(*g->p));
	return;
}

static gedge_t
make_gedge(graph_t g)
{
	gedge_t r = ++g->nedges;

	if (UNLIKELY(r >= E(g, 0).x)) {
		return NULL_EDGE;
	}
	return r;
}

static void
free_gedge(graph_t g, gedge_t o)
{
	E(g, o).x = 0;
	return;
}

static gpair_t
find_pair(graph_t g, struct pair_s p)
{
	for (gpair_t i = 1; i <= g->npairs; i++) {
		if (P(g, i).p.bas == p.bas && P(g, i).p.trm == p.trm) {
			return i;
		}
	}
	return NULL_PAIR;
}

static void
add_pair(graph_t g, struct pair_s p)
{
	gpair_t tmp;

	if ((tmp = find_pair(g, p)) == NULL_PAIR) {
		/* create a new pair */
		CCY_DEBUG("ctor'ing %s%s\n", p.bas->sym, p.trm->sym);
		tmp = make_gpair(g);
		P(g, tmp).p = p;
	}
	return;
}

static gedge_t
find_edge(graph_t g, gpair_t from, gpair_t to)
{
	for (gedge_t i = P(g, from).off;
	     i < P(g, from).off + P(g, from).len; i++) {
		if (E(g, i).x == to) {
			return i;
		}
	}
	return NULL_EDGE;
}

static void
add_edge(graph_t g, gpair_t from, gpair_t to)
{
	gedge_t tmp;

	if ((tmp = find_edge(g, from, to)) == NULL_EDGE) {
		/* create a new edge */
		CCY_DEBUG("ctor'ing %s%s (%zu) -> %s%s (%zu)\n",
			  P(g, from).p.bas->sym, P(g, from).p.trm->sym, from,
			  P(g, to).p.bas->sym, P(g, to).p.trm->sym, to);
		tmp = make_gedge(g);
		E(g, tmp).x = to;
		if (P(g, from).len++ == 0) {
			P(g, from).off = tmp;
		}
	}
	return;
}

#define INITIAL_PAIRS	(64)
#define INITIAL_EDGES	(512)

static graph_t
make_graph(void)
{
#define PROT_MEM	(PROT_READ | PROT_WRITE)
#define MAP_MEM		(MAP_PRIVATE | MAP_ANON)
	graph_t res;
	size_t tmp = sizeof(*res);

	if (pgsz == 0) {
		pgsz = sysconf(_SC_PAGESIZE);
	}

	/* leave room for 63 gpairs */
	tmp += (INITIAL_PAIRS - 1) * sizeof(*res->p);
	tmp += INITIAL_EDGES * sizeof(*res->e);
	/* round up to pgsz */
	if (tmp % pgsz) {
		tmp -= tmp % pgsz;
		tmp += pgsz;
	}

	CCY_DEBUG("make graph of size %zu\n", tmp);
	res = mmap(NULL, tmp, PROT_MEM, MAP_MEM, -1, 0);

	/* polish the result, res->p is the only pointer in shape */
	res->e = (void*)(res->p + INITIAL_PAIRS);

	res->alloc_sz = tmp;
	res->alloc_pairs = INITIAL_PAIRS - 1;
	E(res, 0).x = INITIAL_EDGES;
	return res;
}

static void
free_graph(graph_t g)
{
	size_t sz = g->alloc_sz;

	munmap(g, sz);
	return;
}

static void
populate(graph_t g)
{
	/* reset the edge count */
	g->nedges = 0;

	for (gpair_t i = 1; i <= g->npairs; i++) {
		const_iso_4217_t bas = P(g, i).p.bas;
		const_iso_4217_t trm = P(g, i).p.trm;

		/* reset the nodes */
		P(g, i).off = 0;
		P(g, i).len_aux = P(g, i).len = 0;

		/* foreign bas/trm == bas */
		for (gpair_t j = 1; j <= g->npairs; j++) {
			if (UNLIKELY(i == j)) {
				/* don't want no steenkin loops */
				continue;
			}
			if (P(g, j).p.bas == bas || P(g, j).p.trm == bas) {
				/* yay */
				add_edge(g, i, j);
			}
		}

		/* store the helper length the number of incomings */
		P(g, i).len_aux = P(g, i).len;

		/* outgoing, aka foreign bas/trm == trm */
		for (gpair_t j = 1; j <= g->npairs; j++) {
			if (UNLIKELY(i == j)) {
				/* don't want no steenkin loops */
				continue;
			}
			if (P(g, j).p.bas == trm || P(g, j).p.trm == trm) {
				/* yay */
				add_edge(g, i, j);
			}
		}
	}
	return;
}

/* path finder */
static int
edge_finder(graph_t g, gpair_t x, struct pair_s p)
{
	if (p.bas == p.trm) {
		CCY_DEBUG("  ... trivial %s%s\n",
			  P(g, x).p.bas->sym, P(g, x).p.trm->sym);
		return 2;
	}

	for (gedge_t i = P(g, x).off; i < P(g, x).off + P(g, x).len; i++) {
		gpair_t y = E(g, i).x;
		struct pair_s cp;

		if ((P(g, y).p.bas == p.bas &&
		     P(g, y).p.trm == p.trm) ||
		    (P(g, y).p.bas == p.trm &&
		     P(g, y).p.trm == p.bas)) {
			CCY_DEBUG("  ... finally %s%s\n",
				  P(g, y).p.bas->sym, P(g, y).p.trm->sym);
			return 1;

		} else if (P(g, y).p.bas == p.bas) {
			cp.bas = P(g, y).p.trm;
			cp.trm = p.trm;
		} else if (P(g, y).p.trm == p.bas) {
			cp.bas = P(g, y).p.bas;
			cp.trm = p.trm;
		} else {
			continue;
		}

		/* 2nd indirection, unrolled */
		for (gedge_t j = P(g, y).off;
		     j < P(g, y).off + P(g, y).len; j++) {
			gpair_t z = E(g, j).x;

			if ((P(g, z).p.bas == cp.bas &&
			     P(g, z).p.trm == cp.trm) ||
			    (P(g, z).p.bas == cp.trm &&
			     P(g, z).p.trm == cp.bas)) {
				CCY_DEBUG("  ... finally %s%s\n",
					  P(g, z).p.bas->sym, P(g, z).p.trm->sym);
				goto via;
			}
		}
		continue;

	via:
		CCY_DEBUG("      ... via %s%s\n",
			  P(g, y).p.bas->sym, P(g, y).p.trm->sym);
		CCY_DEBUG("      ... via %s%s\n",
			  P(g, x).p.bas->sym, P(g, x).p.trm->sym);
	}
	return 0;
}

static void
path_finder(graph_t g, struct pair_s x)
{
	struct pair_s p = x;

	CCY_DEBUG("XCH %s FOR %s\n", x.bas->sym, x.trm->sym);

	if (x.bas == x.trm) {
		/* trivial */
		return;
	}
	for (gpair_t i = 1; i <= g->npairs; i++) {
		if (P(g, i).p.bas == x.bas) {
			p.bas = P(g, i).p.trm;
		} else if (P(g, i).p.trm == x.bas) {
			p.bas = P(g, i).p.bas;
		} else {
			continue;
		}
		if (edge_finder(g, i, p) == 1) {
			CCY_DEBUG("      ... via %s%s\n",
				  P(g, i).p.bas->sym, P(g, i).p.trm->sym);
		}
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

static struct pair_s USDJPY = {
	ISO_4217_USD,
	ISO_4217_JPY,
};

static struct pair_s AUDNZD = {
	ISO_4217_AUD,
	ISO_4217_NZD,
};

static struct pair_s AUDUSD = {
	ISO_4217_AUD,
	ISO_4217_USD,
};

static struct pair_s EURAUD = {
	ISO_4217_EUR,
	ISO_4217_AUD,
};

int
main(int argc, char *argv[])
{
	static graph_t g;

	g = make_graph();

	/* pair adding */
	add_pair(g, EURUSD);
	add_pair(g, GBPUSD);
	add_pair(g, EURGBP);
	add_pair(g, USDJPY);
	add_pair(g, AUDNZD);
	add_pair(g, AUDUSD);
	add_pair(g, EURAUD);

	/* population */
	populate(g);

	/* find me all paths from NZD to JPY */
	path_finder(g, (struct pair_s){ISO_4217_NZD, ISO_4217_JPY});

	path_finder(g, (struct pair_s){ISO_4217_AUD, ISO_4217_EUR});

	free_graph(g);
	return 0;
}
#endif	/* STANDALONE */

/* ccy-graph.c ends here */
