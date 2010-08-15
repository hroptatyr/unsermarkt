/*** dso-oq.c -- order queuing
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include "module.h"
#include "unserding.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-nifty.h"
#include "unserding-ctx.h"
#include "ud-sock.h"

#include <ev.h>
#undef EV_P
#define EV_P	struct ev_loop *loop __attribute__((unused))

#define MOD_PRE		"mod/oq"
#define UM_PORT		(12768)


/* order queue */
static void
prhttphdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 200 OK\r\n\
Content-Type: text/html; charset=ASCII\r\n\
\r\n";
	write(fd, httphdr, sizeof(httphdr));
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	static const char nooq[] = "no orders yet\n";

	prhttphdr(fd);
	write(fd, nooq, sizeof(nooq));
	return;
}


/* connection mumbo-jumbo */
static int oqsock;
static ev_io ALGN16(__wio)[1];

/* server to client goodness */
static ud_sockaddr_u __sa6 = {
	.sa6.sin6_addr = IN6ADDR_ANY_INIT
};

static void
__shut_sock(int s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
	return;
}

static inline void
__reuse_sock(int sock)
{
	if (setsock_reuseaddr(sock) < 0) {
		UD_CRITICAL(MOD_PRE ": setsockopt(SO_REUSEADDR) failed\n");
	}
	if (setsock_reuseport(sock) < 0) {
		UD_CRITICAL(MOD_PRE ": setsockopt(SO_REUSEPORT) failed\n");
	}
	return;
}

static void
clo_wio(EV_P_ ev_io *w)
{
	fsync(w->fd);
	ev_io_stop(EV_A_ w);
	__shut_sock(w->fd);
	xfree(w);
	return;
}

/* we could take args like listen address and port number */
static int
listener(void)
{
#if defined IPPROTO_IPV6
	int opt;
	volatile int s;

	__sa6.sa6.sin6_family = AF_INET6;
	__sa6.sa6.sin6_port = htons(UM_PORT);

	if (LIKELY((s = socket(PF_INET6, SOCK_STREAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UD_DEBUG(MOD_PRE ": socket() failed ... I'm clueless now\n");
		return s;
	}
	__reuse_sock(s);

#if defined IPV6_V6ONLY
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif	/* IPV6_V6ONLY */
#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &opt, sizeof(opt));
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_DONTFRAG, &opt, sizeof(opt));
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, &opt, sizeof(opt));
#endif

	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6)) < 0 ||
	    listen(s, 2) < 0) {
		UD_DEBUG(MOD_PRE ": bind() failed, errno %d\n", errno);
		close(s);
		return -1;
	}
	return s;

#else  /* !IPPROTO_IPV6 */
	return -1;
#endif	/* IPPROTO_IPV6 */
}

static void
data_cb(EV_P_ ev_io *w, int re)
{
	char buf[4096];
	ssize_t nrd;

	if ((nrd = read(w->fd, buf, sizeof(buf))) <= 0) {
		UD_DEBUG(MOD_PRE ": no data, closing socket %d %d\n", w->fd, re);
		clo_wio(EV_A_ w);
		return;
	}

#define GET_COOKIE	"GET /"
#define HEAD_COOKIE	"HEAD /"
	UD_DEBUG(MOD_PRE ": new data in sock %d\n", w->fd);
	if (strncmp(buf, GET_COOKIE, sizeof(GET_COOKIE) - 1) == 0) {
		/* obviously a browser managed to connect to us,
		 * print the current order queue and fuck off */
		prstatus(w->fd);
		clo_wio(EV_A_ w);
	} else if (strncmp(buf, HEAD_COOKIE, sizeof(HEAD_COOKIE) - 1) == 0) {
		/* obviously a browser managed to connect to us,
		 * print the current order queue and fuck off */
		prhttphdr(w->fd);
		clo_wio(EV_A_ w);
	} else {
		/* just print the buffer */
		write(STDERR_FILENO, buf, nrd);
	}
	return;
}

static void
inco_cb(EV_P_ ev_io *w, int UNUSED(re))
{
/* we're tcp so we've got to accept() the bugger, don't forget :) */
	volatile int ns;
	ev_io *aw;
	ud_sockaddr_u sa;
	socklen_t sa_size = sizeof(sa);

	UD_DEBUG(MOD_PRE ": they got back to us...");
	if ((ns = accept(w->fd, (struct sockaddr*)&sa, &sa_size)) < 0) {
		UD_DBGCONT("accept() failed\n");
		return;
	}

        /* make an io watcher and watch the accepted socket */
	aw = xnew(ev_io);
        ev_io_init(aw, data_cb, ns, EV_READ);
        ev_io_start(EV_A_ aw);
	UD_DBGCONT("success, new sock %d\n", ns);
	return;
}

static void
init_watchers(EV_P_ int s)
{
	if (s < 0) {
		return;
	}

        /* initialise an io watcher, then start it */
        ev_io_init(__wio, inco_cb, s, EV_READ);
        ev_io_start(EV_A_ __wio);
	return;
}

static void
deinit_watchers(EV_P_ int s)
{
	if (s < 0) {
		return;
	}

        /* initialise an io watcher, then start it */
        ev_io_stop(EV_A_ __wio);

	/* properly shut the socket */
	__shut_sock(s);
	return;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG(MOD_PRE ": loading ...");
	/* connect to scscp and say ehlo */
	oqsock = listener();
	/* set up the IO watcher and timer */
	init_watchers(ctx->mainloop, oqsock);
	UD_DBGCONT("loaded\n");
	return;
}

void
reinit(void *UNUSED(clo))
{
	UD_DEBUG(MOD_PRE ": reloading ...done\n");
	return;
}

void
deinit(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG(MOD_PRE ": unloading ...");
	deinit_watchers(ctx->mainloop, oqsock);
	oqsock = -1;
	UD_DBGCONT("done\n");
	return;
}

/* dso-oq.c ends here */
