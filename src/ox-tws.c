/*** ox-tws.c -- order execution through tws
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
/* for gettimeofday() */
#include <sys/time.h>
#include <fcntl.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <netinet/in.h>
#include <netdb.h>
#include <sys/mman.h>

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

/* our match messages */
#include "match.h"

/* the tws api */
#include "ox-tws-wrapper.h"
#include "ox-tws-private.h"

#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG
# include <assert.h>
# define OX_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define OX_DEBUG(args...)
# define assert(x)
#endif	/* DEBUG_FLAG */
void *logerr;

typedef uint32_t ox_idx_t;

#define CL(x)		(x)

struct ox_cl_s {
	struct umm_agt_s agt;
	/* ib contract */
	tws_instr_t ins;
	/* channel we're talking */
	ud_chan_t ch;
	/* slightly shorter than the allowed sym length */
	char sym[64 + 64 - sizeof(struct umm_agt_s) -
		 sizeof(void*) - sizeof(ud_chan_t)];
};

struct ox_oq_item_s {
	/* the sym, ib instr and agent we're talking */
	ox_cl_t cl;

	/* the order we're talking */
	struct sl1t_s l1t[1];

	/* ib order id */
	tws_oid_t oid;
	/* for partial fills */
	m30_t rem_qty;

	ox_oq_item_t next;
	ox_oq_item_t prev;
};


/* the actual core */
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

static struct ox_cl_s cls[64] = {0};
static size_t ncls = 0;
static size_t umm_pno = 0;

static struct ox_oq_s oq = {0};

/* simplistic dllists */
static size_t __attribute__((const, pure))
nmemb(size_t n)
{
	static size_t pgsz = 0;

	if (!pgsz) {
		pgsz = sysconf(_SC_PAGESIZE);
	}
	return (n * sizeof(*oq.items) + (pgsz - 1)) & ~(pgsz - 1);
}

static void
rbld_dll(ox_oq_dll_t dll, ox_oq_item_t nu_ref, ox_oq_item_t ol_ref)
{
	ptrdiff_t df = nu_ref - ol_ref;

	if (dll->i1st) {
		dll->i1st += df;
	}
	if (dll->ilst) {
		dll->ilst += df;
	}
	for (ox_oq_item_t ip = dll->i1st; ip; ip = ip->next) {
		if (ip->next) {
			ip->next += df;
		}
		if (ip->prev) {
			ip->prev += df;
		}
	}
	return;
}

static void
check_oq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (ox_oq_item_t ip = oq.free->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.unpr->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.sent->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.ackd->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.cncd->i1st; ip; ip = ip->next, ni++);
	for (ox_oq_item_t ip = oq.flld->i1st; ip; ip = ip->next, ni++);
	OX_DEBUG("forw %zu oall\n", ni);
	assert(ni == oq.nitems);

	ni = 0;
	for (ox_oq_item_t ip = oq.free->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.unpr->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.sent->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.ackd->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.cncd->ilst; ip; ip = ip->prev, ni++);
	for (ox_oq_item_t ip = oq.flld->ilst; ip; ip = ip->prev, ni++);
	OX_DEBUG("back %zu oall\n", ni);
	assert(ni == oq.nitems);
#endif	/* DEBUG_FLAG */
	return;
}

static void
init_oq(size_t at_least)
{
#define PROT_MEM	(PROT_READ | PROT_WRITE)
#define MAP_MEM		(MAP_ANONYMOUS | MAP_PRIVATE)
	size_t nusz = nmemb(at_least);
	size_t nu = nusz / sizeof(*oq.items);
	size_t ol = oq.nitems;
	ox_oq_item_t ep;

	if (ol > nu) {
		return;
	} else if (oq.items == NULL) {
		oq.items = mmap(NULL, nusz, PROT_MEM, MAP_MEM, -1, 0);
		oq.nitems = nu;
	} else {
		size_t olsz = nmemb(ol);
		void *nu_items;

		nu_items = mmap(NULL, nusz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(nu_items, oq.items, olsz);

		/* fix up all lists */
		rbld_dll(oq.free, nu_items, oq.items);
		rbld_dll(oq.flld, nu_items, oq.items);
		rbld_dll(oq.cncd, nu_items, oq.items);
		rbld_dll(oq.ackd, nu_items, oq.items);
		rbld_dll(oq.sent, nu_items, oq.items);
		rbld_dll(oq.unpr, nu_items, oq.items);

		/* unmap the old guy */
		munmap(oq.items, olsz);

		/* reassign */
		oq.items = nu_items;
		oq.nitems = nu;
	}

	/* fill up the free list */
	ep = oq.items + ol;
	for (ox_oq_item_t sp = ep, eep = ep + (nu - ol); sp < eep; sp++) {
		if (sp + 1 < eep) {
			sp->next = sp + 1;
		}
		if (sp > ep) {
			sp->prev = sp - 1;
		}
	}
	/* attach new list to free list */
	ep->prev = oq.free->ilst;
	if (oq.free->ilst) {
		oq.free->ilst->next = ep;
	} else {
		assert(oq.free->i1st == NULL);
	}
	if (oq.free->i1st == NULL) {
		oq.free->i1st = ep;
		oq.free->ilst = ep + (nu - ol - 1);
	}
	check_oq();
	return;
}

static void
fini_oq(void)
{
	if (oq.items) {
		munmap(oq.items, oq.nitems);
		oq.items = NULL;
		oq.nitems = 0;
	}
	return;
}

ox_oq_item_t
pop_head(ox_oq_dll_t dll)
{
	ox_oq_item_t res;

	if ((res = dll->i1st) && (dll->i1st = dll->i1st->next) == NULL) {
		dll->ilst = NULL;
	} else if (res) {
		dll->i1st->prev = NULL;
	}
	return res;
}

void
push_tail(ox_oq_dll_t dll, ox_oq_item_t i)
{
	if (dll->ilst) {
		dll->ilst->next = i;
		i->prev = dll->ilst;
		i->next = NULL;
		dll->ilst = i;
	} else {
		assert(dll->i1st == NULL);
		dll->i1st = dll->ilst = i;
		i->next = NULL;
		i->prev = NULL;
	}
	return;
}

static ox_oq_item_t
pop_free(void)
{
	ox_oq_item_t res;

	if (oq.free->i1st == NULL) {
		assert(oq.free->ilst == NULL);
		init_oq(oq.nitems + 256);
		OX_DEBUG("RESIZE -> %zu\n", oq.nitems + 256);
	}
	res = pop_head(oq.free);
	memset(res, 0, sizeof(*res));
	return res;
}

static bool
ox_oq_item_matches_p(ox_oq_item_t i, ox_cl_t cl, const_sl1t_t l1t)
{
	return memcmp(&cl->agt, &i->cl->agt, sizeof(cl->agt)) == 0 &&
		sl1t_tblidx(l1t) == sl1t_tblidx(i->l1t) &&
		l1t->pri == i->l1t->pri;
}

static void
pop_item(ox_oq_dll_t dll, ox_oq_item_t ip)
{
	/* found him */
	if (ip->prev) {
		ip->prev->next = ip->next;
	} else {
		/* must be head then */
		dll->i1st = ip->next;
	}
	if (ip->next) {
		ip->next->prev = ip->prev;
	} else {
		/* must be tail then */
		dll->ilst = ip->prev;
	}
	ip->next = ip->prev = NULL;
	return;
}

static ox_oq_item_t
pop_match_cl_l1t(ox_oq_dll_t dll, ox_cl_t cl, const_sl1t_t l1t)
{
	for (ox_oq_item_t ip = dll->i1st; ip; ip = ip->next) {
		if (ox_oq_item_matches_p(ip, cl, l1t)) {
			pop_item(dll, ip);
			return ip;
		}
	}
	return NULL;
}

ox_oq_item_t
pop_match_oid(ox_oq_dll_t dll, tws_oid_t oid)
{
	for (ox_oq_item_t ip = dll->i1st; ip; ip = ip->next) {
		if (ip->oid == oid) {
			/* found him */
			pop_item(dll, ip);
			return ip;
		}
	}
	return NULL;
}


static inline void
udpc_seria_add_umm(udpc_seria_t sctx, umm_pair_t p)
{
	memcpy(sctx->msg + sctx->msgoff, p, sizeof(*p));
	sctx->msgoff += sizeof(*p);
	return;
}

static void
prep_order(ox_cl_t cl, scom_t sc)
{
	/* pop us an item from the queue */
	ox_oq_item_t o = pop_free();
	uint16_t ttf = scom_thdr_ttf(sc);

	/* nifty that umm_pair_s looks like a struct sl1t_s at the beginning */
	memcpy(o->l1t, sc, sizeof(*o->l1t));

	switch (ttf) {
	case SL1T_TTF_BID:
	case SL2T_TTF_BID:
		OX_DEBUG("B %s\n", cl->sym);
		break;
	case SL1T_TTF_ASK:
	case SL2T_TTF_ASK:
		OX_DEBUG("S %s\n", cl->sym);
		break;
	default:
		OX_DEBUG("order type not supported\n");
		return;
	}

	/* copy the agent */
	o->cl = cl;
	/* push on the unprocessed list */
	push_tail(oq.unpr, o);
	return;
}

static void
prep_cancel(ox_cl_t cl, scom_t s)
{
	ox_oq_item_t ip;
	const_sl1t_t l1t = (const void*)s;

	/* traverse the ackd queue first, then the sent queue */
	if ((ip = pop_match_cl_l1t(oq.ackd, cl, l1t)) == NULL &&
	    (ip = pop_match_cl_l1t(oq.sent, cl, l1t)) == NULL) {
		OX_DEBUG("no matches\n");
		return;
	}
	/* else do something */
	OX_DEBUG("ORDER %p matches <-> %u -> CANCEL\n", ip, ip->oid);

	ip->l1t->qty = 0;
	push_tail(oq.unpr, ip);
	return;
}

static ox_cl_t
find_cli(struct umm_agt_s agt)
{
	for (size_t i = 0; i < ncls; i++) {
		if (memcmp(&CL(cls + i)->agt, &agt, sizeof(agt)) == 0) {
			return cls + i;
		}
	}
	return NULL;
}

static ox_cl_t
add_cli(struct umm_agt_s agt)
{
	size_t idx = ncls++;

	cls[idx].agt = agt;
	return cls + idx;
}


static void
snarf_data(job_t j, ud_chan_t UNUSED(unused_c))
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct umm_agt_s probe;

	if (UNLIKELY(plen == 0)) {
		return;
	}

	/* the network part of the agent, remains constant throughout */
	probe.addr = j->sa.sa6.sin6_addr;
	probe.port = j->sa.sa6.sin6_port;

	for (scom_thdr_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t ttf = scom_thdr_ttf(sp);

		switch (ttf) {
			ox_cl_t cl;
		case SL1T_TTF_BID:
		case SL1T_TTF_ASK:
		case SL2T_TTF_BID:
		case SL2T_TTF_ASK:
			/* dig deeper */
			probe.uidx = scom_thdr_tblidx(sp);

			if ((cl = find_cli(probe)) == NULL) {
				/* fuck this, we don't even know what
				 * instrument to trade */
				break;
			}

			/* make sure it isn't a cancel */
			if (LIKELY(((sl1t_t)sp)->qty)) {
				/* it's an order, enqueue */
				prep_order(cl, sp);
			} else {
				/* too bad, it's a cancel */
				prep_cancel(cl, sp);
			}
			break;
		default:
			break;
		}
	}
	return;
}

static void
snarf_meta(job_t j, ud_chan_t c)
{
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	struct udpc_seria_s ser[1];
	struct umm_agt_s probe;
	uint8_t tag;

	/* snarf the network bits and bobs */
	probe.addr = j->sa.sa6.sin6_addr;
	probe.port = j->sa.sa6.sin6_port;

	udpc_seria_init(ser, pbuf, plen);
	while (ser->msgoff < plen && (tag = udpc_seria_tag(ser))) {
		ox_cl_t cl;

		if (UNLIKELY(tag != UDPC_TYPE_UI16)) {
			break;
		}
		/* otherwise find us the id */
		probe.uidx = udpc_seria_des_ui16(ser);
		/* and the cli, if any */
		if ((cl = find_cli(probe)) == NULL) {
			cl = add_cli(probe);
		}
		/* next up is the symbol */
		tag = udpc_seria_tag(ser);
		if (UNLIKELY(tag != UDPC_TYPE_STR || cl == 0)) {
			break;
		}
		/* fuck error checking */
		udpc_seria_des_str_into(CL(cl)->sym, sizeof(CL(cl)->sym), ser);

		/* bother the ib factory to get us a contract */
		if (CL(cl)->ins) {
			tws_disassemble_instr(CL(cl)->ins);
		}
		CL(cl)->ins = tws_assemble_instr(CL(cl)->sym);
		CL(cl)->ch = c;
	}
	return;
}

static void
send_order(my_tws_t tws, ox_oq_item_t i)
{
	struct tws_order_s foo = {
		/* let the tws decide */
		.oid = i->oid,
		/* instrument, fuck checking */
		.c = i->cl->ins,
		/* good god, can we uphold this? */
		.o = i->l1t,
	};

	OX_DEBUG("ORDER %p %u\n", i, foo.oid);
	if (tws_put_order(tws, &foo) < 0) {
		OX_DEBUG("unusable: %p %p\n", foo.c, foo.o);
	}
	i->oid = foo.oid;
	OX_DEBUG("ORDER %p <-> oid %u\n", i, i->oid);
	return;
}

static void
flush_queue(my_tws_t tws)
{
	size_t nsnt = 0;

	for (ox_oq_item_t ip; (ip = pop_head(oq.unpr)); nsnt++) {
		send_order(tws, ip);
		push_tail(oq.sent, ip);
	}

	/* assume it's possible to write */
	if (nsnt) {
		tws_send(tws);
	}
	return;
}

static void
flush_cncd(void)
{
	static char rpl[UDPC_PKTLEN];
	struct udpc_seria_s ser[1];
	ud_chan_t cur_ch = NULL;

#define PKT(x)		((ud_packet_t){sizeof(x), x})
	udpc_make_pkt(PKT(rpl), 0, umm_pno++, UMM);
#define MAKE_PKT(x)							\
	udpc_set_data_pkt(PKT(x));					\
	udpc_seria_init(ser, UDPC_PAYLOAD(x), UDPC_PAYLLEN(sizeof(x)))

	for (ox_oq_item_t ip, nex = NULL; (ip = pop_head(oq.cncd));) {
		/* send cancellation match message */
		struct umm_pair_s mmp[1];
		uint16_t ttf = sl1t_ttf(ip->l1t);

		if (cur_ch == NULL || ip == nex) {
			/* send the old guy */
			if (cur_ch) {
				ud_chan_send_ser(cur_ch, ser);
			}
			cur_ch = ip->cl->ch;
			MAKE_PKT(rpl);
			nex = NULL;
		} else if (cur_ch != ip->cl->ch) {
			/* later */
			if (nex == NULL) {
				nex = ip;
			}
			push_tail(oq.cncd, ip);
			continue;
		}

		memcpy(mmp->l1, ip->l1t, sizeof(*ip->l1t));
		if (ttf == SL1T_TTF_BID || ttf == SL2T_TTF_BID) {
			mmp->agt[0] = ip->cl->agt;
			memset(mmp->agt + 1, 0, sizeof(*mmp->agt));
		} else if (ttf == SL1T_TTF_ASK || ttf == SL2T_TTF_ASK) {
			memset(mmp->agt + 0, 0, sizeof(*mmp->agt));
			mmp->agt[1] = ip->cl->agt;
		} else {
			OX_DEBUG("uh oh, ttf is %hx\n", ttf);
			memset(mmp->agt, 0, 2 * sizeof(*mmp->agt));
			abort();
		}
		udpc_seria_add_umm(ser, mmp);

		/* make sure we free this guy */
		OX_DEBUG("freeing %p\n", ip);
		push_tail(oq.free, ip);
	}
	if (cur_ch) {
		ud_chan_send_ser(cur_ch, ser);
	}
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
	case QMETA_RPL:
		snarf_meta(j, w->data);
		break;

	case UTE:
		snarf_data(j, w->data);
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
	my_tws_t tws = w->data;

	if (revents & EV_READ) {
		tws_recv(tws);
	}
	if (revents & EV_WRITE) {
		tws_send(tws);
	}
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	my_tws_t tws = w->data;

	/* maybe we've got something up our sleeve */
	flush_queue(tws);
	/* check cancellation list */
	flush_cncd();
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
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
#include "ox-tws-clo.h"
#include "ox-tws-clo.c"
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
		OX_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/ox-tws.log", "w");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	/* args */
	struct ox_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	/* tws specific stuff */
	struct my_tws_s tws[1];
	const char *host;
	uint16_t port;
	int client;
	ev_io cake[1];
	ev_prepare prp[1];
	/* our beef channels */
	size_t nbeef = 0;
	ev_io *beef = NULL;
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (ox_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* snarf host name and port */
	if (argi->tws_host_given) {
		host = argi->tws_host_arg;
	} else {
		host = "localhost";
	}
	if (argi->tws_port_given) {
		port = (uint16_t)argi->tws_port_arg;
	} else {
		port = (uint16_t)7474;
	}
	if (argi->tws_client_id_given) {
		client = argi->tws_client_id_arg;
	} else {
		struct timeval now[1];

		(void)gettimeofday(now, NULL);
		client = now->tv_sec;
	}

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	if ((beef = malloc(nbeef * sizeof(*beef))) == NULL) {
		res = 1;
		goto out;
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

		beef->data = x.p;
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		union __chan_u x = {ud_chan_init(argi->beef_arg[i])};
		int s = ud_chan_init_mcast(x.c);

		beef[i + 1].data = x.p;
		ev_io_init(beef + i + 1, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	/* we init this late, because the tws connection is not a requirement
	 * and we may have support for changing/closing/reopening it later
	 * on anyway, maybe through a nice shell, who knows */
	{
		int s;

		if (init_tws(tws) < 0) {
			res = 1;
			goto unroll;
		} else if ((s = tws_connect(tws, host, port, client)) < 0) {
			res = 1;
			goto past_loop;
		}

		/* init a watcher */
		cake->data = tws;
		ev_io_init(cake, cake_cb, s, EV_READ);
		ev_io_start(EV_A_ cake);

		/* pre and post poll hooks */
		prp->data = tws;
		ev_prepare_init(prp, prep_cb);
		ev_prepare_start(EV_A_ prp);

		/* make sure we know about the global order queue */
		tws->oq = &oq;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	(void)fini_tws(tws);

	/* finish the order queue */
	fini_oq();

	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_chan_t c = beef[i].data;

		ev_io_stop(EV_A_ beef + i);
		ud_chan_fini(c);
	}
	/* free beef resources */
	free(beef);

unroll:
	/* destroy the default evloop */
	ev_default_destroy();
out:
	return res;
}

/* ox-tws.c ends here */
