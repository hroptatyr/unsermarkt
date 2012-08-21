/*** gen-tws-cont-glu.h -- generic tws c api contract builder
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
#if !defined INCLUDED_gen_tws_cont_glu_h_
#define INCLUDED_gen_tws_cont_glu_h_

#include <unistd.h>

#if defined __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif	/* __cplusplus */

/* incorporate attr AID of namespace NS with value VAL into TGT. */
extern int
tws_cont_x(tws_cont_t tgt, unsigned int ns, unsigned int aid, const char *val);

/* excorporate contract C into BUF of size BSZ using namespace NS. */
extern ssize_t
tws_cont_y(char *restrict buf, size_t bsz, unsigned int ns, tws_cont_t c);

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_gen_tws_cont_glu_h_ */
