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
typedef size_t gpath_t;
typedef union graph_u *graph_t;

struct pair_s {
	const_iso_4217_t bas;
	const_iso_4217_t trm;
};

struct gpair_s {
	struct pair_s p;

	/* offsets into the paths array */
	gnode_t off;
	size_t len;

	struct sl1t_s b;
	struct sl1t_s a;
};

struct gedge_s {
	uint64_t x;
};

struct gnode_s {
	gpair_t x;
};

union graph_u {
	struct {
		size_t npairs;
		size_t npaths;
		
		size_t alloc_pairs;
		size_t alloc_sz;

		struct gnode_s *f;
		struct gedge_s *e;
	};

	/* gpairs first */
	struct gpair_s p[];
};

#define NULL_PAIR	((gpair_t)0)
#define NULL_EDGE	((gedge_t)0)
#define NULL_PATH	((gpath_t)0)
#define P(g, x)		(g->p[x])
#define E(g, x)		(g->e[x])
#define F(g, x)		(g->f[x])


static size_t pgsz = 0;

#define INITIAL_PAIRS	(64)
#define INITIAL_PATHS	(512)

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

	/* leave room for 63 gpairs, 64 gedges and 512 gpaths */
	tmp += (INITIAL_PAIRS - 1) * sizeof(*res->p);
	tmp += INITIAL_PAIRS * sizeof(*res->e);
	tmp += (INITIAL_PATHS - INITIAL_PAIRS) * sizeof(*res->f);
	/* round up to pgsz */
	if (tmp % pgsz) {
		tmp -= tmp % pgsz;
		tmp += pgsz;
	}

	CCY_DEBUG("make graph of size %zu\n", tmp);
	res = mmap(NULL, tmp, PROT_MEM, MAP_MEM, -1, 0);

	/* polish the result, res->p is the only pointer in shape */
	res->e = (void*)(res->p + INITIAL_PAIRS);
	res->f = (void*)(res->e + INITIAL_PAIRS);

	res->alloc_sz = tmp;
	res->alloc_pairs = INITIAL_PAIRS - 1;
	E(res, 0).x = INITIAL_PAIRS;
	F(res, 0).x = INITIAL_PATHS;
	return res;
}

static void
free_graph(graph_t g)
{
	size_t sz = g->alloc_sz;

	munmap(g, sz);
	return;
}

static gpair_t
make_gpair(graph_t g)
{
	gpair_t r = ++g->npairs;

	if (UNLIKELY(r >= g->alloc_pairs)) {
		return NULL_PAIR;
	}
	return r;
}

static __attribute__((unused)) void
free_gpair(graph_t g, gpair_t o)
{
	memset(g->p + o, 0, sizeof(*g->p));
	return;
}

static gpath_t
make_gpath(graph_t g)
{
	gpath_t r = ++g->npaths;

	if (UNLIKELY(r >= F(g, 0).x)) {
		return NULL_EDGE;
	}
	return r;
}

static __attribute__((unused)) void
free_gpath(graph_t g, gpath_t o)
{
	F(g, o).x = 0;
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

	if ((tmp = find_pair(g, p)) == NULL_PAIR &&
	    (tmp = make_gpair(g)) != NULL_PAIR) {
		/* create a new pair */
		CCY_DEBUG("ctor'ing %s%s\n", p.bas->sym, p.trm->sym);
		P(g, tmp).p = p;
	}
	return;
}

static gedge_t
find_edge(graph_t g, gpair_t from, gpair_t to)
{
	if (E(g, from).x & (1 << (to - 1))) {
		return from;
	}
	return NULL_EDGE;
}

static void
add_edge(graph_t g, gpair_t from, gpair_t to)
{
	gedge_t tmp;

	if ((tmp = find_edge(g, from, to)) == NULL_EDGE &&
	    (tmp = (gedge_t)from) != NULL_EDGE) {
		/* create a new edge */
		CCY_DEBUG("ctor'ing %s%s (%zu) -> %s%s (%zu)\n",
			  P(g, from).p.bas->sym, P(g, from).p.trm->sym, from,
			  P(g, to).p.bas->sym, P(g, to).p.trm->sym, to);
		E(g, tmp).x |= 1 << (to - 1);
	}
	return;
}

static gpath_t
find_path(graph_t g, gpair_t from, gpair_t to)
{
	for (gedge_t i = P(g, from).off;
	     i < P(g, from).off + P(g, from).len; i++) {
		if (E(g, i).x == to) {
			return i;
		}
	}
	return NULL_PATH;
}

static void
add_path(graph_t g, gpath_t tgtpath, gpair_t via)
{
	gedge_t tmp;

	if ((tmp = find_path(g, from, to)) == NULL_PATH &&
	    (tmp = make_gpath(g)) != NULL_PATH) {
		/* create a new edge */
		CCY_DEBUG("ctor'ing %s%s (%zu) -> %s%s (%zu)\n",
			  P(g, from).p.bas->sym, P(g, from).p.trm->sym, from,
			  P(g, to).p.bas->sym, P(g, to).p.trm->sym, to);
		F(g, tmp).x = to;
		if (P(g, from).len++ == 0) {
			P(g, from).off = tmp;
		}
	}
	return;
}


static void
populate(graph_t g)
{
	for (gpair_t i = 1; i <= g->npairs; i++) {
		const_iso_4217_t bas = P(g, i).p.bas;
		const_iso_4217_t trm = P(g, i).p.trm;

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
static __attribute__((unused)) int
path_edge_finder(graph_t g, gpair_t x, struct pair_s p)
{
	if (p.bas == p.trm) {
		CCY_DEBUG("  ... trivial %s%s\n",
			  P(g, x).p.bas->sym, P(g, x).p.trm->sym);
		return 2;
	}

	for (uint64_t ex = E(g, x).x, i = 1; ex; ex >>= 1, i++) {
		gpair_t y = (gpair_t)i;
		struct pair_s cp;

		if (!(ex & 1)) {
			continue;
		} else if ((P(g, y).p.bas == p.bas &&
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
		for (uint64_t ey = E(g, y).x, j = 1; ey; ey >>= 1, j++) {
			gpair_t z = (gpair_t)j;

			if (!(ey & 1)) {
			} else if ((P(g, z).p.bas == cp.bas &&
				    P(g, z).p.trm == cp.trm) ||
				   (P(g, z).p.bas == cp.trm &&
				    P(g, z).p.trm == cp.bas)) {
				CCY_DEBUG("  ... finally %s%s\n",
					  P(g, z).p.bas->sym,
					  P(g, z).p.trm->sym);
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

static __attribute__((unused)) void
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
		if (path_edge_finder(g, i, p) == 1) {
			CCY_DEBUG("      ... via %s%s\n",
				  P(g, i).p.bas->sym, P(g, i).p.trm->sym);
		}
	}
	return;
}

static gpath_t
edge_finder(graph_t g, gpair_t x, struct pair_s p)
{
	gpath_t res = NULL_PATH;

	for (uint64_t ex = E(g, x).x, i = 1; ex; ex >>= 1, i++) {
		gpair_t y = (gpair_t)i;
		struct pair_s cp;

		if (!(ex & 1)) {
			continue;
		} else if ((P(g, y).p.bas == p.bas &&
			    P(g, y).p.trm == p.trm) ||
			   (P(g, y).p.bas == p.trm &&
			    P(g, y).p.trm == p.bas)) {
			/* all starts off here */
			res = make_gpath(g);
			add_path(g, res, y);
			CCY_DEBUG("  ... finally %s%s\n",
				  P(g, y).p.bas->sym, P(g, y).p.trm->sym);
			break;

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
		for (uint64_t ey = E(g, y).x, j = 1; ey; ey >>= 1, j++) {
			gpair_t z = (gpair_t)j;

			if (!(ey & 1)) {
			} else if ((P(g, z).p.bas == cp.bas &&
				    P(g, z).p.trm == cp.trm) ||
				   (P(g, z).p.bas == cp.trm &&
				    P(g, z).p.trm == cp.bas)) {
				CCY_DEBUG("  ... finally %s%s\n",
					  P(g, z).p.bas->sym,
					  P(g, z).p.trm->sym);
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
	return res;
}

static void
add_paths(graph_t g, struct pair_s x)
{
/* adds a virtual pair X from paths found */
	struct pair_s p = x;

	CCY_DEBUG("adding paths XCH %s FOR %s\n", x.bas->sym, x.trm->sym);

	if (x.bas == x.trm) {
		/* trivial */
		return;
	}
	for (gpair_t i = 1; i <= g->npairs; i++) {
		gpath_t f;
		if (P(g, i).p.bas == x.bas) {
			p.bas = P(g, i).p.trm;
		} else if (P(g, i).p.trm == x.bas) {
			p.bas = P(g, i).p.bas;
		} else {
			continue;
		}
		if (p.bas == p.trm) {
			CCY_DEBUG("  ... trivial %s%s\n",
				  P(g, i).p.bas->sym, P(g, i).p.trm->sym);
			;
		} else if ((f = edge_finder(g, i, p)) != NULL_PATH) {
			CCY_DEBUG("      ... via %s%s  added to %zu\n",
				  P(g, i).p.bas->sym, P(g, i).p.trm->sym, f);
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
	add_paths(g, (struct pair_s){ISO_4217_NZD, ISO_4217_JPY});

	add_paths(g, (struct pair_s){ISO_4217_AUD, ISO_4217_EUR});

	add_paths(g, (struct pair_s){ISO_4217_EUR, ISO_4217_GBP});

	free_graph(g);
	return 0;
}
#endif	/* STANDALONE */

/* ccy-graph.c ends here */
