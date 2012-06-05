/*** tenfore.c -- leech some tenfore resources
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <stdarg.h>
#include <errno.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

#include <uterus.h>
/* to get a take on them m30s and m62s */
#define DEFINE_GORY_STUFF
#include <m30.h>
#include <m62.h>

static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("tenfore: ", stderr);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static int
init_sockaddr(ud_sockaddr_t sa, const char *name, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(name, NULL, &hints, &res)) {
		return -1;
	}
	memcpy(sa, res->ai_addr, res->ai_addrlen);
	sa->sa6.sin6_port = htons(port);
	freeaddrinfo(res);
	return 0;
}


static char iobuf[4096];

static int
init_nd(void)
{
	static const char host[] = "balancer.netdania.com";
	union ud_sockaddr_u sa[1];
	int res;

	if (init_sockaddr(sa, host, 80) < 0) {
		error("Error resolving host %s", host);
		return -1;
	} else if ((res = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		error("Error getting socket");
		return -1;
	} else if (connect(res, &sa->sa, sizeof(*sa)) < 0) {
		error("Error connecting to sock %d", res);
		close(res);
		return -1;
	}
	return res;
}

static int
subs_nd(int s, const char **syms, size_t nsyms)
{
	static const char pre[] = "\
GET /StreamingServer/StreamingServer?xstream&group=www.forex-markets.com&user=.&pass=.&appid=quotelist_awt&xcmd&type=1&reqid=";
	static const char post[] = "\
User-Agent: Streaming Agent\r\n\
Host: balancer.netdania.com\r\n\
\r\n\
";
	static const char trick[] = "1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16";
	static const char sym[] = "&sym=";
	char *p = iobuf;
	unsigned int len;

	if (nsyms == 0) {
		return -1;
	} else if (nsyms > 16) {
		/* for now */
		nsyms = 16;
	}

	memcpy(p, pre, len = sizeof(pre) - 1);
	p += len;

	len = 2 * nsyms - 1 + (nsyms < 10 ? 0 : nsyms - 9);
	memcpy(p, trick, len);
	p += len;

	memcpy(p, sym, len = sizeof(sym) - 1);
	p += len;

	for (size_t i = 0; i < nsyms; i++) {
		static const char suf[] = "|ms_dla;";

		memcpy(p, syms[i], len = strlen(syms[i]));
		p += len;

		memcpy(p, suf, len = sizeof(suf) - 1);
		p += len;
	}
	p[-1] = '\r';
	*p++ = '\n';

	memcpy(p, post, len = sizeof(post));
	p += len - 1/*\nul*/;

	/* assume the socket is safe to send to */
	send(s, iobuf, p - iobuf, 0);
	return 0;
}

/* the actual worker function */
static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nread;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	j->sock = w->fd;
	nread = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nread < 0)) {
		goto out_revok;
	} else if (nread == 0) {
		/* no need to bother */
		goto out_revok;
	}

	j->blen = nread;

	fputs("hooray\n", stderr);

out_revok:
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "tenfore-clo.h"
#include "tenfore-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* args */
	struct tf_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_io beef[1];
	/* unserding resources */
	ud_chan_t hdl;
	int nd_sock;

	/* parse the command line */
	if (tf_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach to the beef channel */
	hdl = ud_chan_init(argi->beef_given ? argi->beef_arg : 8584/*UT*/);

	/* connect to netdania balancer */
	nd_sock = init_nd();
	ev_io_init(beef, mon_beef_cb, nd_sock, EV_READ);
	ev_io_start(EV_A_ beef);

	/* quickly perform the subscription */
	subs_nd(nd_sock, (const char**)argi->inputs, argi->inputs_num);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	ev_io_stop(EV_A_ beef);
	close(nd_sock);

	/* detach ud resources */
	ud_chan_fini(hdl);

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	tf_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* tenfore.c ends here */
