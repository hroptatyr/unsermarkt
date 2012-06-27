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
#define DEFINE_GORY_STUFF
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
#include "match.h"
#include "nifty.h"

#define MAYBE_UNUSED	__attribute__((unused))

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
	"GBPUSD",
	"USDNOK",
	"USDSEK",
	"USDCHF",
	"AUDUSD",
	"USDCAD",
	"EURUSD",
	"EURCHF",
	"NZDUSD",
	"EURGBP",
	"EURAUD",
	"EURSEK",
	"AUDCHF",
	"AUDCAD",
	"EURNOK",

#define FIRST_SELL	16
	"GBPCAD",
	"EURCAD",
	"GBPNOK",
	"USDDKK",
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

static int
tv_diff(struct timeval *t1, struct timeval *t2)
{
/* in milliseconds */
	useconds_t res = (t2->tv_sec - t1->tv_sec) * 1000;
	res += (t2->tv_usec - t1->tv_usec) / 1000;
	return res;
}

static int
__umm_p(ud_packet_t pkt)
{
	return udpc_pkt_cmd(pkt) == UMM;
}

static int
udpc_seria_des_umm(umm_pair_t mmp, udpc_seria_t ser)
{
	if (ser->msgoff + sizeof(*mmp) > ser->len) {
		return -1;
	}
	memcpy(mmp, ser->msg + ser->msgoff, sizeof(*mmp));
	ser->msgoff += sizeof(*mmp);
	return 0;
}

static void
pr_match(umm_pair_t mmp)
{
	char buyer[INET6_ADDRSTRLEN];
	char seller[INET6_ADDRSTRLEN];
	char prc[32];
	char qty[32];
	short unsigned int bport;
	short unsigned int sport;

	ffff_m30_s(prc, ffff_m30_get_ui32(mmp->l1->pri));
	ffff_m30_s(qty, ffff_m30_get_ui32(mmp->l1->qty));
	inet_ntop(AF_INET6, &mmp->agt[0].addr, buyer, sizeof(buyer));
	inet_ntop(AF_INET6, &mmp->agt[1].addr, seller, sizeof(seller));
	bport = ntohs(mmp->agt[0].port);
	sport = ntohs(mmp->agt[1].port);

	fprintf(stdout, "MATCH\tB:[%s]:%hu+%hu\tS:[%s]:%hu+%hu\t%s\t%s\n",
		buyer, bport, mmp->agt[0].uidx,
		seller, sport, mmp->agt[1].uidx,
		prc, qty);
	return;
}

static void
muca_cb(int s)
{
	static char buf[UDPC_PKTLEN];
	union ud_sockaddr_u sa;
	socklen_t ss = sizeof(sa);
	ssize_t nrd;

	if ((nrd = recvfrom(s, buf, sizeof(buf), 0, &sa.sa, &ss)) <= 0) {
		return;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, buf})) {
		return;
	} else if (!__umm_p((ud_packet_t){nrd, buf})) {
		return;
	}
	/* otherwise decipher it */
	{
		struct umm_pair_s mmp[1];
		struct udpc_seria_s ser[1];

		udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		while (udpc_seria_des_umm(mmp, ser) >= 0) {
			pr_match(mmp);
		}
	}
	return;
}


static jmp_buf jb;
#define work		work_all

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

static void MAYBE_UNUSED
work_all(const struct xmpl_s *ctx)
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
	for (size_t k = 0;; k++) {
		struct sl1t_s t[1];

		/* first of all announce ourselves */
		if ((k % 10) == 0) {
			shout_syms(ctx);
		}
		RESET_SER;
		gettimeofday(now, NULL);
		sl1t_set_stmp_sec(t, now->tv_sec);
		sl1t_set_stmp_msec(t, 0);
		sl1t_set_ttf(t, SL1T_TTF_BID);
		for (size_t i = 1; i <= countof(syms); i++) {
			sl1t_set_tblidx(t, i);
			t->pri = SL1T_PRC_MKT;
			t->qty = ffff_m30_get_d(1.0).u;

			if (i == FIRST_SELL) {
				sl1t_set_ttf(t, SL1T_TTF_ASK);
			}

			udpc_seria_add_scom(ser, AS_SCOM(t), sizeof(*t));
		}
		fprintf(stderr, "BANG\n");
		ud_chan_send_ser(ctx->ud, ser);

		{
			struct timeval tv[2];
			struct epoll_event ev[1];
			int slp = 1000;

			gettimeofday(tv + 0, NULL);
			while (epoll_wait(ctx->epfd, ev, 1, slp)) {
				if (ev->events & EPOLLIN) {
					muca_cb(ev->data.fd);
				}
				/* compute the new waiting time */
				gettimeofday(tv + 1, NULL);
				if ((slp = tv_diff(tv + 0, tv + 1)) < 1000) {
					slp = 1000 - slp;
				} else {
					slp = 0;
				}
			}
		}
	}
	/* not reached */
}

static void MAYBE_UNUSED
work_EURUSD(const struct xmpl_s *ctx)
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
	for (size_t k = 0; k < -1UL; k++) {
		struct sl1t_s t[1];

		/* first of all announce ourselves */
		shout_syms(ctx);
		RESET_SER;
		gettimeofday(now, NULL);
		sl1t_set_stmp_sec(t, now->tv_sec);
		sl1t_set_stmp_msec(t, 0);
		sl1t_set_ttf(t, SL1T_TTF_BID);
		{
			size_t i = 7;
			sl1t_set_tblidx(t, i);
			t->pri = SL1T_PRC_MKT;
			t->qty = ffff_m30_get_d(1.0).u;

			if (i == FIRST_SELL) {
				sl1t_set_ttf(t, SL1T_TTF_ASK);
			}

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
