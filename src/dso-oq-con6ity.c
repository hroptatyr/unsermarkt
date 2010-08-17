/*** just to focus on the essential stuff in the dso-oq module */

#include <ev.h>
#undef EV_P
#define EV_P	struct ev_loop *loop __attribute__((unused))

#define UM_PORT		(12768)

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
	UD_DEBUG(MOD_PRE ": new data in sock %d\n", w->fd);
	if (handle_data(w->fd, buf, nrd) < 0) {
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

/* dso-oq-connectivity.c ends here */