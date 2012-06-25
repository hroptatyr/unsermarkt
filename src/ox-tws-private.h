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

typedef struct ox_cl_s *ox_cl_t;
typedef struct ox_oq_item_s *ox_oq_item_t;
typedef struct ox_oq_dll_s *ox_oq_dll_t;
typedef struct ox_oq_s *ox_oq_t;


struct ox_oq_dll_s {
	ox_oq_item_t i1st;
	ox_oq_item_t ilst;
};

struct ox_oq_s {
	ox_oq_item_t items;
	size_t nitems;

	struct ox_oq_dll_s free[1];
	struct ox_oq_dll_s flld[1];
	struct ox_oq_dll_s cncd[1];
	struct ox_oq_dll_s ackd[1];
	struct ox_oq_dll_s sent[1];
	struct ox_oq_dll_s unpr[1];
};

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern ox_oq_item_t pop_match_oid(ox_oq_dll_t, tws_oid_t);
extern ox_oq_item_t pop_head(ox_oq_dll_t);
extern void push_tail(ox_oq_dll_t, ox_oq_item_t);

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ox_tws_private_h_ */
