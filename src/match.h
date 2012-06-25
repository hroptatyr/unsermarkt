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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdint.h>
#include <netinet/in.h>
#include <uterus/uterus.h>
#include "um-types.h"

/**
 * Match message for the unsermarkt dso. */
typedef struct umm_s *umm_t;

/**
 * Match message for ox-* daemons. */
typedef struct umm_pair_s *umm_pair_t;

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

/**
 * Match messages for ox-* and uschi-* daemons. */
struct umm_agt_s {
	struct in6_addr addr;
	uint16_t port;
	uint16_t uidx;
};

union umm_hdr_u {
	uint64_t u;
	struct {
#if defined WORDS_BIGENDIAN
		uint32_t stmp:32;
		uint32_t msec:10;
		uint32_t rest:22;
#else  /* !WORDS_BIGENDIAN */
		uint32_t rest:22;
		uint32_t msec:10;
		uint32_t stmp:32;
#endif	/* WORDS_BIGENDIAN */
	};
	union scom_thdr_u sc[1];
};

struct umm_pair_s {
	/* by coincidence this is a normal ute sl1t_s */
	union {
		struct {
			union umm_hdr_u hdr[1];
			m30_t p;
			m30_t q;
		};
		struct sl1t_s l1[1];
	};
	/* agents now, first one is the buyer, second the seller */
	struct umm_agt_s agt[2];
};

#endif	/* !INCLUDED_match_h_ */
