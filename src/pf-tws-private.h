/*** pf-tws-private.h -- private data flow guts
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
#if !defined INCLUDED_pf_tws_private_h_
#define INCLUDED_pf_tws_private_h_

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

typedef void *pf_pq_t;
typedef struct pf_pos_s *pf_pos_t;

struct pf_pos_s {
	const char *sym;
	double lqty;
	double sqty;
};

extern void fix_pos_rpt(pf_pq_t, const char *ac, struct pf_pos_s);

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_pf_tws_private_h_ */
