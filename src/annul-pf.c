/*** example that fishes for FIX PosRpt messages and balances a portfolio */
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

struct xmpl_s {
	ud_chan_t ud_ox;
	ud_chan_t ud_pf;
	int epfd;
};

typedef struct __pos_s *__pos_t;

struct __pos_s {
	char sym[6];
	/* is qty */
	m30_t lqty;
	m30_t sqty;
	/* should-be qty */
	m30_t lqty_sb;
	m30_t sqty_sb;
};


/* list of stuff to fish for, only currencies, atm */
static struct __pos_s poss[] = {
	{
		"AUDCAD", 0, 0, 0, 0,
	}, {
		"AUDUSD", 0, 0, 0, 0,
	}, {
		"EURAUD", 0, 0, 0, 0,
	}, {
		"EURCHF", 0, 0, 0, 0,
	}, {
		"EURGBP", 0, 0, 0, 0,
	}, {
		"EURNOK", 0, 0, 0, 0,
	}, {
		"EURSEK", 0, 0, 0, 0,
	}, {
		"EURUSD", 0, 0, 0, 0,
	}, {
		"GBPUSD", 0, 0, 0, 0,
	}, {
		"NZDCHF", 0, 0, 0, 0,
	}, {
		"NZDUSD", 0, 0, 0, 0,
	}, {
		"USDCAD", 0, 0, 0, 0,
	}, {
		"USDCHF", 0, 0, 0, 0,
	}, {
		"USDDKK", 0, 0, 0, 0,
	}, {
		"USDNOK", 0, 0, 0, 0,
	}, {
		"USDSEK", 0, 0, 0, 0,
	},
};

static __pos_t
find_pos_by_name(const char *sym)
{
	for (size_t i = 0; i < countof(poss); i++) {
		__pos_t p = poss + i;
		if (strcmp(p->sym, sym) == 0) {
			return p;
		}
	}
	return NULL;
}


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
/* unsermarkt/FIX PosRpt messages */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))

static unsigned int pno = 0;

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

	for (size_t i = 0; i < countof(poss); i++) {
		udpc_seria_add_ui16(ser, i + 1);
		udpc_seria_add_str(ser, poss[i].sym, sizeof(poss->sym));
	}
	ud_chan_send_ser(ctx->ud_ox, ser);
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
/* process a match message */
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

	/* update the should-be side */
	;
	return;
}

static void
pr_pos_rpt(char *buf, size_t bsz)
{
	fprintf(stdout, "POSRPT\t\n");
	return;
}

static void
muca_cb(const struct xmpl_s *ctx, int s)
{
	static char buf[UDPC_PKTLEN];
	union ud_sockaddr_u sa;
	socklen_t ss = sizeof(sa);
	ssize_t nrd;

	if ((nrd = recvfrom(s, buf, sizeof(buf), 0, &sa.sa, &ss)) <= 0) {
		return;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, buf})) {
		return;
	}
	/* otherwise decipher it */
	switch (udpc_pkt_cmd((ud_packet_t){nrd, buf})) {
	case UMM: {
		struct umm_pair_s mmp[1];
		struct udpc_seria_s ser[1];

		udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		while (udpc_seria_des_umm(mmp, ser) >= 0) {
			pr_match(mmp);
		}
		break;
	}
	case POS_RPT:
	case POS_RPT_RPL: {
		/* great, fix message parsing */
		pr_pos_rpt(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		break;
	}
	default:
		break;
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

	while (1) {
		struct timeval tv[2];
		struct epoll_event ev[1];
		int slp = 1000;

		gettimeofday(tv + 0, NULL);
		while (epoll_wait(ctx->epfd, ev, 1, slp)) {
			if (ev->events & EPOLLIN) {
				muca_cb(ctx, ev->data.fd);
			}
			/* compute the new waiting time */
			gettimeofday(tv + 1, NULL);
			if ((slp = tv_diff(tv + 0, tv + 1)) < 1000) {
				slp = 1000 - slp;
			} else {
				slp = 0;
			}
		}

		fprintf(stderr, "BANG\n");
	}
	/* not reached */
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
	ctx->ud_ox = ud_chan_init(port);
	/* we should have an option for this as well, innit? */
	ctx->ud_pf = ud_chan_init(port + 1);

	/* also accept connections on that socket and the mcast network */
	if ((ctx->epfd = epoll_create(2)) < 0) {
		perror("cannot instantiate epoll on um-xmit socket");
		res = 1;
		goto out;
	} else {
		struct epoll_event ev[1];
		int s;

		ev->events = EPOLLIN;
		ev->data.fd = ctx->ud_ox->sock;
		epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ev->data.fd, ev);

		if ((s = ud_chan_init_mcast(ctx->ud_ox)) < 0) {
			perror("cannot instantiate mcast listener");
			res = 1;
			goto out;
		} else {
			ev->events = EPOLLIN;
			ev->data.fd = s;
			epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, s, ev);
		}

		ev->events = EPOLLIN;
		ev->data.fd = ctx->ud_pf->sock;
		epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ev->data.fd, ev);

		if ((s = ud_chan_init_mcast(ctx->ud_pf)) < 0) {
			perror("cannot instantiate mcast listener");
			res = 1;
			goto out;
		} else {
			ev->events = EPOLLIN;
			ev->data.fd = s;
			epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, s, ev);
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

	/* undeclare interest */
	{
		epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->ud_ox->sock, NULL);
		epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->ud_ox->mcfd, NULL);
		epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->ud_pf->sock, NULL);
		epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->ud_pf->mcfd, NULL);
	}

	/* close epoll */
	close(ctx->epfd);
out:
	/* and lose the handle again */
	ud_chan_fini(ctx->ud_ox);
	ud_chan_fini(ctx->ud_pf);
	return res;
}

/* annul-pf.c ends here */
