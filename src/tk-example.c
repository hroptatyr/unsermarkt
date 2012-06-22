/*** just to show tilman */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <sys/epoll.h>
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#include <unserding/unserding.h>
#include <unserding/protocore.h>
#include "nifty.h"

struct xmpl_s {
	ud_chan_t ud;
	int epfd;
	int mcfd;
};


#define UTE_LE		(0x7574)
#define UTE_BE		(0x5554)
#define QMETA		(0x7572)
#define QMETA_RPL	(UDPC_PKT_RPL(QMETA))
#if defined WORDS_BIGENDIAN
# define UTE		UTE_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE		UTE_LE
#endif	/* WORDS_BIGENDIAN */
#define UTE_RPL		(UDPC_PKT_RPL(UTE))
/* unsermarkt match messages */
#define UMM		(0x7576)
#define UMM_RPL		(UDPC_PKT_RPL(UMM))

static unsigned int pno = 0;

static const char syms[][6] = {
	"EURUSD",
	"GBPUSD",
	"USDJPY",
	"USDCHF",
};

static inline void
udpc_seria_add_scom(udpc_seria_t sctx, scom_t s, size_t len)
{
	memcpy(sctx->msg + sctx->msgoff, s, len);
	sctx->msgoff += len;
	return;
}

static void
shout_syms(const struct xmpl_s *ctx)
{
	struct udpc_seria_s ser[1];
	static char buf[UDPC_PKTLEN];

#define PKT(x)		(ud_packet_t){sizeof(x), x}
	udpc_make_pkt(PKT(buf), 0, pno++, QMETA_RPL);
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)));

	for (size_t i = 1; i <= countof(syms); i++) {
		udpc_seria_add_ui16(ser, i);
		udpc_seria_add_str(ser, syms[i - 1], sizeof(*syms));
	}
	ud_chan_send_ser(ctx->ud, ser);
	return;
}


static jmp_buf jb;

static void
handle_sigint(int signum)
{
	longjmp(jb, signum);
	return;
}

static int
pre_work(const struct xmpl_s *UNUSED(ctx))
{
	return 0;
}

static int
post_work(const struct xmpl_s *UNUSED(ctx))
{
	return 0;
}

static void
work(const struct xmpl_s *ctx)
{
/* generate market orders */
	struct udpc_seria_s ser[1];
	static char buf[UDPC_PKTLEN];
	static ud_packet_t pkt = {0, buf};
	struct timeval now[1];

#define RESET_SER						\
	udpc_make_pkt(pkt, 0, pno++, UTE);			\
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PLLEN)

	/* just a few rounds of market orders */
	for (size_t k = 0; k < 120; k++) {
		struct sl1t_s t[1];

		/* first of all announce ourselves */
		shout_syms(ctx);
		RESET_SER;
		gettimeofday(now, NULL);
		sl1t_set_stmp_sec(t, now->tv_sec);
		sl1t_set_stmp_msec(t, 0);
		sl1t_set_ttf(t, SL1T_TTF_BID);
		for (size_t i = 1; i <= countof(syms); i++) {
			sl1t_set_tblidx(t, i);
			t->pri = SL1T_PRC_MKT;
			t->qty = ffff_m30_get_d(10.0).u;

			udpc_seria_add_scom(ser, AS_SCOM(t), sizeof(*t));
		}
		ud_chan_send_ser(ctx->ud, ser);
		sleep(5);
	}

	return;
}


int
main(int argc, char *argv[])
{
	short unsigned int port = 4942;
	struct xmpl_s ctx[1];
	int res = 0;

	/* set signal handler */
	signal(SIGINT, handle_sigint);

	/* obtain a new handle, somehow we need to use the port number innit? */
	ctx->ud = ud_chan_init(port);

	/* also accept connections on that socket and the mcast network */
	if ((ctx->epfd = epoll_create(2)) < 0) {
		perror("cannot instantiate epoll on um-xmit socket");
		res = 1;
		goto out;
	} else {
		struct epoll_event ev[1];

		ev->events = EPOLLIN;
		ev->data.fd = ctx->ud->sock;
		epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->ud->sock, ev);

		if ((ctx->mcfd = ud_mcast_init(port)) < 0) {
			perror("cannot instantiate mcast listener");
			res = 1;
			goto out;
		} else {
			ev->events = EPOLLIN;
			ev->data.fd = ctx->mcfd;
			epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->mcfd, ev);
		}
	}

	/* the actual work */
	switch (setjmp(jb)) {
	case 0:
		if (pre_work(ctx) == 0) {
			/* do the actual work */
			work(ctx);
		}
	case SIGINT:
	default:
		if (post_work(ctx) < 0) {
			res = 1;
		}
		break;	
	}


	/* close epoll */
	close(ctx->epfd);
out:
	/* and lose the handle again */
	ud_chan_fini(ctx->ud);
	return res;
}

/* tk-example.c ends here */
