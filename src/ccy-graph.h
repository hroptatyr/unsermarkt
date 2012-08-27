/*** ccy-graph.h -- paths through foreign exchange flow graphs
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
#if !defined INCLUDED_ccy_graph_h_
#define INCLUDED_ccy_graph_h_

#include <stdint.h>
#include "iso4217.h"

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

typedef size_t gpair_t;
typedef size_t gnode_t;
typedef size_t gedge_t;
typedef size_t gpath_def_t;
typedef size_t gpath_hop_t;
typedef union graph_u *graph_t;

struct pair_s {
	const_iso_4217_t bas;
	const_iso_4217_t trm;
};

#define NULL_PAIR	((gpair_t)0)
#define NULL_EDGE	((gedge_t)0)
#define NULL_PATH_HOP	((gpath_def_t)0)


extern graph_t make_graph(void);
extern void free_graph(graph_t);

extern gpair_t ccyg_find_pair(graph_t, struct pair_s);
extern gpair_t ccyg_add_pair(graph_t, struct pair_s p);

/**
 * After all pairs have been added, the graph must be augmented with
 * auxiliary information. */
extern void ccyg_populate(graph_t);

/**
 * Add all possible path that go from the term currency to the base currency.
 * Paths are virtual gpairs. */
extern void ccyg_add_paths(graph_t, struct pair_s);

#if defined DEBUG_FLAG
extern void prnt_graph(graph_t);
#endif	/* DEBUG_FLAG */

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ccy_graph_h_ */
