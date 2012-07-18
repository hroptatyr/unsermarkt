/*** quo-tws-wrapper.cpp -- quotes and trades from tws
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

#include "quo-tws-wrapper.h"
#include "quo-tws-private.h"
#include "wrp-debug.h"

#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define glu_debug(args...)
# define wrp_debug(args...)
# define assert(x)
#endif	/* DEBUG_FLAG */

#define TWSAPI_IPV6		1

#define QTY_MULTIPLIER		(100000)
#define QTY_MULTIPLIER_D	((double)QTY_MULTIPLIER)

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

void 
__wrapper::tickPrice(IB::TickerId id, IB::TickType fld, double pri, int autop)
{
	WRP_DEBUG("prc %ld %u %.6f", id, fld, pri);
	return;
}

void
__wrapper::tickSize(IB::TickerId id, IB::TickType fld, int size)
{
	WRP_DEBUG("qty %ld %u %d", id, fld, size);
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

void
__wrapper::orderStatus(
	IB::OrderId, const IB::IBString&,
	int flld, int remn, double avg_fill_prc, int permId,
	int parentId, double last_fill_prc, int clientId,
	const IB::IBString& whyHeld)
{
	return;
}

void
__wrapper::openOrder(
	IB::OrderId oid, const IB::Contract&,
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
__wrapper::winError(const IB::IBString &str, int lastError)
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
	const char *ca = acct_name.c_str();
	const char *ck = key.c_str();
	const char *cv = val.c_str();
	const char *cc = ccy.c_str();

	WRP_DEBUG("acct %s: %s <- %s %s", ca, ck, cv, cc);
	return;
}

void
__wrapper::updatePortfolio(
	const IB::Contract &c, int pos,
	double mkt_prc, double mkt_val, double avg_cost,
	double upnl, double rpnl,
	const IB::IBString &acct_name)
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
	return;
}

void
__wrapper::execDetailsEnd(int req_id)
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
tws_req_quo(my_tws_t foo, tws_instr_t i)
{
	IB::EPosixClientSocket *cli = (IB::EPosixClientSocket*)foo->cli;
	IB::Contract *c = (IB::Contract*)i;
	IB::IBString x = std::string("");

	cli->reqMktData(foo->next_oid++, *c, x, false);
	return cli->isSocketOK() ? 0 : -1;
}

/* quo-tws-wrapper.cpp ends here */
