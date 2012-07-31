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
#include "wrp-debug.h"

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
		wrp_debug(tws, "connection to [%s]:%hu failed", host, port);
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
