/*** match.h -- unsermarkt match messages.
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
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
/**
 * Matches are the glues between order execution engines and clearing
 * engines (or whoever wishes to track the portfolio).
 *
 * Serving the true nature of the unserding ad-hoc policy, matches in
 * unsermarkt are stripped down to the bare essentials, being quite
 * verbose nonetheless as there is no central agency that hands out
 * order ids, agent ids, broker ids, etc.
 *
 * Many things in this file will eventually wander into unserding.
 * Consider this header a testbed for all sorts of match messages.
 *
 **/
#if !defined INCLUDED_match_h_
#define INCLUDED_match_h_

#include <uterus/m30.h>
#include "um-types.h"

typedef struct umm_s *umm_t;

struct umm_s {
	/* buyer and seller order ids */
	oid_t ob, os;
	/* buyer and seller agent ids */
	agtid_t ab, as;
	/* instr ids, buyer/seller */
	insid_t ib, is;
	/* agreed upon price */
	m30_t p;
	/* agreed upon quantity */
	uint32_t q;
	/* time stamp */
	uint32_t ts_sec;
	uint32_t ts_usec:20;
};

#endif	/* !INCLUDED_match_h_ */
