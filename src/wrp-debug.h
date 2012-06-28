/*** wrp-debug.h -- just variadic debug printers
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
#if !defined INCLUDED_wrp_debug_h_
#define INCLUDED_wrp_debug_h_

#include <stdio.h>
#include <stdarg.h>

/* logerr should be defined to void* somewhere  */
extern void *logerr;
#define LOGERR		((FILE*)logerr)

static inline void
__attribute__((format(printf, 2, 3)))
wrp_debug(void *c, const char *fmt, ...)
{
	va_list vap;

	fprintf(LOGERR, "[tws] %p: ", c);
	va_start(vap, fmt);
	vfprintf(LOGERR, fmt, vap);
	va_end(vap);
	fputc('\n', LOGERR);
	return;
}

static inline void
__attribute__((format(printf, 2, 3)))
glu_debug(void *c, const char *fmt, ...)
{
	va_list vap;

	fprintf(LOGERR, "[glu/contract] %p: ", c);
	va_start(vap, fmt);
	vfprintf(LOGERR, fmt, vap);
	va_end(vap);
	fputc('\n', LOGERR);
	return;
}

#if defined DEBUG_FLAG
/* assumes your class has a context */
# define GLU_DEBUG(args...)	glu_debug((void*)this->ctx, args)
# define WRP_DEBUG(args...)	wrp_debug((void*)this->ctx, args)
#else  /* !DEBUG_FLAG */
# define GLU_DEBUG(args...)
# define WRP_DEBUG(args...)
#endif	/* DEBUG_FLAG */

#endif	/* INCLUDED_wrp_debug_h_ */
