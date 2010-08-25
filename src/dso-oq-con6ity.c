/*** just to focus on the essential stuff in the dso-oq module */
#include <unistd.h>
#include <fcntl.h>
#include <ev.h>
#include "nifty.h"

#undef EV_P
#define EV_P	struct ev_loop *loop __attribute__((unused))

#define UM_PORT		(12768)

#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

/* connection mumbo-jumbo */
static int oqsock;
static ev_io __wio[1];

static void
__shut_sock(int s)
{
	shutdown(s, SHUT_RDWR);
	close(s);
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

static inline int
getsockopt_int(int s, int level, int optname)
{
	int res[1];
	socklen_t rsz = sizeof(*res);
	if (getsockopt(s, level, optname, res, &rsz) >= 0) {
		return *res;
	}
	return -1;
}

static inline int
setsockopt_int(int s, int level, int optname, int value)
{
	return setsockopt(s, level, optname, &value, sizeof(value));
}

/**
 * Mark address behind socket S as reusable. */
static inline int
setsock_reuseaddr(int s)
{
#if defined SO_REUSEADDR
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEADDR, 1);
#else  /* !SO_REUSEADDR */
	return 0;
#endif	/* SO_REUSEADDR */
}

/* probably only available on BSD */
static inline int
setsock_reuseport(int __attribute__((unused)) s)
{
#if defined SO_REUSEPORT
	return setsockopt_int(s, SOL_SOCKET, SO_REUSEPORT, 1);
#else  /* !SO_REUSEPORT */
	return 0;
#endif	/* SO_REUSEPORT */
}

/* we could take args like listen address and port number */
static int
listener(void)
{
#if defined IPPROTO_IPV6
	static struct sockaddr_in6 __sa6 = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT
	};
	volatile int s;

	/* non-constant slots of __sa6 */
	__sa6.sin6_port = htons(UM_PORT);

	if (LIKELY((s = socket(PF_INET6, SOCK_STREAM, 0)) >= 0)) {
		/* likely case upfront */
		;
	} else {
		UM_DEBUG(MOD_PRE ": socket() failed ... I'm clueless now\n");
		return s;
	}

#if defined IPV6_V6ONLY
	setsockopt_int(s, IPPROTO_IPV6, IPV6_V6ONLY, 0);
#endif	/* IPV6_V6ONLY */
#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, 1);
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_DONTFRAG, 1);
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	setsockopt_int(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, 1);
#endif
	setsock_reuseaddr(s);
	setsock_reuseport(s);

	/* we used to retry upon failure, but who cares */
	if (bind(s, (struct sockaddr*)&__sa6, sizeof(__sa6)) < 0 ||
	    listen(s, 2) < 0) {
		UM_DEBUG(MOD_PRE ": bind() failed, errno %d\n", errno);
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
		UM_DEBUG(MOD_PRE ": no data, closing socket %d %d\n", w->fd, re);
		handle_close(w->fd);
		clo_wio(EV_A_ w);
		return;
	}
	UM_DEBUG(MOD_PRE ": new data in sock %d\n", w->fd);
	if (handle_data(w->fd, buf, nrd) < 0) {
		UM_DEBUG(MOD_PRE ": negative, closing down\n");
		clo_wio(EV_A_ w);
	}
	return;
}

static void
inco_cb(EV_P_ ev_io *w, int UNUSED(re))
{
/* we're tcp so we've got to accept() the bugger, don't forget :) */
	volatile int ns;
	ev_io *aw;
	struct sockaddr_storage sa;
	socklen_t sa_size = sizeof(sa);

	UM_DEBUG(MOD_PRE ": they got back to us...");
	if ((ns = accept(w->fd, (struct sockaddr*)&sa, &sa_size)) < 0) {
		UM_DBGCONT("accept() failed\n");
		return;
	}

        /* make an io watcher and watch the accepted socket */
	aw = xnew(ev_io);
        ev_io_init(aw, data_cb, ns, EV_READ);
        ev_io_start(EV_A_ aw);
	UM_DBGCONT("success, new sock %d\n", ns);
	return;
}

static void
clo_evsock(EV_P_ int UNUSED(type), void *w)
{
	ev_io *wp = w;

        /* deinitialise the io watcher */
        ev_io_stop(EV_A_ wp);
	/* properly shut the socket */
	__shut_sock(wp->fd);
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
deinit_watchers(EV_P)
{
#if defined EV_WALK_ENABLE && EV_WALK_ENABLE
	/* properly close all sockets */
	ev_walk(EV_A_ EV_IO, clo_evsock);
#else  /* !EV_WALK_ENABLE */
	/* close the main socket at least */
	clo_evsock(EV_A_ EV_IO, __wio);
#endif	/* EV_WALK_ENABLE */
	return;
}

/* dso-oq-connectivity.c ends here */
