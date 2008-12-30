/*** unserding-tcp6.c -- tcp6 handlers
 *
 * Copyright (C) 2008 Sebastian Freundt
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_NETDB_H
# include <netdb.h>
#elif defined HAVE_LWRES_NETDB_H
# include <lwres/netdb.h>
#endif	/* NETDB */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
/* our master include */
#include "unserding.h"
#include "unserding-private.h"

#define TCPUDP_TIMEOUT		60

static int lsock __attribute__((used));
static ev_io __srv_watcher __attribute__((aligned(16)));

static inline void
__reuse_sock(int sock)
{
	const int one = 1;

#if defined SO_REUSEADDR
	UD_DEBUG_TCPUDP("setting option SO_REUSEADDR for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		UD_CRITICAL_TCPUDP("setsockopt(SO_REUSEADDR) failed\n");
	}
#else
# error "Go away!"
#endif
#if defined SO_REUSEPORT
	UD_DEBUG_TCPUDP("setting option SO_REUSEPORT for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
		UD_CRITICAL_TCPUDP("setsockopt(SO_REUSEPORT) failed\n");
	}
#endif
	return;
}

static inline void
__linger_sock(int sock)
{
#if defined SO_LINGER
	struct linger lng;

	lng.l_onoff = 1;	/* 1 == on */
	lng.l_linger = 1;	/* linger time in seconds */

	UD_DEBUG_TCPUDP("setting option SO_LINGER for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) < 0) {
		UD_CRITICAL_TCPUDP("setsockopt(SO_LINGER) failed\n");
	}
#else
# error "Go away"
#endif
	return;
}

static int
_tcpudp_listener_try(volatile struct addrinfo *lres)
{
	volatile int s;
	int retval;
	char servbuf[NI_MAXSERV];

	s = socket(lres->ai_family, SOCK_STREAM, 0);
	if (s < 0) {
		UD_CRITICAL_TCPUDP("socket() failed, whysoever\n");
		return s;
	}
	__reuse_sock(s);

	/* we used to retry upon failure, but who cares */
	retval = bind(s, lres->ai_addr, lres->ai_addrlen);
	if (retval >= 0 ) {
		retval = listen(s, 5);
	}
	if (UNLIKELY(retval == -1)) {
		UD_CRITICAL_TCPUDP("bind() failed, whysoever\n");
		if (errno != EISCONN) {
			close(s);
			return -1;
		}
	}

	if (getnameinfo(lres->ai_addr, lres->ai_addrlen, NULL,
			0, servbuf, sizeof(servbuf), NI_NUMERICSERV) == 0) {
		UD_DEBUG_TCPUDP("listening on port %s\n", servbuf);
	}
	return s;
}

#if defined HAVE_GETADDRINFO
static int
tcpudp_listener_init(void)
{
	struct addrinfo *res;
	const struct addrinfo hints = {
		/* allow both v6 and v4 */
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		/* specify to whom we listen */
		.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ALL | AI_NUMERICHOST,
		/* naught out the rest */
		.ai_canonname = NULL,
		.ai_addr = NULL,
		.ai_next = NULL,
	};
	int retval;
	volatile int s;

	retval = getaddrinfo(NULL, UD_NETWORK_SERVICE, &hints, &res);
	if (retval != 0) {
		/* abort(); */
		return -1;
	}
	for (struct addrinfo *lres = res; lres; lres = lres->ai_next) {
		if ((s = _tcpudp_listener_try(lres)) >= 0) {
			UD_DEBUG_TCPUDP(":sock %d  :proto %d :host %s\n",
					s, lres->ai_protocol,
					lres->ai_canonname);
			break;
		}
	}

	freeaddrinfo(res);
	/* succeeded if > 0 */
	return s;
}
#else  /* !GETADDRINFO */
# error "Listen bloke, need getaddrinfo() bad, give me one or I'll stab myself."
#endif

static void
tcpudp_listener_deinit(int sock)
{
	/* linger the sink sock */
	__linger_sock(sock);
	UD_DEBUG_TCPUDP("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
tcpudp_kick_ctx(EV_P_ conn_ctx_t ctx)
{
	UD_DEBUG_TCPUDP("kicking ctx %p :socket %d...\n", ctx, ctx->snk);
	/* kick the timer */
	ev_timer_stop(EV_A_ ctx_timer(ctx));
	/* kick the io handlers */
	ev_io_stop(EV_A_ ctx_rio(ctx));
	ev_io_stop(EV_A_ ctx_wio(ctx));
	/* stop the bugger */
	tcpudp_listener_deinit(ctx->snk);
	/* lock and destroy the mutex */
	ctx_lock_ibuf(ctx);
	ctx_unlock_ibuf(ctx);
	pthread_mutex_unlock(&ctx->ibuf_mtx);
	/* finally, give the ctx struct a proper rinse */
	ctx->src = ctx->snk = -1;
	ctx->bidx = 0;
	return;
}

static const char idle_msg[] = "Oi shitface, stop wasting my time, will ya!\n";

static void
tcpudp_idleto_cb(EV_P_ ev_timer *w, int revents)
{
	/* our brilliant type pun */
	conn_ctx_t ctx = ev_timer_ctx(w);
	ev_tstamp now = ev_now (EV_A);

	if (ctx->timeout < now) {
		/* yepp, timed out */
		UD_DEBUG_TCPUDP("bitching back at the eejit on %d\n", ctx->snk);
		write(ctx->snk, idle_msg, countof(idle_msg)-1);
		tcpudp_kick_ctx(loop, ctx);
	} else {
		/* callback was invoked, but there was some activity, re-arm */
		w->repeat = ctx->timeout - now;
		ev_timer_again(EV_A_ w);
	}
	return;
}

static inline void
tcpudp_ctx_renew_timer(EV_P_ conn_ctx_t ctx)
{
	ctx->timeout = ev_now(EV_A) + TCPUDP_TIMEOUT;
	return;
}


/* this callback is called when data is readable on one of the polled socks */
static void
tcpudp_traf_rcb(EV_P_ ev_io *w, int revents)
{
	size_t nread;
	/* our brilliant type pun */
	struct conn_ctx_s *ctx = (void*)w;

	UD_DEBUG_TCPUDP("traffic on %d\n", w->fd);
	ctx_lock_ibuf(ctx);
	nread = read(w->fd, &ctx->buf[ctx->bidx],
		     CONN_CTX_BUF_SIZE - ctx->bidx);
	if (UNLIKELY(nread == 0)) {
		tcpudp_kick_ctx(EV_A_ ctx);
		return;
	}
	/* prolongate the timer a wee bit */
	tcpudp_ctx_renew_timer(EV_A_ ctx);
	/* wind the buffer index */
	ctx->bidx += nread;
	/* quickly check if we've seen the \n\n sequence */
	if (ctx->buf[ctx->bidx-2] == '\n' &&
	    ctx->buf[ctx->bidx-1] == '\n') {
		/* be generous with the connexion timeout now, 1 day */
		ctx->timeout = ev_now(EV_A) + 1440*TCPUDP_TIMEOUT;
		/* enqueue t3h job and notify the slaves */
		enqueue_job(glob_jq, ud_parse, ctx);
		trigger_job_queue();
	} else {
		/* we aren't finished so unlock the buffer again */
		ctx_unlock_ibuf(ctx);
	}
	return;
}

/* this callback is called when data is writable on one of the polled socks */
static void __attribute__((unused))
tcpudp_traf_wcb(EV_P_ ev_io *w, int revents)
{
	conn_ctx_t ctx = ev_wio_ctx(w);

	UD_DEBUG_TCPUDP("writing buffer of length %lu to %d\n",
			ctx->obuflen, ctx->snk);
	if (LIKELY(ctx->obufidx < ctx->obuflen)) {
		const char *buf = ctx->obuf + ctx->obufidx;
		size_t blen = ctx->obuflen - ctx->obufidx;
		/* the actual write */
		ctx->obufidx += write(w->fd, buf, blen);
	}
	if (LIKELY(ctx->obufidx >= ctx->obuflen)) {
		/* if nothing's to be printed just turn it off */
		ev_io_stop(EV_A_ w);
		/* reset the timeout, we want activity on the remote side now */
		ctx->timeout = ev_now(EV_A) + TCPUDP_TIMEOUT;
	}
	return;
}

/* this callback is called when data is readable on the main server socket */
static void
tcpudp_inco_cb(EV_P_ ev_io *w, int revents)
{
	volatile int ns;
	struct sockaddr_in6 sa;
	socklen_t sa_size = sizeof(sa);
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	conn_ctx_t lctx;
	ev_io *watcher;
	ev_timer *to;

	UD_DEBUG_TCPUDP("incoming connection\n");

	ns = accept(w->fd, (struct sockaddr *)&sa, &sa_size);
	if (ns < 0) {
		UD_CRITICAL_TCPUDP("could not handle incoming connection\n");
		return;
	}
	/* obtain the address in human readable form */
	a = inet_ntop(sa.sin6_family, &sa.sin6_addr, buf, sizeof(buf));
	p = ntohs(sa.sin6_port);

	UD_DEBUG_TCPUDP("Server: connect from host %s, port %d.\n", a, p);

	/* initialise an io watcher, then start it */
	lctx = find_ctx();

	/* escrow the socket poller */
	watcher = ctx_rio(lctx);
	ev_io_init(watcher, tcpudp_traf_rcb, ns, EV_READ);
	ev_io_start(EV_A_ watcher);

	/* escrow the writer */
	watcher = ctx_wio(lctx);
	ev_io_init(watcher, tcpudp_traf_wcb, ns, EV_WRITE);

	/* escrow the connexion idle timeout */
	lctx->timeout = ev_now(EV_A) + TCPUDP_TIMEOUT;
	to = ctx_timer(lctx);
	ev_timer_init(to, tcpudp_idleto_cb, 0.0, TCPUDP_TIMEOUT);
	ev_timer_again(EV_A_ to);

	/* initialise the input buffer */
	pthread_mutex_init(&lctx->ibuf_mtx, NULL);
	lctx->bidx = 0;
	/* put the src and snk sock into glob_ctx */
	lctx->src = w->fd;
	lctx->snk = ns;
	return;
}


void
ud_print_tcp6(EV_P_ conn_ctx_t ctx, const char *m, size_t mlen)
{
#if 0
/* simplistic approach */
	write(ctx->snk, m, mlen);
#else
/* callback approach */
	ctx->obuflen = mlen;
	ctx->obufidx = 0;
	ctx->obuf = m;

	/* start the write watcher */
	ev_io_start(EV_A_ ctx_wio(ctx));
	trigger_evloop(EV_A);
#endif
}

void
ud_kick_tcp6(EV_P_ conn_ctx_t ctx)
{
	tcpudp_kick_ctx(EV_A_ ctx);
	return;
}

int
ud_attach_tcp6(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;

	/* get us a global sock */
	lsock = tcpudp_listener_init();

	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, tcpudp_inco_cb, lsock, EV_READ);
	ev_io_start(EV_A_ srv_watcher);
	return 0;
}

int
ud_detach_tcp6(EV_P)
{
	tcpudp_listener_deinit(lsock);
	return 0;
}

/* unserding-tcp6.c ends here */
