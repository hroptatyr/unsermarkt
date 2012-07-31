/*** gen-tws.c -- generic tws c api
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
/* for gmtime_r */
#include <time.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#if defined STANDALONE
# include <sys/epoll.h>
#endif	/* STANDALONE */

/* the tws api */
#include "gen-tws.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define GEN_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define GEN_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

/* error() impl */
static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("[quo-tws] ", stderr);
	vfprintf(logerr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ? eno : errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


#if defined STANDALONE
int
main(int argc, char *argv[])
{
	const char host[] = "quant";
	short unsigned int port = 7474;
	struct tws_s tws[1] = {{0}};
	struct epoll_event ev[1];
	int epfd;
	int s;
	int res = 0;

	if (init_tws(tws) < 0) {
		return 1;
	}

	if ((s = tws_connect(tws, host, port, 3333)) < 0) {
		res = 1;
		goto fini;
	}

	if ((epfd = epoll_create(1)) < 0) {
		res = 1;
		goto disc;
	}
	/* add s to epoll descriptor */
	ev->events = EPOLLIN | EPOLLOUT | EPOLLHUP;
	epoll_ctl(epfd, EPOLL_CTL_ADD, s, ev);

	while (epoll_wait(epfd, ev, 1, 2000) > 0) {
		if (ev->events & EPOLLHUP) {
			break;
		}
		if (ev->events & EPOLLIN) {
			tws_recv(tws);
		}
		if (ev->events & EPOLLOUT) {
			tws_send(tws);
			ev->events = EPOLLIN | EPOLLHUP;
			epoll_ctl(epfd, EPOLL_CTL_MOD, s, ev);
		}
	}

disc:
	if (tws_disconnect(tws) < 0) {
		res = 1;
		goto fini;
	}

fini:
	if (fini_tws(tws) < 0) {
		res = 1;
	}
	return res;
}
#endif	/* STANDALONE */

/* gen-tws.c ends here */
