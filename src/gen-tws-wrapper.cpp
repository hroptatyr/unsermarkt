/*** gen-tws-wrapper.cpp -- generic tws c api
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
#include <string.h>

/* the tws api */
#include <twsapi/EWrapper.h>
#include <twsapi/EPosixClientSocket.h>
#include <twsapi/Order.h>
#include <twsapi/Order.h>
#include <twsapi/Contract.h>

#include "gen-tws.h"
#include "nifty.h"

#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define glu_debug(args...)
# define wrp_debug(args...)
# define assert(x)
#endif	/* DEBUG_FLAG */

#define TWSAPI_IPV6		1

extern void *logerr;

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
#if 1
	void marketDataType(IB::TickerId reqId, int mkt_data_type);
#endif	// 1

	/* sort of private */
	tws_oid_t next_oid;
	tws_time_t time;
	void *cli;
	void *tws;
};

#define TWS_WRP(x)	((__wrapper*)x)
#define TWS_CLI(x)	((IB::EPosixClientSocket*)(TWS_WRP(x))->cli)
#define TWS_PRIV_WRP(x)	TWS_WRP(((tws_t)x)->priv)
#define TWS_PRIV_CLI(x)	TWS_CLI(((tws_t)x)->priv)

#define WRP_TWS(x)	((tws_t)x->tws)


/* pres */
static struct tws_pre_clo_s pre_clo;

#define __PRE_CB(x, what, _oid_, _tt_, _fancy_)		\
	if (LIKELY(x->pre_cb != NULL)) {		\
		pre_clo.oid = _oid_;			\
		pre_clo.tt = _tt_;			\
		pre_clo _fancy_;			\
		x->pre_cb(x, what, pre_clo);		\
	} else while (0)
#define PRE_CB(x, what, args...)	__PRE_CB(x, what, args)
#define PRE_ONLY_ID(_id)		(tws_oid_t)(_id), 0, .data = NULL

void 
__wrapper::tickPrice(IB::TickerId id, IB::TickType fld, double pri, int)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_PRICE, (tws_oid_t)id, fld, .val = pri);
	return;
}

void
__wrapper::tickSize(IB::TickerId id, IB::TickType fld, int size)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_SIZE, (tws_oid_t)id, fld, .val = (double)size);
	return;
}

void
__wrapper::tickOptionComputation(
	IB::TickerId id, IB::TickType fld,
	double, double,
	double, double, double, double,
	double, double)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_OPTION, (tws_oid_t)id, fld, .data = NULL);
	return;
}

void
__wrapper::tickGeneric(IB::TickerId id, IB::TickType fld, double value)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_GENERIC, (tws_oid_t)id, fld, .val = value);
	return;
}

void
__wrapper::tickString(IB::TickerId id, IB::TickType fld, const IB::IBString &s)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_STRING, (tws_oid_t)id, fld, .str = s.c_str());
	return;
}

void
__wrapper::tickEFP(
	IB::TickerId id, IB::TickType fld,
	double, const IB::IBString&,
	double, int,
	const IB::IBString&, double, double)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_EFP, (tws_oid_t)id, fld, .data = NULL);
	return;
}

void
__wrapper::tickSnapshotEnd(int id)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_SNAP_END, PRE_ONLY_ID(id));
	return;
}

#if 1
void
__wrapper::marketDataType(IB::TickerId id, int mkt_data_type)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_MKT_DATA_TYPE,
		(tws_oid_t)id, 0, .i = mkt_data_type);
	return;
}
#endif	// 1

void
__wrapper::contractDetails(int req_id, const IB::ContractDetails &cd)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_CONT_DTL, (tws_oid_t)req_id, 0, .data = &cd);
	return;
}

void
__wrapper::bondContractDetails(int req_id, const IB::ContractDetails &cd)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_CONT_DTL, (tws_oid_t)req_id, 0, .data = &cd);
	return;
}

void
__wrapper::contractDetailsEnd(int req_id)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_CONT_DTL_END, PRE_ONLY_ID(req_id));
	return;
}

void
__wrapper::fundamentalData(IB::TickerId id, const IB::IBString &data)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_FUND_DATA,
		(tws_oid_t)id, 0, .str = data.c_str());
	return;
}

void
__wrapper::updateMktDepth(
	IB::TickerId id,
	int, int, int, double, int)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_UPD_MKT_DEPTH, PRE_ONLY_ID(id));
	return;
}

void
__wrapper::updateMktDepthL2(
	IB::TickerId id,
	int, IB::IBString,
	int, int, double, int)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_UPD_MKT_DEPTH, PRE_ONLY_ID(id));
	return;
}

void
__wrapper::historicalData(
	IB::TickerId id, const IB::IBString&,
	double, double, double, double, int,
	int, double, int)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_HIST_DATA, PRE_ONLY_ID(id));
	return;
}

void
__wrapper::realtimeBar(
	IB::TickerId id, long,
	double, double, double, double,
	long, double, int)
{
	tws_t tws = WRP_TWS(this);

	PRE_CB(tws, TWS_CB_PRE_REALTIME_BAR, PRE_ONLY_ID(id));
	return;
}


/* trades */
static struct tws_trd_clo_s trd_clo;

#define __TRD_CB(x, what, _id_, _data_)			\
	if (LIKELY(x->trd_cb != NULL)) {		\
		trd_clo.oid = _id_;			\
		trd_clo.data = _data_;			\
		x->trd_cb(x, what, trd_clo);		\
	} else while (0)
#define TRD_CB(x, what, args...)	__TRD_CB(x, what, args)

void
__wrapper::orderStatus(
	IB::OrderId id, const IB::IBString&,
	int, int, double, int,
	int, double, int,
	const IB::IBString&)
{
	tws_t tws = WRP_TWS(this);

	TRD_CB(tws, TWS_CB_TRD_ORD_STATUS, (tws_oid_t)id, NULL);
	return;
}

void
__wrapper::openOrder(
	IB::OrderId id, const IB::Contract&,
	const IB::Order&, const IB::OrderState&)
{
	tws_t tws = WRP_TWS(this);

	TRD_CB(tws, TWS_CB_TRD_OPEN_ORD, (tws_oid_t)id, NULL);
	return;
}

void
__wrapper::openOrderEnd(void)
{
	tws_t tws = WRP_TWS(this);

	TRD_CB(tws, TWS_CB_TRD_OPEN_ORD_END, 0, NULL);
	return;
}


/* post trade */
static struct tws_post_clo_s post_clo;

#define __POST_CB(x, what, _id_, _data_)		\
	if (LIKELY(x->post_cb != NULL)) {		\
		post_clo.oid = _id_;			\
		post_clo.data = _data_;			\
		x->post_cb(x, what, post_clo);		\
	} else while (0)
#define POST_CB(x, what, args...)	__POST_CB(x, what, args)

void
__wrapper::execDetails(int req_id, const IB::Contract&, const IB::Execution&)
{
	tws_t tws = WRP_TWS(this);

	POST_CB(tws, TWS_CB_POST_EXEC_DTL, (tws_oid_t)req_id, NULL);
	return;
}

void
__wrapper::execDetailsEnd(int req_id)
{
	tws_t tws = WRP_TWS(this);

	POST_CB(tws, TWS_CB_POST_EXEC_DTL_END, (tws_oid_t)req_id, NULL);
	return;
}

void
__wrapper::updateAccountValue(
	const IB::IBString&, const IB::IBString&,
	const IB::IBString&, const IB::IBString&)
{
	return;
}

void
__wrapper::updatePortfolio(
	const IB::Contract&, int,
	double, double, double, double, double,
	const IB::IBString&)
{
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


/* infra */
static struct tws_infra_clo_s infra_clo;

#define __INFRA_CB(x, what, _oid_, _code_, _data_)	\
	if (LIKELY(x->infra_cb != NULL)) {		\
		infra_clo.oid = _oid_;			\
		infra_clo.code = _code_;		\
		infra_clo.data = _data_;		\
		x->infra_cb(x, what, infra_clo);	\
	} else while (0)
#define INFRA_CB(x, what, args...)	__INFRA_CB(x, what, args)

void
__wrapper::error(const int id, const int code, const IB::IBString msg)
{
	tws_t tws = WRP_TWS(this);

	INFRA_CB(tws, TWS_CB_INFRA_ERROR, (tws_oid_t)id, code, msg.c_str());
	return;
}

void
__wrapper::winError(const IB::IBString &str, int code)
{
	tws_t tws = WRP_TWS(this);

	INFRA_CB(tws, TWS_CB_INFRA_ERROR, 0, code, str.c_str());
	return;
}

void
__wrapper::connectionClosed(void)
{
	tws_t tws = WRP_TWS(this);

	INFRA_CB(tws, TWS_CB_INFRA_CONN_CLOSED, 0, 0, 0);
	return;
}


/* stuff that doesn't do calling-back at all */
void
__wrapper::currentTime(long int time)
{
/* not public */
	this->time = time;
	return;
}

void
__wrapper::nextValidId(IB::OrderId oid)
{
/* not public */
	this->next_oid = oid;
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
__wrapper::deltaNeutralValidation(int, const IB::UnderComp&)
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
rset_tws(tws_t tws)
{
	TWS_PRIV_WRP(tws)->time = 0;
	TWS_PRIV_WRP(tws)->next_oid = 0;
	return;
}

int
init_tws(tws_t tws)
{
	tws->priv = new __wrapper();
	rset_tws(tws);
	TWS_PRIV_WRP(tws)->cli = new IB::EPosixClientSocket(TWS_PRIV_WRP(tws));

	/* just so we know who we are */
	TWS_PRIV_WRP(tws)->tws = tws;
	return 0;
}

int
fini_tws(tws_t tws)
{
	if (tws->priv == NULL) {
		// all's done innit
		return 0;
	}
	tws_disconnect(tws);
	/* wipe our context off the face of this earth */
	rset_tws(tws);

	TWS_PRIV_WRP(tws)->tws = NULL;
	if (TWS_PRIV_CLI(tws)) {
		delete TWS_PRIV_CLI(tws);
		TWS_PRIV_WRP(tws)->cli = NULL;
	}
	if (TWS_PRIV_WRP(tws)) {
		delete TWS_PRIV_WRP(tws);
	}
	tws->priv = NULL;
	return 0;
}

int
tws_connect(tws_t tws, const char *host, uint16_t port, int client)
{
	int rc;

	if (UNLIKELY(tws->priv == NULL)) {
		return - 1;
	}

#if defined TWSAPI_IPV6
	rc = TWS_PRIV_CLI(tws)->eConnect2(host, port, client, AF_UNSPEC);
#else  // !TWSAPI_IPV6
	rc = TWS_PRIV_CLI(tws)->eConnect(host, port, client);
#endif	// TWSAPI_IPV6

	if (rc == 0) {
		/* set the errno? */
		return -1;
	}

	// just request a lot of buggery here
	TWS_PRIV_CLI(tws)->reqCurrentTime();
	return TWS_PRIV_CLI(tws)->fd();
}

int
tws_disconnect(tws_t tws)
{
	if (UNLIKELY(tws->priv == NULL)) {
		return -1;
	}
	TWS_PRIV_CLI(tws)->eDisconnect();
	return 0;
}

int
tws_recv(tws_t tws)
{
	TWS_PRIV_CLI(tws)->onReceive();
	return TWS_PRIV_CLI(tws)->isSocketOK() ? 0 : -1;
}

int
tws_send(tws_t tws)
{
	if (TWS_PRIV_CLI(tws)->isOutBufferEmpty()) {
		return 0;
	}
	TWS_PRIV_CLI(tws)->onSend();
	return TWS_PRIV_CLI(tws)->isSocketOK() ? 0 : -1;
}

int
tws_ready_p(tws_t tws)
{
/* inspect TWS and return non-nil if requests to the tws can be made */
	return TWS_PRIV_WRP(tws)->next_oid > 0 && TWS_PRIV_WRP(tws)->time > 0;
}

/* gen-tws-wrapper.cpp ends here */
