/*** quo-tws.c -- quotes and trades from tws
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
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
/* for gmtime_r */
#include <time.h>
/* for gettimeofday() */
#include <sys/time.h>
/* for mmap */
#include <sys/mman.h>
#include <fcntl.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/utsname.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>
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

/* the tws api */
#include "gen-tws.h"
#include "gen-tws-cont.h"
#include "quo-tws-private.h"
#include "nifty.h"
#include "strops.h"
#include "gq.h"
#include "ud-sock.h"

#include "proto-twsxml-reqtyp.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

/* only recent ute versions (>=0.2.3) have this */
#if !defined UO_NO_CREAT_TPC
# define UO_NO_CREAT_TPC	(0)
#endif	/* !UO_NO_CREAT_TPC */

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define QUO_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define QUO_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

typedef struct ctx_s *ctx_t;
typedef struct quo_qqq_s *quo_qqq_t;
typedef struct q30_s q30_t;
typedef m30_t q30_pack_t[4];

struct ctx_s {
	struct tws_s tws[1];

	/* static context */
	const char *host;
	uint16_t port;
	int client;

	/* dynamic context */
	int tws_sock;
};

struct comp_s {
	struct in6_addr addr;
	uint16_t port;
};

struct quo_qq_s {
	struct gq_s q[1];
	struct gq_ll_s sbuf[1];
};

/* indexing into the quo_sub_s->quos */
struct q30_s {
	union {
		struct {
			size_t subtyp:1;
			size_t:1;
		};
		size_t typ:2;
	};
	size_t idx:16;
};

/* the quote-queue quote, i.e. an item of the quote queue */
struct quo_qqq_s {
	struct gq_item_s i;

	/* pointer into our quotes array */
	q30_t q;
};

/* AoV-based subscriptions class */
struct quo_sub_s {
	size_t nsubs;
	struct {
		tws_cont_t cont;
		tws_sdef_t sdef;
	} *inss;

	/* actual quotes, this is bid, bsz, ask, asz  */
	q30_pack_t *quos;

	/* array of last dissemination time stamps */
	uint32_t *last_dsm;
};

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


/* the quotes array */
static inline q30_t
make_q30(uint16_t iidx, quo_typ_t t)
{
#if defined HAVE_ANON_STRUCTS_INIT
	if (LIKELY(t >= QUO_TYP_BID && t <= QUO_TYP_ASZ)) {
		return __extension__(q30_t){.typ = t - 1, .idx = iidx};
	}
	return __extension__(q30_t){0};
#else  /* !HAVE_ANON_STRUCTS_INIT */
	struct q30_s res = {0};

	if (LIKELY(t >= QUO_TYP_BID && t <= QUO_TYP_ASZ)) {
		res.typ = t - 1;
		res.idx = iidx;
	}
	return res;
#endif	/* HAVE_ANON_STRUCTS_INIT */
}

static inline uint16_t
q30_idx(q30_t q)
{
	return (uint16_t)q.idx;
}

static inline quo_typ_t
q30_typ(q30_t q)
{
	return (quo_typ_t)q.typ;
}

static inline unsigned int
q30_sl1t_typ(q30_t q)
{
	return q30_typ(q) / 2 + SL1T_TTF_BID;
}


/* networking */
static int
make_dccp(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_DCCP, IPPROTO_DCCP)) < 0) {
		return s;
	}
	/* mark the address as reusable */
	setsock_reuseaddr(s);
	/* impose a sockopt service, we just use 1 for now */
	set_dccp_service(s, 1);
	/* make a timeout for the accept call below */
	setsock_rcvtimeo(s, 1000);
	/* make socket non blocking */
	setsock_nonblock(s);
	/* turn off nagle'ing of data */
	setsock_nodelay(s);
	return s;
}

static int
make_tcp(void)
{
	int s;

	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return s;
	}
	/* reuse addr in case we quickly need to turn the server off and on */
	setsock_reuseaddr(s);
	/* turn lingering on */
	setsock_linger(s, 1);
	return s;
}

static int
sock_listener(int s, ud_sockaddr_t sa)
{
	if (s < 0) {
		return s;
	}

	if (bind(s, &sa->sa, sizeof(*sa)) < 0) {
		return -1;
	}

	return listen(s, MAX_DCCP_CONNECTION_BACK_LOG);
}


/* the actual core, ud and fix stuff */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))

/* for them quo oper queues */
#include "gq.c"

/* our beef channels */
static ev_io beef[1];

/* them top-level snapper */
static struct quo_sub_s subs = {0};

/* the sender buffer queue */
static struct quo_qq_s qq = {0};
static utectx_t uu = NULL;

#define INSTRMT(i)	(subs.inss[i - 1].cont)
#define SECDEF(i)	(subs.inss[i - 1].sdef)

/* ute services come in 2 flavours little endian "ut" and big endian "UT" */
#define UTE_CMD_LE	0x7574
#define UTE_CMD_BE	0x5554
#if defined WORDS_BIGENDIAN
# define UTE_CMD	UTE_CMD_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE_CMD	UTE_CMD_LE
#endif	/* WORDS_BIGENDIAN */

#define BRAG_INTV	(10)
#define UTE_QMETA	0x7572

static bool
udpc_seria_fits_qqq_p(udpc_seria_t ser, quo_qqq_t UNUSED(q))
{
	/* super quick check if we can afford to take it the pkt on
	 * we need roughly 16 bytes */
	if (ser->msgoff + sizeof(struct sl1t_s) > ser->len) {
		return false;
	}
	return true;
}

static bool
udpc_seria_fits_dsm_p(udpc_seria_t ser, const char *UNUSED(sym), size_t len)
{
	return udpc_seria_msglen(ser) + len + 2 + 4 <= UDPC_PLLEN;
}

static inline void
udpc_seria_add_scom(udpc_seria_t ser, scom_t s, size_t len)
{
	memcpy(ser->msg + ser->msgoff, s, len);
	ser->msgoff += len;
	return;
}

static void
ud_chan_send_ser_all(udpc_seria_t ser)
{
	ud_chan_t ch = beef->data;

	/* assume it's possible to write */
	ud_chan_send_ser(ch, ser);
	return;
}

/* looks like dccp://host:port/secdef?idx=00000 */
static char brag_uri[INET6_ADDRSTRLEN] = "dccp://";
/* offset into brag_uris idx= field */
static size_t brag_uri_offset = 0;

static int
make_brag_uri(ud_sockaddr_t sa, socklen_t UNUSED(sa_len))
{
	struct utsname uts[1];
	char dnsdom[64];
	const size_t uri_host_offs = sizeof("dccp://");
	char *curs = brag_uri + uri_host_offs - 1;
	size_t rest = sizeof(brag_uri) - uri_host_offs;
	int len;

	if (uname(uts) < 0) {
		return -1;
	} else if (getdomainname(dnsdom, sizeof(dnsdom)) < 0) {
		return -1;
	}

	len = snprintf(
		curs, rest, "%s.%s:%hu/secdef?idx=",
		uts->nodename, dnsdom, sa->sa6.sin6_port);

	if (len > 0) {
		brag_uri_offset = uri_host_offs + len - 1;
	}

	QUO_DEBUG("adv_name: %s\n", brag_uri);
	return 0;
}

static int
brag(udpc_seria_t ser, uint16_t idx)
{
	const char *sym = ute_idx2sym(uu, idx);
	size_t len, tot;

	tot = (len = strlen(sym)) + brag_uri_offset + 5;

	if (UNLIKELY(!udpc_seria_fits_dsm_p(ser, sym, tot))) {
		ud_packet_t pkt = {UDPC_PKTLEN, /*hack*/ser->msg - UDPC_HDRLEN};
		ud_pkt_no_t pno = udpc_pkt_pno(pkt);

		ud_chan_send_ser_all(ser);

		/* hack hack hack
		 * reset the packet */
		udpc_make_pkt(pkt, 0, pno + 2, UDPC_PKT_RPL(UTE_QMETA));
		ser->msgoff = 0;
	}
	/* add this guy */
	udpc_seria_add_ui16(ser, idx);
	udpc_seria_add_str(ser, sym, len);
	/* put stuff in our uri */
	len = snprintf(
		brag_uri + brag_uri_offset, sizeof(brag_uri) - brag_uri_offset,
		"%hu", idx);
	udpc_seria_add_str(ser, brag_uri, brag_uri_offset + len);
	return 0;
}

static void
flush_queue(tws_t UNUSED(tws))
{
	static size_t pno = 0;
	char buf[UDPC_PKTLEN];
	char dsm[UDPC_PKTLEN];
	struct udpc_seria_s ser[2];
	struct sl1t_s l1t[1];
	struct timeval now[1];

#define PKT(x)		((ud_packet_t){sizeof(x), x})
#define MAKE_DSM_PKT							\
	udpc_make_pkt(PKT(dsm), 0, pno++, UDPC_PKT_RPL(UTE_QMETA));	\
	udpc_seria_init(ser, UDPC_PAYLOAD(dsm), UDPC_PAYLLEN(sizeof(dsm)))
#define MAKE_PKT							\
	MAKE_DSM_PKT;							\
	udpc_make_pkt(PKT(buf), 0, pno++, UTE_CMD);			\
	udpc_set_data_pkt(PKT(buf));					\
	udpc_seria_init(ser + 1, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(sizeof(buf)))

	/* time */
	gettimeofday(now, NULL);

	/* populate l1t somewhat */
	sl1t_set_stmp_sec(l1t, now->tv_sec);
	sl1t_set_stmp_msec(l1t, now->tv_usec / 1000);

	/* get the packet ctor'd */
	MAKE_PKT;
	for (gq_item_t ip; (ip = gq_pop_head(qq.sbuf));
	     gq_push_tail(qq.q->free, ip)) {
		quo_qqq_t q = (quo_qqq_t)ip;
		uint16_t tblidx;
		unsigned int ttf;

		if (UNLIKELY(!udpc_seria_fits_qqq_p(ser + 1, q))) {
			ud_chan_send_ser_all(ser);
			ud_chan_send_ser_all(ser + 1);
			MAKE_PKT;
		}

		if ((tblidx = q30_idx(q->q)) == 0 ||
		    (ttf = q30_sl1t_typ(q->q)) == SCOM_TTF_UNK) {
			continue;
		} else if (subs.quos[tblidx - 1][q30_typ(q->q)].u == 0) {
			continue;
		}
		/* the typ was designed to coincide with ute's sl1t types */
		sl1t_set_ttf(l1t, ttf);
		sl1t_set_tblidx(l1t, tblidx);

		l1t->pri = subs.quos[tblidx - 1][q30_typ(q->q)].u;
		l1t->qty = subs.quos[tblidx - 1][q30_typ(q->q) + 1].u;

		udpc_seria_add_scom(ser + 1, AS_SCOM(l1t), sizeof(*l1t));

		/* i think it's worth checking when we last disseminated this */
		if (now->tv_sec - subs.last_dsm[tblidx - 1] >= BRAG_INTV) {
			brag(ser, tblidx);
			subs.last_dsm[tblidx - 1] = now->tv_sec;
		}
	}
	ud_chan_send_ser_all(ser);
	ud_chan_send_ser_all(ser + 1);
	return;
}


/* web services */
typedef enum {
	WEBSVC_F_UNK,
	WEBSVC_F_SECDEF,
} websvc_f_t;

struct websvc_s {
	websvc_f_t ty;

	union {
		struct {
			uint16_t idx;
		} secdef;
	};
};

static websvc_f_t
websvc_from_request(struct websvc_s *tgt, const char *req, size_t UNUSED(len))
{
	static const char get_slash[] = "GET /";
	const char *p;

	if ((p = strstr(req, get_slash))) {
		if (strncmp(p += sizeof(get_slash) - 1, "secdef?", 7) == 0) {
			const char *q;

			tgt->ty = WEBSVC_F_SECDEF;
			if ((q = strstr(p += 7, "idx="))) {
				/* let's see what idx they want */
				long int idx = strtol(q + 4, NULL, 10);

				tgt->secdef.idx = (uint16_t)idx;
			}
		}
		return tgt->ty;
	}
	return WEBSVC_F_UNK;
}

static size_t
websvc_secdef(char *restrict tgt, size_t tsz, struct websvc_s sd)
{
	tws_const_sdef_t sdef = NULL;
	ssize_t res;

	QUO_DEBUG("printing secdef idx %hu\n", sd.secdef.idx);
	if (sd.secdef.idx && sd.secdef.idx <= subs.nsubs) {
		sdef = SECDEF(sd.secdef.idx);
	}

	if ((res = tws_sdef_xml(tgt, tsz, sdef)) < 0) {
		res = 0;
	}
	return res;
}


/* the queue */
static void
check_qq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = qq.q->free->i1st; ip; ip = ip->next, ni++);
	for (gq_item_t ip = qq.sbuf->i1st; ip; ip = ip->next, ni++);
	assert(ni == qq.q->nitems / sizeof(struct quo_qqq_s));

	ni = 0;
	for (gq_item_t ip = qq.q->free->ilst; ip; ip = ip->prev, ni++);
	for (gq_item_t ip = qq.sbuf->ilst; ip; ip = ip->prev, ni++);
	assert(ni == qq.q->nitems / sizeof(struct quo_qqq_s));
#endif	/* DEBUG_FLAG */
	return;
}

static quo_qqq_t
pop_q(void)
{
	quo_qqq_t res;

	if (qq.q->free->i1st == NULL) {
		size_t nitems = qq.q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(qq.q->free->ilst == NULL);
		QUO_DEBUG("QQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(qq.q, sizeof(*res), nitems + 64);
		gq_rbld_ll(qq.sbuf, df);
		check_qq();
	}
	/* get us a new client and populate the object */
	res = (void*)gq_pop_head(qq.q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

void
fix_quot(quo_qq_t UNUSED(qq_unused), struct quo_s q)
{
/* shall we rely on c++ code passing us a pointer we handed out earlier? */
	q30_t tgt;

	/* use the dummy ute file to do the sym2idx translation */
	if (q.idx == 0) {
		return;
	} else if (!(tgt = make_q30(q.idx, q.typ)).idx) {
		return;
	} else if (q.idx > subs.nsubs) {
		/* that's actually so fatal I wanna vomit
		 * that means IB sent us ticker ids we've never requested */
		return;
	}

	/* only when the coffee is roasted to perfection:
	 * update the slot TGT ... */
	subs.quos[tgt.idx - 1][tgt.typ] = ffff_m30_get_d(q.val);
	/* ... and push a reminder on the queue */
	{
		quo_qqq_t qi = pop_q();

		qi->q = tgt;
		qi->q.subtyp = 0UL;
		gq_push_tail(qq.sbuf, (gq_item_t)qi);
		QUO_DEBUG("pushed %p\n", qi);
	}
	return;
}


/* callbacks coming from the tws */
static void
infra_cb(tws_t tws, tws_cb_t what, struct tws_infra_clo_s clo)
{
	switch (what) {
	case TWS_CB_INFRA_ERROR:
		QUO_DEBUG("tws %p: oid %u  code %u: %s\n",
			tws, clo.oid, clo.code, (const char*)clo.data);
		break;
	case TWS_CB_INFRA_CONN_CLOSED:
		QUO_DEBUG("tws %p: connection closed\n", tws);
		break;
	default:
		QUO_DEBUG("%p infra: what %u  oid %u  code %u  data %p\n",
			tws, what, clo.oid, clo.code, clo.data);
		break;
	}
	return;
}

static void
pre_cb(tws_t tws, tws_cb_t what, struct tws_pre_clo_s clo)
{
	struct quo_s q;

	switch (what) {
	case TWS_CB_PRE_PRICE:
		switch (clo.tt) {
			/* hardcoded non-sense here!!! */
		case 1:
		case 9:
			q.typ = (quo_typ_t)clo.tt;
			break;
		case 2:
		case 4:
			q.typ = (quo_typ_t)(clo.tt + 1);
			break;
		default:
			q.typ = QUO_TYP_UNK;
			goto fucked;
		}
		q.idx = clo.oid;
		q.val = clo.val;
		break;
	case TWS_CB_PRE_SIZE:
		switch (clo.tt) {
		case 0:
			q.typ = QUO_TYP_BSZ;
			break;
		case 3:
		case 5:
			q.typ = (quo_typ_t)(clo.tt + 1);
			break;
		case 8:
			q.typ = QUO_TYP_VOL;
			break;
		default:
			q.typ = QUO_TYP_UNK;
			goto fucked;
		}
		q.idx = clo.oid;
		q.val = clo.val;
		break;

	case TWS_CB_PRE_CONT_DTL:
		QUO_DEBUG("sdef coming back %p\n", clo.data);
		if (clo.oid && clo.oid <= subs.nsubs && clo.data) {
			if (INSTRMT(clo.oid)) {
				tws_free_cont(INSTRMT(clo.oid));
			}
			if (SECDEF(clo.oid)) {
				tws_free_sdef(SECDEF(clo.oid));
			}
			INSTRMT(clo.oid) = tws_sdef_make_cont(clo.data);
			SECDEF(clo.oid) = tws_dup_sdef(clo.data);
		}
	case TWS_CB_PRE_CONT_DTL_END:
		break;

	default:
	fucked:
		QUO_DEBUG("%p pre: what %u  oid %u  tt %u  data %p\n",
			tws, what, clo.oid, clo.tt, clo.data);
		return;
	}
	fix_quot(NULL, q);
	return;
}


static void
beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nrd;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	nrd = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nrd < 0)) {
		goto out_revok;
	} else if (nrd == 0) {
		/* no need to bother */
		goto out_revok;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, j->buf})) {
		goto out_revok;
	}

	/* preapre a job */
	j->blen = nrd;

	/* intercept special channels */
	switch (udpc_pkt_cmd(JOB_PACKET(j))) {
	case POS_RPT:
		break;
	case POS_RPT_RPL:
		break;
	default:
		break;
	}

out_revok:
	return;
}

static void
cake_cb(EV_P_ ev_io *w, int revents)
{
	tws_t tws = w->data;

	if (revents & EV_READ) {
		if (tws_recv(tws) < 0) {
			/* grrrr */
			goto del_cake;
		}
	}
	if (revents & EV_WRITE) {
		if (tws_send(tws) < 0) {
			/* brilliant */
			goto del_cake;
		}
	}
	return;
del_cake:
	ev_io_stop(EV_A_ w);
	w->fd = -1;
	w->data = NULL;
	QUO_DEBUG("cake stopped\n");
	return;
}

/* fdfs */
static ev_io conns[8];
static size_t next_conn = 0;

static void
ev_io_shut(EV_P_ ev_io *w)
{
	int fd = w->fd;

	ev_io_stop(EV_A_ w);
	shutdown(fd, SHUT_RDWR);
	close(fd);
	w->fd = -1;
	return;
}

static void
dccp_data_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	/* the final \n will be subst'd later on */
	static const char hdr[] = "\
HTTP/1.1 200 OK\r\n\
Server: quo-tws\r\n\
Content-Length: % 5zu\r\n\
Content-Type: text/xml\r\n\
\r";
	/* hdr is a format string and hdr_len is as wide as the result printed
	 * later on */
	static const size_t hdr_len = sizeof(hdr);
	char buf[4096];
	char *rsp = buf + hdr_len;
	const size_t rsp_len = sizeof(buf) - hdr_len;
	ssize_t nrd;
	size_t cont_len;
	struct websvc_s voodoo;

	if ((nrd = read(w->fd, buf, sizeof(buf))) < 0) {
		goto clo;
	} else if ((size_t)nrd < sizeof(buf)) {
		buf[nrd] = '\0';
	} else {
		/* uh oh, mega request, wtf? */
		buf[sizeof(buf) - 1] = '\0';
	}

	switch (websvc_from_request(&voodoo, buf, nrd)) {
	default:
	case WEBSVC_F_UNK:
		goto clo;

	case WEBSVC_F_SECDEF:
		cont_len = websvc_secdef(rsp, rsp_len, voodoo);
		break;
	}

	/* prepare the header */
	(void)snprintf(buf, sizeof(buf), hdr, cont_len);
	buf[hdr_len - 1] = '\n';

	/* and append the actual contents */
	send(w->fd, buf, hdr_len + cont_len, 0);

clo:
	ev_io_shut(EV_A_ w);
	return;
}

static void
dccp_cb(EV_P_ ev_io *w, int UNUSED(re))
{
	union ud_sockaddr_u sa;
	socklen_t sasz = sizeof(sa);
	int s;

	QUO_DEBUG("interesting activity on %d\n", w->fd);

	if ((s = accept(w->fd, &sa.sa, &sasz)) < 0) {
		return;
	}

	if (conns[next_conn].fd > 0) {
		ev_io_shut(EV_A_ conns + next_conn);
	}

	ev_io_init(conns + next_conn, dccp_data_cb, s, EV_READ);
	conns[next_conn].data = NULL;
	ev_io_start(EV_A_ conns + next_conn);
	if (++next_conn >= countof(conns)) {
		next_conn = 0;
	}
	return;
}

static size_t
mmap_size(size_t nelem, size_t elemsz)
{
	static size_t pgsz = 0;

	if (UNLIKELY(!pgsz)) {
		pgsz = sysconf(_SC_PAGESIZE);
	}
	return ((nelem * elemsz) / pgsz + 1) * pgsz;
}

static int
__cont_batch_cb(tws_cont_t ins, void *clo)
{
	struct {
		utectx_t u;
	} *ctx = clo;
	uint16_t iidx;
	const char *nick;

	if (UNLIKELY(ins == NULL)) {
		error(0, "invalid contract");
		return -1;
	} else if (UNLIKELY((nick = tws_cont_nick(ins)) == NULL)) {
		error(0, "warning, could not find a nick name for %p", ins);
		return -1;
	} else if (UNLIKELY((iidx = ute_sym2idx(ctx->u, nick)) == 0)) {
		error(0, "warning, cannot find suitable index for %s", nick);
		return -1;
	}

	if (iidx > subs.nsubs) {
		/* singleton/resizer */
		size_t new_sz;
		void *new;

		/* sort the subs array out first */
		new_sz = mmap_size(iidx, sizeof(*subs.inss));
		new = mmap(subs.inss, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.inss, subs.nsubs * sizeof(*subs.inss));
		subs.inss = new;

		/* while we're at it, resize the quos array */
		/* we should at least accomodate 4 * iidx slots innit? */
		new_sz = mmap_size(iidx, sizeof(*subs.quos));
		new = mmap(subs.quos, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.quos, subs.nsubs * sizeof(*subs.quos));
		subs.quos = new;

		new_sz = mmap_size(iidx, sizeof(*subs.last_dsm));
		new = mmap(subs.last_dsm, new_sz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(new, subs.last_dsm, subs.nsubs * sizeof(*subs.last_dsm));
		subs.last_dsm = new;

		/* the largest guy determines the number of subs now */
		subs.nsubs = mmap_size(iidx, sizeof(*subs.quos)) /
			sizeof(*subs.quos) - 1;
	}

	INSTRMT(iidx) = ins;
	QUO_DEBUG("reg'd %s %hu\n", nick, iidx);
	return 0;
}

static void
init_subs(const char *file)
{
#define PR	(PROT_READ)
#define MS	(MAP_SHARED)
	void *fp;
	int fd;
	struct stat st;
	ssize_t fsz;

	if (stat(file, &st) < 0 || (fsz = st.st_size) < 0) {
		error(0, "subscription file %s invalid", file);
	} else if ((fd = open(file, O_RDONLY)) < 0) {
		error(0, "cannot read subscription file %s", file);
	} else if ((fp = mmap(NULL, fsz, PR, MS, fd, 0)) == MAP_FAILED) {
		error(0, "cannot read subscription file %s", file);
	} else {
		struct {
			utectx_t u;
		} x = {
			.u = uu,
		};

		tws_batch_cont(fp, fsz, __cont_batch_cb, &x);
	}
	return;
}

static void
redo_subs(tws_t tws)
{
	if (UNLIKELY(tws == NULL)) {
		/* stop ourselves */
		goto del_req;
	}

	/* and finally call the a/c requester */
	for (unsigned int i = 1; i <= subs.nsubs; i++) {
		if (INSTRMT(i) == NULL) {
			;
		} else if (tws_req_sdef(tws, i, INSTRMT(i)) < 0) {
			error(0, "cannot acquire secdefs of ins %u\n", i);
		} else if (tws_req_quo(tws, i, INSTRMT(i)) < 0) {
			error(0, "cannot (re)subscribe to ins %u\n", i);
		} else {
			QUO_DEBUG("sub'd %s\n", ute_idx2sym(uu, i));
		}
	}
	return;
del_req:
	/* clean up work if something got fucked */
	QUO_DEBUG("req stopped\n");
	return;
}

static void
undo_subs(tws_t UNUSED(tws))
{
	for (size_t i = 1; i <= subs.nsubs; i++) {
		if (INSTRMT(i)) {
			tws_free_cont(INSTRMT(i));
			INSTRMT(i) = NULL;
		}
		if (SECDEF(i)) {
			tws_free_sdef(SECDEF(i));
			SECDEF(i) = NULL;
		}
	}
	if (subs.nsubs) {
		size_t alloc_sz;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.inss));
		munmap(subs.inss, alloc_sz);
		subs.inss = NULL;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.quos));
		munmap(subs.quos, alloc_sz);
		subs.quos = NULL;

		alloc_sz = mmap_size(subs.nsubs, sizeof(*subs.last_dsm));
		munmap(subs.last_dsm, alloc_sz);
		subs.last_dsm = NULL;

		subs.nsubs = 0;
	}
	return;
}

static void
reco_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
/* this is a do fuckall thing */
	ctx_t p = w->data;
	int s;

	if ((s = tws_connect(p->tws, p->host, p->port, p->client)) < 0) {
		/* retry later */
		return;
	}

	/* pass on the socket we've got */
	p->tws_sock = s;
	/* reset tws structure */
	rset_tws(p->tws);

	/* stop ourselves */
	ev_timer_stop(EV_A_ w);
	w->data = NULL;
	QUO_DEBUG("reco stopped\n");
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	static ev_io cake[1] = {{0}};
	static ev_timer tm_reco[1] = {{0}};
	static int conndp = 0;
	ctx_t ctx = w->data;

	/* check if the tws is there */
	if (cake->fd <= 0 && ctx->tws_sock <= 0 && tm_reco->data == NULL) {
		/* uh oh! */
		ev_io_stop(EV_A_ cake);
		cake->data = NULL;

		/* start the reconnection timer */
		tm_reco->data = ctx;
		ev_timer_init(tm_reco, reco_cb, 0.0, 2.0/*option?*/);
		ev_timer_start(EV_A_ tm_reco);
		QUO_DEBUG("reco started\n");

	} else if (cake->fd <= 0 && ctx->tws_sock <= 0) {
		/* great, no connection yet */
		cake->data = NULL;
		QUO_DEBUG("no cake yet\n");

	} else if (cake->fd <= 0) {
		/* ah, connection is back up, init the watcher */
		cake->data = ctx->tws;
		ev_io_init(cake, cake_cb, ctx->tws_sock, EV_READ);
		ev_io_start(EV_A_ cake);
		QUO_DEBUG("cake started\n");

		/* clear tws_sock */
		ctx->tws_sock = -1;
		/* and the oid semaphore */
		conndp = 0;

	} else if (!conndp && tws_ready_p(ctx->tws)) {
		/* a DREAM i tell ya, let's do our subscriptions */
		redo_subs(ctx->tws);
		conndp = 1;

	} else {
		/* check the queue integrity */
		check_qq();

		/* maybe we've got something up our sleeve */
		flush_queue(ctx->tws);
	}

	/* and check the queue's integrity again */
	check_qq();

	QUO_DEBUG("queue %zu\n", qq.q->nitems / sizeof(struct quo_qqq_s));
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	QUO_DEBUG("unlooping\n");
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "quo-tws-clo.h"
#include "quo-tws-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

static pid_t
detach(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		/* i am the parent */
		fprintf(stdout, "daemonisation successful %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return -1;
	}
	/* close standard tty descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	/* reattach them to /dev/null */
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
	}
#if defined DEBUG_FLAG
	logerr = fopen("/tmp/quo-tws.log", "a");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	struct ctx_s ctx[1] = {{0}};
	/* args */
	struct quo_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_io ctrl[1];
	ev_io dccp[2];
	ev_prepare prp[1];
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (quo_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	/* snarf host name and port */
	if (argi->tws_host_given) {
		ctx->host = argi->tws_host_arg;
	} else {
		ctx->host = "localhost";
	}
	if (argi->tws_port_given) {
		ctx->port = (uint16_t)argi->tws_port_arg;
	} else {
		ctx->port = (uint16_t)7474;
	}
	if (argi->tws_client_id_given) {
		ctx->client = argi->tws_client_id_arg;
	} else {
		struct timeval now[1];

		(void)gettimeofday(now, NULL);
		ctx->client = now->tv_sec;
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	ev_signal_init(sighup_watcher, sigall_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	union __chan_u {
		ud_chan_t c;
		void *p;
	};
	{
		union __chan_u x = {ud_chan_init(UD_NETWORK_SERVICE)};
		int s = ud_chan_init_mcast(x.c);

		ctrl->data = x.p;
		ev_io_init(ctrl, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ ctrl);
	}

	/* make a channel for http/dccp requests */
	{
		union ud_sockaddr_u sa = {
			.sa6 = {
				.sin6_family = AF_INET6,
				.sin6_addr = in6addr_any,
				.sin6_port = 0,
			},
		};
		socklen_t sa_len = sizeof(sa);
		int s;

		if ((s = make_dccp()) < 0) {
			/* just to indicate we have no socket */
			dccp[0].fd = -1;
		} else if (sock_listener(s, &sa) < 0) {
			/* grrr, whats wrong now */
			close(s);
			dccp[0].fd = -1;
		} else {
			/* everything's brilliant */
			ev_io_init(dccp, dccp_cb, s, EV_READ);
			ev_io_start(EV_A_ dccp);

			getsockname(s, &sa.sa, &sa_len);
		}


		if (countof(dccp) < 2) {
			;
		} else if ((s = make_tcp()) < 0) {
			/* just to indicate we have no socket */
			dccp[1].fd = -1;
		} else if (sock_listener(s, &sa) < 0) {
			/* bugger */
			close(s);
			dccp[1].fd = -1;
		} else {
			/* yay */
			ev_io_init(dccp + 1, dccp_cb, s, EV_READ);
			ev_io_start(EV_A_ dccp + 1);

			getsockname(s, &sa.sa, &sa_len);
		}

		if (s >= 0) {
			make_brag_uri(&sa, sa_len);
		}
	}

	/* go through all beef channels */
	if (argi->beef_given) {
		union __chan_u x = {ud_chan_init(argi->beef_arg)};
		int s = ud_chan_init_mcast(x.c);

		beef->data = x.p;
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	if (init_tws(ctx->tws, -1, ctx->client) < 0) {
		res = 1;
		goto unroll;
	} else if ((uu = ute_mktemp(UO_NO_CREAT_TPC)) == NULL) {
		/* shall we warn the user about this */
		res = 1;
		goto unroll;
	}
	/* prepare the context and the tws */
	ctx->tws->infra_cb = infra_cb;
	ctx->tws->pre_cb = pre_cb;
	ctx->tws_sock = -1;
	/* pre and post poll hooks */
	prp->data = ctx;
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		init_subs(argi->inputs[i]);
	}

	/* and just before we're entering that REPL check for daemonisation */
	if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* cancel them timers and stuff */
	ev_prepare_stop(EV_A_ prp);

	/* kill all tws associated data */
	undo_subs(ctx->tws);
	/* secondly, get rid of the tws intrinsics */
	QUO_DEBUG("finalising tws guts\n");
	(void)fini_tws(ctx->tws);

	/* finish the order queue */
	check_qq();
	fini_gq(qq.q);
	ute_free(uu);

unroll:
	/* detaching beef and ctrl channels */
	if (argi->beef_given) {
		ud_chan_t c = beef->data;

		ev_io_stop(EV_A_ beef);
		ud_chan_fini(c);
	}
	{
		ud_chan_t c = ctrl->data;

		ev_io_stop(EV_A_ ctrl);
		ud_chan_fini(c);
	}

	/* detach http/dccp */
	for (size_t i = 0; i < countof(conns); i++) {
		if (conns[i].fd > 0) {
			ev_io_shut(EV_A_ conns + i);
		}
	}

	for (size_t i = 0; i < countof(dccp); i++) {
		int s = dccp[i].fd;

		if (s > 0) {
			ev_io_shut(EV_A_ dccp + i);
		}
	}

	/* destroy the default evloop */
	ev_default_destroy();
out:
	quo_parser_free(argi);
	return res;
}

/* quo-tws.c ends here */
