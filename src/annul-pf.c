/*** example that fishes for FIX PosRpt messages and balances a portfolio */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <math.h>
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

#include "ccy-graph.h"

struct xmpl_s {
	ud_chan_t ud_ox;
	ud_chan_t ud_pf;
	int epfd;
};

typedef struct __pos_s *__pos_t;

struct __pos_s {
	char sym[8];
	/* is qty */
	double lqty;
	double sqty;
};


/* list of stuff to fish for, only currencies, atm */
static struct __pos_s poss[] = {
	{
		"AUDCAD", 0, 0,
	}, {
		"AUDCHF", 0, 0,
	}, {
		"AUDUSD", 0, 0,
	}, {
		"EURAUD", 0, 0,
	}, {
		"EURCAD", 0, 0,
	}, {
		"EURCHF", 0, 0,
	}, {
		"EURGBP", 0, 0,
	}, {
		"EURNOK", 0, 0,
	}, {
		"EURSEK", 0, 0,
	}, {
		"EURUSD", 0, 0,
	}, {
		"GBPUSD", 0, 0,
	}, {
		"NZDCHF", 0, 0,
	}, {
		"NZDUSD", 0, 0,
	}, {
		"USDCAD", 0, 0,
	}, {
		"USDCHF", 0, 0,
	}, {
		"USDDKK", 0, 0,
	}, {
		"USDNOK", 0, 0,
	}, {
		"USDSEK", 0, 0,
	},
	/* just currencies from here */
	{
		"AUD", 0, 0,
	}, {
		"CAD", 0, 0,
	}, {
		"CHF", 0, 0,
	}, {
		"DKK", 0, 0,
	}, {
		"EUR", 0, 0,
	}, {
		"GBP", 0, 0,
	}, {
		"JPY", 0, 0,
	}, {
		"NOK", 0, 0,
	}, {
		"NZD", 0, 0,
	}, {
		"SEK", 0, 0,
	}, {
		"USD", 0, 0,
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
	static unsigned int pno = 0;
	struct udpc_seria_s ser[1];
	static char buf[UDPC_PKTLEN];

#define PKT(x)		(ud_packet_t){sizeof(x), x}
	udpc_make_pkt(PKT(buf), 0, pno++, QMETA_RPL);
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)));

	for (size_t i = 0; i < countof(poss); i++) {
		udpc_seria_add_ui16(ser, i + 1);
		udpc_seria_add_str(ser, poss[i].sym, strlen(poss[i].sym));
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

static double
calc_qty(double pos)
{
#define LOT_SIZE	(100000.0)
	double res = fmod(pos / LOT_SIZE, 1.0);
	if (res < 0.60000) {
		res += 1.0;
	}
	return res;
}

static udpc_seria_t
check_poss(const struct xmpl_s *UNUSED(ctx))
{
	static unsigned int pno = 0;
	static char buf[UDPC_PKTLEN];
	static struct udpc_seria_s ser[1];
	struct timeval now[1];
	struct sl1t_s t[1];

	/* set up the serialisation struct, just in case */
	udpc_make_pkt((ud_packet_t){sizeof(buf), buf}, 1, pno++, UTE);
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PLLEN);

	/* start setting */
	gettimeofday(now, NULL);
	sl1t_set_stmp_sec(t, now->tv_sec);
	sl1t_set_stmp_msec(t, now->tv_usec / 1000);
	for (size_t i = 0; i < countof(poss); i++) {
		__pos_t pos = poss + i;

		/* just in case */
		sl1t_set_tblidx(t, i + 1);
		t->pri = SL1T_PRC_MKT;

		if (pos->lqty != 0.0) {
			double qty = calc_qty(pos->lqty);

			fprintf(stderr, "\
ORDER\t%s\tSELL\t%.8f\t% 12.0f\n", pos->sym, qty, pos->lqty);
			sl1t_set_ttf(t, SL1T_TTF_ASK);

			t->qty = ffff_m30_get_d(qty).u;
			udpc_seria_add_scom(ser, AS_SCOM(t), sizeof(*t));
		}
		if (pos->sqty != 0.0) {
			double qty = calc_qty(pos->sqty);

			fprintf(stderr, "\
ORDER\t%s\tBUY\t%.8f\t% 12.0f\n", pos->sym, qty, pos->sqty);
			sl1t_set_ttf(t, SL1T_TTF_BID);

			t->qty = ffff_m30_get_d(qty).u;
			udpc_seria_add_scom(ser, AS_SCOM(t), sizeof(*t));
		}
	}

	if (udpc_seria_msglen(ser)) {
		return ser;
	}
	return NULL;
}


static size_t
find_fix_fld(char **p, char *msg, const char *key)
{
#define SOH	"\001"
	char *cand = msg - 1;
	char *eocand;

	while ((cand = strstr(cand + 1, key)) &&
	       cand != msg && cand[-1] != *SOH);
	/* cand should be either NULL or point to the key */
	if (UNLIKELY(cand == NULL)) {
		return 0;
	}
	/* search for the next SOH */
	if (UNLIKELY((eocand = strchr(cand, *SOH)) == NULL)) {
		return 0;
	}
	*p = cand;
	return eocand - cand;
}

static char*
find_fix_eofld(char *msg, const char *key)
{
#define SOHC	'\001'
#define SOH	"\001"
	char *cand;
	size_t clen;

	if (UNLIKELY((clen = find_fix_fld(&cand, msg, key)) == 0)) {
		return NULL;
	} else if (UNLIKELY(cand[++clen] == '\0')) {
		return NULL;
	}
	return cand + clen;
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
	static const char fix_pos_rpt[] = "35=AP";
	static const char fix_chksum[] = "10=";
	static const char fix_inssym[] = "55=";
	static const char fix_lqty[] = "704=";
	static const char fix_sqty[] = "705=";

	for (char *p = buf, *ep = buf + bsz;
	     p && p < ep && (p = find_fix_eofld(p, fix_pos_rpt));
	     p = find_fix_eofld(p, fix_chksum)) {
		__pos_t pos = NULL;
		size_t tmp;
		char *sym;
		char *lqty;
		char *sqty;

		if (find_fix_fld(&sym, p, fix_inssym)) {
			/* go behind the = */
			char ccy[8] = {0};

			memcpy(ccy, sym += sizeof(fix_inssym) - 1, 3);
			switch (*(sym += 3)) {
			case '.':
			case '/':
				sym++;
				break;
			case SOHC:
				goto f;
			default:
				break;
			}
			memcpy(ccy + 3, sym, 3);
	f:
			pos = find_pos_by_name(ccy);
		}
		if (UNLIKELY(pos == NULL)) {
			continue;
		}
		if ((tmp = find_fix_fld(&lqty, p, fix_lqty))) {
			char bkup = lqty[tmp];
			const char *cursor = lqty + sizeof(fix_lqty) - 1;

			lqty[tmp] = '\0';
			pos->lqty = strtod(cursor, NULL);
			lqty[tmp] = bkup;
		}
		if ((tmp = find_fix_fld(&sqty, p, fix_sqty))) {
			char bkup = sqty[tmp];
			const char *cursor = sqty + sizeof(fix_sqty) - 1;

			sqty[tmp] = '\0';
			pos->sqty = strtod(cursor, NULL);
			sqty[tmp] = bkup;
		}

		fprintf(stdout, "POSRPT\t%s\n", pos->sym);
	}
	return;
}

static struct pair_s ccyp[64];
static struct sl1t_s ccyb[64];
static struct sl1t_s ccya[64];

static void
pr_qmeta(char *buf, size_t bsz)
{
	struct udpc_seria_s ser[1];
	uint8_t tag;

	udpc_seria_init(ser, buf, bsz);
	while (ser->msgoff < bsz && (tag = udpc_seria_tag(ser))) {
		uint16_t id;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		id = udpc_seria_des_ui16(ser);
		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (LIKELY(tag == UDPC_TYPE_STR && id < countof(ccyp))) {
			/* fuck error checking */
			const char *p;

			(void)udpc_seria_des_str(ser, &p);
			ccyp[id].bas = find_iso_4217_by_name(p);
			switch (*(p += 3)) {
			case '.':
			case '/':
				p++;
			default:
				break;
			}
			ccyp[id].trm = find_iso_4217_by_name(p);
		}
	}
	return;
}

static void
pr_quote(char *buf, size_t bsz)
{
	for (scom_t sp = (void*)buf, ep = (void*)(buf + bsz);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t ttf = scom_thdr_ttf(sp);
		uint16_t id = scom_thdr_tblidx(sp);
		const_sl1t_t l1t;

		l1t = (const void*)sp;
		switch (ttf) {
		case SL1T_TTF_BID:
			ccyb[id] = *l1t;
			break;
		case SL1T_TTF_ASK:
			ccya[id] = *l1t;
			break;
		case SL1T_TTF_TRA:
		case SL1T_TTF_FIX:
		case SL1T_TTF_STL:
		case SL1T_TTF_AUC:
			break;
		case SL1T_TTF_VOL:
		case SL1T_TTF_VPR:
		case SL1T_TTF_OI:
			break;

			/* snaps */
		case SSNP_FLAVOUR:
		case SBAP_FLAVOUR:
			break;

			/* candles */
		case SL1T_TTF_BID | SCOM_FLAG_LM:
		case SL1T_TTF_ASK | SCOM_FLAG_LM:
		case SL1T_TTF_TRA | SCOM_FLAG_LM:
		case SL1T_TTF_FIX | SCOM_FLAG_LM:
		case SL1T_TTF_STL | SCOM_FLAG_LM:
		case SL1T_TTF_AUC | SCOM_FLAG_LM:
			break;

		case SCOM_TTF_UNK:
		default:
			break;
		}
	}
	return;
}


static void
muca_cb(const struct xmpl_s *UNUSED(ctx), int s)
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
	case QMETA_RPL:
		pr_qmeta(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		break;
	case UTE:
		/* ooooh a quote, how lovely */
		pr_quote(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		break;
	case POS_RPT:
	case POS_RPT_RPL: {
		/* great, fix message parsing */
		pr_pos_rpt(UDPC_PAYLOAD(buf), UDPC_PAYLLEN(nrd));
		buf[nrd] = '\0';
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
	while (1) {
		struct timeval tv[2];
		struct epoll_event ev[4];
		udpc_seria_t ser;
		int slp = 1000;
		int nwt;

		gettimeofday(tv + 0, NULL);
		while ((nwt = epoll_wait(ctx->epfd, ev, countof(ev), slp))) {
			for (int i = 0; i < nwt; i++) {
				if (ev[i].events & EPOLLIN) {
					muca_cb(ctx, ev[i].data.fd);
				}
			}
			/* compute the new waiting time */
			gettimeofday(tv + 1, NULL);
			if ((slp = tv_diff(tv + 0, tv + 1)) < 1000) {
				slp = 1000 - slp;
			} else {
				slp = 0;
			}
		}

		/* check all positions */
		if ((ser = check_poss(ctx))) {
			/* first off announce ourselves */
			shout_syms(ctx);
			ud_chan_send_ser(ctx->ud_ox, ser);
		}

		fprintf(stderr, "BANG\n");
	}
	/* not reached */
}


int
main(int UNUSED(argc), char *UNUSED(argv[]))
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
