/*** ox-tws-wrapper.cpp -- order execution through tws
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
#endif	// HAVE_CONFIG_H
#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <string>

/* the tws api */
#include <twsapi/EWrapper.h>
#include <twsapi/EPosixClientSocket.h>
#include <twsapi/Order.h>
#include <twsapi/Order.h>
#include <twsapi/Contract.h>

#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "ox-tws-wrapper.h"
#include "ox-tws-private.h"

#define TWSAPI_IPV6	1

extern "C" {
extern int tws_reconcile(my_tws_t);
}

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


/* implementation of the class above */
#if defined __INTEL_COMPILER
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

static void
__attribute__((format(printf, 2, 3)))
wrp_debug(my_tws_t c, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fprintf(LOGERR, "[tws] %p: ", c);
	vfprintf(LOGERR, fmt, vap);
	va_end(vap);
	fputc('\n', stderr);
	return;
}

#define WRP_DEBUG(args...)			\
	wrp_debug(this->ctx, args)

void 
__wrapper::tickPrice(IB::TickerId id, IB::TickType fld, double pri, int autop)
{
	return;
}

void
__wrapper::tickSize(IB::TickerId, IB::TickType, int size)
{
	return;
}

void
__wrapper::tickOptionComputation(
	IB::TickerId, IB::TickType,
	double imp_vol, double delta,
	double pri, double div, double gamma, double vega,
	double theta, double undly_pri)
{
	return;
}

void
__wrapper::tickGeneric(IB::TickerId, IB::TickType, double value)
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
	double basisPoints, const IB::IBString &formattedBasisPoints,
	double totalDividends, int holdDays,
	const IB::IBString& futureExpiry, double dividendImpact,
	double dividendsToExpiry)
{
	return;
}

static bool
msg_cncd_p(const char *msg)
{
	static const char cncd[] = "Cancelled";
	static const char inact[] = "Inactive";

	return strcmp(msg, cncd) == 0 || strcmp(msg, inact) == 0;
}

static bool
msg_flld_p(const char *msg)
{
	static const char flld[] = "Filled";

	return strcmp(msg, flld) == 0;
}

static bool
msg_ackd_p(const char *msg)
{
	static const char subm[] = "Submitted";
	static const char pres[] = "PreSubmitted";

	return strcmp(msg, subm) == 0 || strcmp(msg, pres) == 0;
}

void
__wrapper::orderStatus(
	IB::OrderId oid, const IB::IBString &status,
	int filled, int remaining, double avgFillPrice, int permId,
	int parentId, double lastFillPrice, int clientId,
	const IB::IBString& whyHeld)
{
	my_tws_t tws = this->ctx;
	const char *msg = status.c_str();
	ox_oq_t oq = (ox_oq_t)tws->oq;
	tws_oid_t roid = (tws_oid_t)oid;

	WRP_DEBUG("ostatus %li  %s  f:%d  r:%d", oid, msg, filled, remaining);

	if (msg_cncd_p(msg)) {
		ox_oq_item_t ip;

		if ((ip = pop_match_oid(oq->ackd, roid)) ||
		    (ip = pop_match_oid(oq->sent, roid))) {
			WRP_DEBUG("CNCD %p <-> %u", ip, roid);
			push_tail(oq->cncd, ip);
		}
	} else if (msg_flld_p(msg)) {
		ox_oq_item_t ip;

		if ((ip = pop_match_oid(oq->ackd, roid)) ||
		    (ip = pop_match_oid(oq->sent, roid))) {
			WRP_DEBUG("FLLD %p <-> %u", ip, roid);
			push_tail(oq->flld, ip);
			if (remaining > 0) {
				/* split the order */
				;
			}
		}
	} else if (msg_ackd_p(msg)) {
		ox_oq_item_t ip;

		if ((ip = pop_match_oid(oq->sent, roid))) {
			WRP_DEBUG("ACKD %p <-> %u", ip, roid);
			push_tail(oq->ackd, ip);
		}
	}
	return;
}

void
__wrapper::openOrder(
	IB::OrderId oid, const IB::Contract&,
	const IB::Order&, const IB::OrderState&)
{
	WRP_DEBUG("open ord %li", oid);
	return;
}

void
__wrapper::openOrderEnd(void)
{
	WRP_DEBUG("open ord end");
	return;
}

void
__wrapper::winError(const IB::IBString &str, int lastError)
{
	WRP_DEBUG("win error: %s", str.c_str());
	return;
}

void
__wrapper::connectionClosed(void)
{
	return;
}

void
__wrapper::updateAccountValue(
	const IB::IBString &key, const IB::IBString &val,
	const IB::IBString &currency, const IB::IBString &accountName)
{
	return;
}

void
__wrapper::updatePortfolio(
	const IB::Contract&, int position,
	double marketPrice, double marketValue, double averageCost,
	double unrealizedPNL, double realizedPNL,
	const IB::IBString& accountName)
{
	return;
}

void
__wrapper::updateAccountTime(const IB::IBString &timeStamp)
{
	return;
}

void
__wrapper::accountDownloadEnd(const IB::IBString &accountName)
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
__wrapper::contractDetails(int req_id, const IB::ContractDetails&)
{
	return;
}

void
__wrapper::bondContractDetails(int req_id, const IB::ContractDetails&)
{
	return;
}

void
__wrapper::contractDetailsEnd(int req_id)
{
	return;
}

void
__wrapper::execDetails(int req_id, const IB::Contract&, const IB::Execution&)
{
	WRP_DEBUG("exec dtl %i", req_id);
	return;
}

void
__wrapper::execDetailsEnd(int req_id)
{
	WRP_DEBUG("exec dtl %i end", req_id);
	return;
}

void
__wrapper::error(const int id, const int code, const IB::IBString msg)
{
	WRP_DEBUG("code <- %i: %s", code, msg.c_str());
	return;
}

void
__wrapper::updateMktDepth(
	IB::TickerId,
	int position, int operation, int side, double price, int size)
{
	return;
}

void
__wrapper::updateMktDepthL2(
	IB::TickerId,
	int position, IB::IBString marketMaker,
	int operation, int side, double price, int size)
{
	return;
}

void
__wrapper::updateNewsBulletin(
	int msgId, int msgType,
	const IB::IBString &newsMessage, const IB::IBString &originExch)
{
	return;
}

void
__wrapper::managedAccounts(const IB::IBString &accountsList)
{
	return;
}

void
__wrapper::receiveFA(IB::faDataType, const IB::IBString &cxml)
{
	return;
}

void
__wrapper::historicalData(
	IB::TickerId,
	const IB::IBString &date,
	double open, double high, double low, double close, int volume,
	int barCount, double WAP, int hasGaps)
{
	return;
}

void
__wrapper::scannerParameters(const IB::IBString &xml)
{
	return;
}

void
__wrapper::scannerData(
	int reqId, int rank,
	const IB::ContractDetails&,
	const IB::IBString &distance, const IB::IBString &benchmark,
	const IB::IBString &projection, const IB::IBString &legsStr)
{
	return;
}

void
__wrapper::scannerDataEnd(int reqId)
{
	return;
}

void
__wrapper::realtimeBar(
	IB::TickerId, long time,
	double open, double high, double low, double close,
	long volume, double wap, int count)
{
	return;
}

void
__wrapper::currentTime(long int time)
{
	WRP_DEBUG("current_time <- %ld", time);
	return;
}

void
__wrapper::fundamentalData(IB::TickerId, const IB::IBString &data)
{
	return;
}

void
__wrapper::deltaNeutralValidation(int reqId, const IB::UnderComp&)
{
	return;
}

void
__wrapper::tickSnapshotEnd(int reqId)
{
	return;
}


int
init_tws(my_tws_t foo)
{
	__wrapper *wrp = new __wrapper();

	foo->cli = new IB::EPosixClientSocket(wrp);
	foo->wrp = wrp;
	foo->time = 0;
	foo->next_oid = 0;

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
	return 0;
}

int
tws_send(my_tws_t foo)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;

	if (cli->isOutBufferEmpty()) {
		return 0;
	}
	cli->onSend();
	return 0;
}


// testing
int
tws_put_order(my_tws_t tws, tws_order_t o)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)tws->cli;
	IB::Contract *__c;
	const_sl1t_t s;
	IB::Order __o;

	if (o->c == NULL) {
		// bugger off right away
		return -1;
	} else if ((s = (const_sl1t_t)o->o) == NULL) {
		// unusable too
		return -1;
	}

	// the contract is ctor'd already
	__c = (IB::Contract*)o->c;

	// quickly ctor the ib order on the fly
	if ((__o.orderId = o->oid) == 0) {
		__o.orderId = o->oid = tws->next_oid++;
	}
	switch (sl1t_ttf(s)) {
	case SL1T_TTF_BID:
	case SL2T_TTF_BID:
		__o.action = "BUY";
		break;
	case SL1T_TTF_ASK:
	case SL2T_TTF_ASK:
		__o.action = "SELL";
		break;
	default:
		// oh oh
		__o.action = "CANCEL";
		break;
	}
	// make sure we pick the right order type
	if (s->pri == SL1T_PRC_MKT) {
		__o.orderType = "MKT";
	} else {
		m30_t m = {.u = s->pri};

		__o.orderType = "LMT";
		__o.lmtPrice = ffff_m30_d(m);
	}
	// quantity is always important
	if (s->qty) {
		m30_t m = {.u = s->qty};

		__o.totalQuantity = ffff_m30_d(m);
		// as this is currency only, we're probably talking lots
		__o.totalQuantity *= 100000;

		cli->placeOrder(o->oid, *__c, __o);
	} else {
		/* cancel? */
		__o.action = "CANCEL";
		__o.totalQuantity = 0;

		cli->cancelOrder(o->oid);
	}
	return 0;
}

int
tws_reconcile(my_tws_t tws)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)tws->cli;

	wrp_debug(tws, "reconciliation");
	cli->reqOpenOrders();
	return 0;
}

/* ox-tws-wrapper.cpp ends here */
