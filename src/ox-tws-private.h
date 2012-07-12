/*** ox-tws-private.h -- private data flow guts
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
#if !defined INCLUDED_ox_tws_private_h_
#define INCLUDED_ox_tws_private_h_

#include "match.h"
#include "gq.h"

typedef struct ox_oq_item_s *ox_oq_item_t;
typedef struct ox_oq_dll_s *ox_oq_dll_t;
typedef struct ox_oq_s *ox_oq_t;


struct ox_oq_dll_s {
	ox_oq_item_t i1st;
	ox_oq_item_t ilst;
};

struct ox_oq_s {
	struct gq_s q[1];

	struct ox_oq_dll_s flld[1];
	struct ox_oq_dll_s cncd[1];
	struct ox_oq_dll_s ackd[1];
	struct ox_oq_dll_s sent[1];
	struct ox_oq_dll_s unpr[1];
};

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern ox_oq_item_t find_match_oid(ox_oq_dll_t, tws_oid_t);
extern ox_oq_item_t clone_item(ox_oq_item_t);

/* to indicate actual execution prices and sizes */
extern void set_prc(ox_oq_item_t, double pri);
extern void set_qty(ox_oq_item_t, double qty);

static inline ox_oq_item_t
oq_pop_head(ox_oq_dll_t dll)
{
	return (ox_oq_item_t)gq_pop_head((gq_ll_t)dll);
}

static inline void
oq_push_tail(ox_oq_dll_t dll, ox_oq_item_t i)
{
	gq_push_tail((gq_ll_t)dll, (gq_item_t)i);
	return;
}

static inline void
oq_pop_item(ox_oq_dll_t dll, ox_oq_item_t i)
{
	gq_pop_item((gq_ll_t)dll, (gq_item_t)i);
	return;
}

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ox_tws_private_h_ */
