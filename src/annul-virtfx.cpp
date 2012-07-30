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
#include <netdb.h>
#include <stdarg.h>
#include <string.h>

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

#include <twsapi/EWrapper.h>
#include <twsapi/EPosixClientSocket.h>
#include <twsapi/Order.h>
#include <twsapi/Contract.h>

#include "wrp-debug.h"
#include "nifty.h"
#include "gq.h"

#include "tws-cont.h"

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define ANN_DEBUG(args...)	fprintf(LOGERR, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define ANN_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

typedef struct my_tws_s *my_tws_t;

typedef void *tws_instr_t;

typedef unsigned int tws_oid_t;

typedef struct ctx_s *ctx_t;
typedef void *quo_qq_t;
typedef struct quo_s *quo_t;
typedef struct quo_qqq_s *quo_qqq_t;
typedef struct q30_s q30_t;
typedef double q30_pack_t[4];

typedef void *pf_pq_t;
typedef struct pf_pos_s *pf_pos_t;
typedef struct pf_pqpr_s *pf_pqpr_t;

typedef enum {
	QUO_TYP_UNK,
	QUO_TYP_BID,
	QUO_TYP_BSZ,
	QUO_TYP_ASK,
	QUO_TYP_ASZ,
	QUO_TYP_TRA,
	QUO_TYP_TSZ,
	QUO_TYP_VWP,
	QUO_TYP_VOL,
	QUO_TYP_CLO,
	QUO_TYP_CSZ,
} quo_typ_t;

struct ctx_s {
	/* static context */
	const char *host;
	uint16_t port;
	int client;

	/* dynamic context */
	my_tws_t tws;
	int tws_sock;
};

struct my_tws_s {
	tws_oid_t next_oid;
	unsigned int time;
	void *wrp;
	void *cli;
	pf_pq_t pq;
	quo_qq_t qq;
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
	tws_cont_t *inss;

	/* actual quotes, this is bid, bsz, ask, asz  */
	q30_pack_t *quos;
};

struct quo_s {
	uint16_t idx;
	quo_typ_t typ;
	double val;
};

struct pf_pos_s {
	const char *sym;
	double lqty;
	double sqty;
};

struct pf_pq_s {
	struct gq_s q[1];
	struct gq_ll_s sbuf[1];
};

struct pf_pqpr_s {
	struct gq_item_s i;

	/* a/c name */
	char ac[16];
	/* symbol not as big, but fits in neatly with the 16b a/c identifier */
	char sym[48];
	double lqty;
	double sqty;
};

class __wrapper: public IB::EWrapper
{
public:
	void tickPrice(IB::TickerId, IB::TickType, double pri, int autop);
	void tickSize(IB::TickerId, IB::TickType, int size);
	void tickOptionComputation(
		IB::TickerId, IB::TickType,
		double imp_vol, double delta,
		double pri, double div, double gamma, double vega,
		double theta, double undly_pri);
	void tickGeneric(IB::TickerId, IB::TickType, double value);
	void tickString(IB::TickerId, IB::TickType, const IB::IBString&);
	void tickEFP(
		IB::TickerId, IB::TickType,
		double basisPoints, const IB::IBString &formattedBasisPoints,
		double totalDividends, int holdDays,
		const IB::IBString& futureExpiry, double dividendImpact,
		double dividendsToExpiry);
	void orderStatus(
		IB::OrderId, const IB::IBString &status,
		int filled, int remaining, double avgFillPrice, int permId,
		int parentId, double lastFillPrice, int clientId,
		const IB::IBString& whyHeld);
	void openOrder(
		IB::OrderId, const IB::Contract&,
		const IB::Order&, const IB::OrderState&);
	void openOrderEnd(void);
	void winError(const IB::IBString &str, int lastError);
	void connectionClosed(void);
	void updateAccountValue(
		const IB::IBString &key, const IB::IBString &val,
		const IB::IBString &currency, const IB::IBString &accountName);
	void updatePortfolio(
		const IB::Contract&, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL,
		const IB::IBString& accountName);
	void updateAccountTime(const IB::IBString &timeStamp);
	void accountDownloadEnd(const IB::IBString &accountName);
	void nextValidId(IB::OrderId orderId);
	void contractDetails(int req_id, const IB::ContractDetails&);
	void bondContractDetails(int req_id, const IB::ContractDetails&);
	void contractDetailsEnd(int req_id);
	void execDetails(int req_id, const IB::Contract&, const IB::Execution&);
	void execDetailsEnd(int reqId);
	void error(const int id, const int errorCode, const IB::IBString);
	void updateMktDepth(
		IB::TickerId,
		int position, int operation, int side, double price, int size);
	void updateMktDepthL2(
		IB::TickerId,
		int position, IB::IBString marketMaker,
		int operation, int side, double price, int size);
	void updateNewsBulletin(
		int msgId, int msgType,
		const IB::IBString &newsMessage, const IB::IBString &originExch);
	void managedAccounts(const IB::IBString &accountsList);
	void receiveFA(IB::faDataType, const IB::IBString &cxml);
	void historicalData(
		IB::TickerId,
		const IB::IBString &date,
		double open, double high, double low, double close, int volume,
		int barCount, double WAP, int hasGaps);
	void scannerParameters(const IB::IBString &xml);
	void scannerData(
		int reqId, int rank,
		const IB::ContractDetails&,
		const IB::IBString &distance, const IB::IBString &benchmark,
		const IB::IBString &projection, const IB::IBString &legsStr);
	void scannerDataEnd(int reqId);
	void realtimeBar(
		IB::TickerId, long time,
		double open, double high, double low, double close,
		long volume, double wap, int count);
	void currentTime(long time);
	void fundamentalData(IB::TickerId, const IB::IBString &data);
	void deltaNeutralValidation(int reqId, const IB::UnderComp&);
	void tickSnapshotEnd(int reqId);

	/* sort of private */
	my_tws_t ctx;
};

#if defined __cplusplus
extern "C" {
#endif	// __cplusplus

extern int init_tws(my_tws_t);
extern int fini_tws(my_tws_t);

extern int tws_connect(my_tws_t, const char *host, uint16_t port, int client);
extern int tws_disconnect(my_tws_t);

extern int tws_recv(my_tws_t);
extern int tws_send(my_tws_t);

extern void fix_quot(quo_qq_t, struct quo_s);
extern void fix_pos_rpt(pf_pq_t, const char *ac, struct pf_pos_s pos);

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("[annul-vfx] ", (FILE*)logerr);
	vfprintf((FILE*)logerr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', (FILE*)logerr);
		fputc(' ', (FILE*)logerr);
		fputs(strerror(eno ? eno : errno), (FILE*)logerr);
	}
	fputc('\n', (FILE*)logerr);
	return;
}

#if defined __cplusplus
}
#endif	// __cplusplus


/* wrapper impl */
void 
__wrapper::tickPrice(IB::TickerId id, IB::TickType fld, double pri, int)
{
	my_tws_t tws = this->ctx;
	struct quo_s q;
	uint16_t real_idx = id - tws->next_oid;

	WRP_DEBUG("prc %ld (%hu) %u %.6f", id, real_idx, fld, pri);

	// IB goes bsz bid ask asz tra tsz
	// we go   bid bsz ask asz tra tsz
	// so we need to swap the type if it's bid or bsz
	switch (fld) {
	case IB::BID:
	case IB::CLOSE:
		q.typ = (quo_typ_t)(unsigned int)fld;
		break;
	case IB::ASK:
	case IB::LAST:
		q.typ = (quo_typ_t)((unsigned int)fld + 1);
		break;
	default:
		q.typ = QUO_TYP_UNK;
		return;
	}

	// populate the rest
	q.idx = real_idx;
	q.val = pri;

	fix_quot(tws->qq, q);
	return;
}

void
__wrapper::tickSize(IB::TickerId id, IB::TickType fld, int size)
{
	my_tws_t tws = this->ctx;
	struct quo_s q;
	uint16_t real_idx = id - tws->next_oid;

	WRP_DEBUG("qty %ld (%hu) %u %d", id, real_idx, fld, size);

	// IB goes bsz bid ask asz tra tsz
	// we go   bid bsz ask asz tra tsz
	// so we need to swap the type if it's bid or bsz
	switch (fld) {
	case IB::BID_SIZE:
		q.typ = QUO_TYP_BSZ;
		break;
	case IB::ASK_SIZE:
	case IB::LAST_SIZE:
		q.typ = (quo_typ_t)((unsigned int)fld + 1);
		break;
	case IB::VOLUME:
		q.typ = (quo_typ_t)(unsigned int)fld;
		break;
	default:
		q.typ = QUO_TYP_UNK;
		return;
	}

	// populate the rest
	q.idx = real_idx;
	q.val = (double)size;

	fix_quot(tws->qq, q);
	return;
}

void
__wrapper::tickOptionComputation(
	IB::TickerId, IB::TickType,
	double, double,
	double, double, double, double,
	double, double)
{
	return;
}

void
__wrapper::tickGeneric(IB::TickerId, IB::TickType, double)
{
	return;
}

void
__wrapper::tickString(IB::TickerId, IB::TickType, const IB::IBString&)
{
	return;
}

void
__wrapper::tickEFP(
	IB::TickerId, IB::TickType,
	double, const IB::IBString&,
	double, int,
	const IB::IBString&, double,
	double)
{
	return;
}

void
__wrapper::orderStatus(
	IB::OrderId, const IB::IBString&,
	int, int, double, int,
	int, double, int,
	const IB::IBString&)
{
	return;
}

void
__wrapper::openOrder(
	IB::OrderId, const IB::Contract&,
	const IB::Order&, const IB::OrderState&)
{
	return;
}

void
__wrapper::openOrderEnd(void)
{
	return;
}

void
__wrapper::winError(const IB::IBString &str, int)
{
	WRP_DEBUG("win error: %s", str.c_str());
	return;
}

void
__wrapper::connectionClosed(void)
{
	WRP_DEBUG("conn closed");
	return;
}

void
__wrapper::updateAccountValue(
	const IB::IBString &key, const IB::IBString &val,
	const IB::IBString &ccy, const IB::IBString &acct_name)
{
	my_tws_t tws = this->ctx;
	const char *ck = key.c_str();

	if (strcmp(ck, "CashBalance") == 0) {
		const char *ac = acct_name.c_str();
		const char *cc = ccy.c_str();
		const char *cv = val.c_str();
		double pos = strtod(cv, NULL);
		struct pf_pos_s p = {
			cc,
			pos > 0 ? pos : 0.0,
			pos < 0 ? -pos : 0.0,
		};

		WRP_DEBUG("acct %s: portfolio %s -> %s", ac, cc, cv);
		fix_pos_rpt(tws->pq, ac, p);
	}
	return;
}

void
__wrapper::updatePortfolio(
	const IB::Contract &c, int pos,
	double, double, double,
	double, double,
	const IB::IBString &acct_name)
{
	my_tws_t tws = this->ctx;
	const char *ac = acct_name.c_str();
	struct pf_pos_s p = {
		c.localSymbol.c_str(),
		pos > 0 ? pos : 0.0,
		pos < 0 ? -pos : 0.0,
	};

	WRP_DEBUG("acct %s: portfolio %s -> %d", ac, p.sym, pos);
	fix_pos_rpt(tws->pq, ac, p);
	return;
}

void
__wrapper::updateAccountTime(const IB::IBString&)
{
	return;
}

void
__wrapper::accountDownloadEnd(const IB::IBString&)
{
	return;
}

void
__wrapper::nextValidId(IB::OrderId oid)
{
	WRP_DEBUG("next_oid <- %li", oid);
	this->ctx->next_oid = oid;
	return;
}

void
__wrapper::contractDetails(int, const IB::ContractDetails&)
{
	return;
}

void
__wrapper::bondContractDetails(int, const IB::ContractDetails&)
{
	return;
}

void
__wrapper::contractDetailsEnd(int)
{
	return;
}

void
__wrapper::execDetails(int, const IB::Contract&, const IB::Execution&)
{
	return;
}

void
__wrapper::execDetailsEnd(int)
{
	return;
}

void
__wrapper::error(const int id, const int code, const IB::IBString msg)
{
	WRP_DEBUG("id %d: code <- %i: %s", id, code, msg.c_str());
	return;
}

void
__wrapper::updateMktDepth(
	IB::TickerId,
	int, int, int, double, int)
{
	return;
}

void
__wrapper::updateMktDepthL2(
	IB::TickerId,
	int, IB::IBString,
	int, int, double, int)
{
	return;
}

void
__wrapper::updateNewsBulletin(
	int, int,
	const IB::IBString&, const IB::IBString&)
{
	return;
}

void
__wrapper::managedAccounts(const IB::IBString&)
{
	return;
}

void
__wrapper::receiveFA(IB::faDataType, const IB::IBString&)
{
	return;
}

void
__wrapper::historicalData(
	IB::TickerId,
	const IB::IBString&,
	double, double, double, double, int,
	int, double, int)
{
	return;
}

void
__wrapper::scannerParameters(const IB::IBString&)
{
	return;
}

void
__wrapper::scannerData(
	int, int,
	const IB::ContractDetails&,
	const IB::IBString&, const IB::IBString&,
	const IB::IBString&, const IB::IBString&)
{
	return;
}

void
__wrapper::scannerDataEnd(int)
{
	return;
}

void
__wrapper::realtimeBar(
	IB::TickerId, long,
	double, double, double, double,
	long, double, int)
{
	return;
}

void
__wrapper::currentTime(long int time)
{
	WRP_DEBUG("current_time <- %ld", time);
	this->ctx->time = time;
	return;
}

void
__wrapper::fundamentalData(IB::TickerId, const IB::IBString&)
{
	return;
}

void
__wrapper::deltaNeutralValidation(int, const IB::UnderComp&)
{
	return;
}

void
__wrapper::tickSnapshotEnd(int)
{
	return;
}


#if defined __cplusplus
extern "C" {
#endif	// __cplusplus

void
rset_tws(my_tws_t foo)
{
	foo->time = 0;
	foo->next_oid = 0;
	return;
}

int
init_tws(my_tws_t foo)
{
	__wrapper *wrp = new __wrapper();

	foo->cli = new IB::EPosixClientSocket(wrp);
	foo->wrp = wrp;
	rset_tws(foo);

	/* just so we know who we are */
	wrp->ctx = foo;
	return 0;
}

int
fini_tws(my_tws_t foo)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	__wrapper *wrp = (__wrapper*)foo->wrp;

	tws_disconnect(foo);

	/* wipe our context off the face of this earth */
	wrp->ctx = NULL;

	if (cli) {
		delete cli;
	}
	if (wrp) {
		delete wrp;
	}

	foo->cli = NULL;
	foo->wrp = NULL;
	rset_tws(foo);
	return 0;
}

int
tws_connect(my_tws_t foo, const char *host, uint16_t port, int client)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	int rc;

#if defined TWSAPI_IPV6
	rc = cli->eConnect2(host, port, client, AF_UNSPEC);
#else  // !TWSAPI_IPV6
	rc = cli->eConnect(host, port, client);
#endif	// TWSAPI_IPV6

	if (rc == 0) {
		wrp_debug(foo, "connection to [%s]:%hu failed", host, port);
		return -1;
	}

	// just request a lot of buggery here
	cli->reqCurrentTime();
	return cli->fd();
}

int
tws_disconnect(my_tws_t foo)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	cli->eDisconnect();
	return 0;
}

int
tws_recv(my_tws_t foo)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;

	cli->onReceive();
	return cli->isSocketOK() ? 0 : -1;
}

int
tws_send(my_tws_t foo)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;

	if (cli->isOutBufferEmpty()) {
		return 0;
	}
	cli->onSend();
	return cli->isSocketOK() ? 0 : -1;
}

int
tws_req_quo(my_tws_t foo, unsigned int idx, tws_instr_t i)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	long int real_idx;

	if (foo->next_oid == 0) {
		wrp_debug(foo, "subscription req'd no ticker ids available");
		return -1;
	}
	// we request idx + next_oid
	real_idx = foo->next_oid + idx;
	// we just have to assume it works
	cli->reqMktData(real_idx, *(IB::Contract*)i, std::string(""), false);
	return cli->isSocketOK() ? 0 : -1;
}

int
tws_connd_p(my_tws_t foo)
{
/* inspect TWS and return non-nil if requests to the tws can be made */
	return foo->next_oid && foo->time;
}

// contract stuff
void
tws_disassemble_instr(tws_cont_t ins)
{
	IB::Contract *ibi = (IB::Contract*)ins;

	if (ibi) {
		glu_debug((void*)ibi, "deleting");
		delete ibi;
	}
	return;
}

int
tws_req_ac(my_tws_t foo, const char *name)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	IB::IBString ac = std::string(name ?: "");

	cli->reqAccountUpdates(true, ac);
	return cli->isSocketOK() ? 0 : -1;
}


/* quote subscriptions */
static struct quo_sub_s subs = {0, 0, 0};
static utectx_t uu = NULL;

static inline size_t
mmap_size(size_t nelem, size_t elemsz)
{
	static size_t pgsz = 0;

	if (UNLIKELY(!pgsz)) {
		pgsz = sysconf(_SC_PAGESIZE);
	}
	return ((nelem * elemsz) / pgsz + 1) * pgsz;
}

static void
redo_subs(my_tws_t tws)
{
	if (UNLIKELY(tws == NULL)) {
		/* stop ourselves */
		goto del_req;
	}

	if (subs.nsubs == 0) {
		ANN_DEBUG("need to find some subs\n");
		tws_req_ac(tws, NULL/*== all_accounts*/);
		return;
	}

	/* and finally call the a/c requester */
	for (unsigned int i = 1; i <= subs.nsubs; i++) {
		if (subs.inss[i - 1] == NULL) {
			;
		} else if (tws_req_quo(tws, i, subs.inss[i - 1]) < 0) {
			error(0, "cannot (re)subscribe to ins %u\n", i);
		} else {
			ANN_DEBUG("sub'd %s\n", ute_idx2sym(uu, i));
		}
	}
	return;
del_req:
	/* clean up work if something got fucked */
	ANN_DEBUG("req stopped\n");
	return;
}

static void
undo_subs(my_tws_t)
{
	for (size_t i = 0; i < subs.nsubs; i++) {
		if (subs.inss[i]) {
			tws_disassemble_instr(subs.inss[i]);
			subs.inss[i] = NULL;
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

		subs.nsubs = 0;
	}
	return;
}


/* quoting queue */
static struct quo_qq_s qq = {0, 0, 0, 0, 0, 0};
static struct pf_pq_s pq = {0, 0, 0, 0, 0, 0};

static inline q30_t
make_q30(uint16_t iidx, quo_typ_t t)
{
	struct q30_s res = {0};

	if (LIKELY(t >= QUO_TYP_BID && t <= QUO_TYP_ASZ)) {
		res.typ = t - 1;
		res.idx = iidx;
	}
	return res;
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

static void
flush_queue(my_tws_t UNUSED(tws))
{
	struct sl1t_s l1t[1];
	struct timeval now[1];

	/* time */
	gettimeofday(now, NULL);

	/* populate l1t somewhat */
	sl1t_set_stmp_sec(l1t, now->tv_sec);
	sl1t_set_stmp_msec(l1t, now->tv_usec / 1000);

	for (gq_item_t ip; (ip = gq_pop_head(qq.sbuf));
	     gq_push_tail(qq.q->free, ip)) {
		quo_qqq_t q = (quo_qqq_t)ip;
		uint16_t tblidx;
		unsigned int ttf;

		if ((tblidx = q30_idx(q->q)) == 0 ||
		    (ttf = q30_sl1t_typ(q->q)) == SCOM_TTF_UNK) {
			continue;
		} else if (subs.quos[tblidx - 1][q30_typ(q->q)] == 0.0) {
			continue;
		}
		/* the typ was designed to coincide with ute's sl1t types */
		sl1t_set_ttf(l1t, ttf);
		sl1t_set_tblidx(l1t, tblidx);

		l1t->pri = ffff_m30_get_d(
			subs.quos[tblidx - 1][q30_typ(q->q)]).u;
		l1t->qty = ffff_m30_get_d(
			subs.quos[tblidx - 1][q30_typ(q->q) + 1]).u;

		fprintf(LOGERR, "%hu  %u  %u\n", ttf, l1t->pri, l1t->qty);
	}
	return;
}


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
pop_q(quo_qq_t the_q)
{
	quo_qqq_t res;
	struct quo_qq_s *rqq = (struct quo_qq_s*)the_q;

	if (rqq->q->free->i1st == NULL) {
		size_t nitems = rqq->q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(rqq->q->free->ilst == NULL);
		ANN_DEBUG("QQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(rqq->q, sizeof(*res), nitems + 64);
		gq_rbld_ll(rqq->sbuf, df);
		check_qq();
	}
	/* get us a new client and populate the object */
	res = (quo_qqq_t)gq_pop_head(rqq->q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

void
fix_quot(quo_qq_t the_q, struct quo_s q)
{
/* shall we rely on c++ code passing us a pointer we handed out earlier? */
	q30_t tgt;
	struct quo_qq_s *rqq = (struct quo_qq_s*)the_q;

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
	subs.quos[tgt.idx - 1][tgt.typ] = q.val;
	/* ... and push a reminder on the queue */
	{
		quo_qqq_t qi = pop_q(the_q);

		qi->q = tgt;
		qi->q.subtyp = 0UL;
		gq_push_tail(rqq->sbuf, (gq_item_t)qi);
		ANN_DEBUG("pushed %p\n", qi);
	}
	return;
}

static void
check_pq(void)
{
#if defined DEBUG_FLAG
	/* count all items */
	size_t ni = 0;

	for (gq_item_t ip = pq.q->free->i1st; ip; ip = ip->next, ni++);
	for (gq_item_t ip = pq.sbuf->i1st; ip; ip = ip->next, ni++);
	assert(ni == pq.q->nitems / sizeof(struct pf_pqpr_s));

	ni = 0;
	for (gq_item_t ip = pq.q->free->ilst; ip; ip = ip->prev, ni++);
	for (gq_item_t ip = pq.sbuf->ilst; ip; ip = ip->prev, ni++);
	assert(ni == pq.q->nitems / sizeof(struct pf_pqpr_s));
#endif	/* DEBUG_FLAG */
	return;
}

static pf_pqpr_t
pop_pr(pf_pq_t the_q)
{
	pf_pqpr_t res;
	struct pf_pq_s *rpq = (struct pf_pq_s*)the_q;

	if (rpq->q->free->i1st == NULL) {
		size_t nitems = rpq->q->nitems / sizeof(*res);
		ptrdiff_t df;

		assert(rpq->q->free->ilst == NULL);
		ANN_DEBUG("PQ RESIZE -> %zu\n", nitems + 64);
		df = init_gq(rpq->q, sizeof(*res), nitems + 64);
		gq_rbld_ll(rpq->sbuf, df);
		check_pq();
	}
	/* get us a new client and populate the object */
	res = (pf_pqpr_t)gq_pop_head(rpq->q->free);
	memset(res, 0, sizeof(*res));
	return res;
}

void
fix_pos_rpt(pf_pq_t the_q, const char *ac, struct pf_pos_s pos)
{
/* shall we rely on c++ code passing us a pointer we handed out earlier? */
	pf_pqpr_t pr = pop_pr(the_q);
	struct pf_pq_s *rpq = (struct pf_pq_s*)the_q;

	/* keep a rough note of the account name
	 * not that it does anything at the mo */
	strncpy(pr->ac, ac, sizeof(pr->ac));
	pr->ac[sizeof(pr->ac) - 1] = '\0';
	/* keep a rough note of the account name
	 * not that it does anything at the mo */
	strncpy(pr->sym, pos.sym, sizeof(pr->sym));
	pr->sym[sizeof(pr->sym) - 1] = '\0';
	/* and of course our quantities */
	pr->lqty = pos.lqty;
	pr->sqty = pos.sqty;

	gq_push_tail(rpq->sbuf, (gq_item_t)pr);
	ANN_DEBUG("pushed %p\n", pr);
	return;
}


static void
cake_cb(EV_P_ ev_io *w, int revents)
{
	my_tws_t tws = (my_tws_t)w->data;

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
	ANN_DEBUG("cake stopped\n");
	return;
}

static void
reco_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
/* this is a do fuckall thing */
	ctx_t p = (ctx_t)w->data;
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
	ANN_DEBUG("reco stopped\n");
	return;
}

static void
prep_cb(EV_P_ ev_prepare *w, int UNUSED(revents))
{
	static ev_io cake = {0, 0, 0, 0, 0, 0, 0, 0};
	static ev_timer tm_reco = {0, 0, 0, 0, 0, 0, 0};
	static int conndp = 0;
	ctx_t ctx = (ctx_t)w->data;

	/* check if the tws is there */
	if (cake.fd <= 0 && ctx->tws_sock <= 0 && tm_reco.data == NULL) {
		/* uh oh! */
		ev_io_stop(EV_A_ &cake);
		cake.data = NULL;

		/* start the reconnection timer */
		tm_reco.data = ctx;
		ev_timer_init(&tm_reco, reco_cb, 0.0, 2.0/*option?*/);
		ev_timer_start(EV_A_ &tm_reco);
		ANN_DEBUG("reco started\n");

	} else if (cake.fd <= 0 && ctx->tws_sock <= 0) {
		/* great, no connection yet */
		cake.data = NULL;
		ANN_DEBUG("no cake yet\n");

	} else if (cake.fd <= 0) {
		/* ah, connection is back up, init the watcher */
		cake.data = ctx->tws;
		ev_io_init(&cake, cake_cb, ctx->tws_sock, EV_READ);
		ev_io_start(EV_A_ &cake);
		ANN_DEBUG("cake started\n");

		/* clear tws_sock */
		ctx->tws_sock = -1;
		/* and the oid semaphore */
		conndp = 0;

	} else if (!conndp && tws_connd_p(ctx->tws)) {
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

	ANN_DEBUG("queue %zu\n", qq.q->nitems / sizeof(struct quo_qqq_s));
	return;
}

static void
sigall_cb(EV_P_ ev_signal*, int)
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	ANN_DEBUG("unlooping\n");
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
		ANN_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/ox-tws.log", "a");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	struct ctx_s ctx[1];
	/* args */
	struct quo_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_prepare prp[1];
	/* tws stuff */
	struct my_tws_s tws[1];
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

	if (init_tws(tws) < 0) {
		res = 1;
		goto unroll;
	}
	/* assign queues and whatnot */
	tws->pq = &pq;
	tws->qq = &qq;
	/* prepare the context */
	ctx->tws = tws;
	ctx->tws_sock = -1;
	/* pre and post poll hooks */
	prp->data = ctx;
	ev_prepare_init(prp, prep_cb);
	ev_prepare_start(EV_A_ prp);

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
	undo_subs(tws);
	/* secondly, get rid of the tws intrinsics */
	ANN_DEBUG("finalising tws guts\n");
	(void)fini_tws(ctx->tws);

	/* finish the order queue */
	check_qq();
	fini_gq(qq.q);

unroll:
	/* destroy the default evloop */
	ev_default_destroy();
out:
	quo_parser_free(argi);
	return res;
}

#if defined __cplusplus
}
#endif	// __cplusplus

/* annul-virtfx.cpp ends here */
