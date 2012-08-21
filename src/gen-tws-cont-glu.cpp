/*** gen-tws-cont-glu.cpp -- generic tws c api contract builder
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
#include <string.h>
#include <stdio.h>
#include <twsapi/Contract.h>
#include "iso4217.h"

#include "proto-tx-ns.h"
#include "proto-fixml-attr.h"
#include "proto-twsxml-attr.h"

#include "gen-tws-cont.h"
#include "gen-tws-cont-glu.h"


static int
tws_cont_tx(tws_cont_t tgt, unsigned int aid, const char *val)
{
	IB::Contract *c = (IB::Contract*)tgt;

	switch ((tws_xml_aid_t)aid) {
	case TX_ATTR_SYMBOL:
		c->symbol = std::string(val);
		break;
	case TX_ATTR_CURRENCY:
		c->currency = std::string(val);
		break;
	case TX_ATTR_SECTYPE:
		c->secType = std::string(val);
		break;
	case TX_ATTR_EXCHANGE:
		c->exchange = std::string(val);
		break;
	default:
		break;
	}
	return 0;
}

static int
tws_cont_fix(tws_cont_t tgt, unsigned int aid, const char *val)
{
	IB::Contract *c = (IB::Contract*)tgt;

	switch ((fixml_aid_t)aid) {
	case FIX_ATTR_CCY:
		c->currency = std::string(val);
		break;
	case FIX_ATTR_EXCH:
		c->exchange = std::string(val);
		break;
	case FIX_ATTR_SYM:
		c->localSymbol = std::string(val);
		break;
	case FIX_ATTR_SECTYP:
		if (!strcmp(val, "FXSPOT")) {
			c->secType = std::string("CASH");
		}
		break;
	default:
		break;
	}
	return 0;
}

static int
tws_cont_symstr(tws_cont_t tgt, unsigned int, const char *val)
{
// this one only supports FX pairs and metals at the moment
	static const char fxvirt[] = "IDEALPRO";
	static const char fxconv[] = "FXCONV";
	IB::Contract *c = (IB::Contract*)tgt;
	const_iso_4217_t bas;
	const_iso_4217_t trm;
	const char *exch = fxvirt;;

	if ((bas = find_iso_4217_by_name(val)) == NULL) {
		return -1;
	}
	switch (*(val += 3)) {
	case '\000':
		/* oooh, just one ccy */
		exch = fxconv;

		switch (iso_4217_id(bas)) {
		case ISO_4217_EUR_IDX:
		case ISO_4217_GBP_IDX:
		case ISO_4217_AUD_IDX:
		case ISO_4217_NZD_IDX:
			trm = ISO_4217_USD;
			break;
		case ISO_4217_USD_IDX:
			// um, USDUSD?  make it EURUSD
			bas = ISO_4217_EUR;
			trm = ISO_4217_USD;
			break;
		default:
			// assume USDxxx
			trm = bas;
			bas = ISO_4217_USD;
			break;
		}
		goto special;
	case '.':
	case '/':
		// stuff like EUR/USD or EUR.USD
		val++;
		break;
	default:
		break;
	}
	if ((trm = find_iso_4217_by_name(val)) == NULL) {
		return -1;
	}

	switch (iso_4217_id(bas)) {
	case ISO_4217_XAU_IDX:
	case ISO_4217_XAG_IDX:
	case ISO_4217_XPT_IDX:
	case ISO_4217_XPD_IDX:
		// make sure its XXXUSD
		if (iso_4217_id(trm) != ISO_4217_USD_IDX) {
			return -1;
		}
		switch (iso_4217_id(bas)) {
		case ISO_4217_XAU_IDX:
			c->symbol = std::string("XAUUSD");
			break;
		case ISO_4217_XAG_IDX:
			c->symbol = std::string("XAGUSD");
			break;
		case ISO_4217_XPT_IDX:
			c->symbol = std::string("XPTUSD");
			break;
		case ISO_4217_XPD_IDX:
			c->symbol = std::string("XPDUSD");
			break;
		}
		c->currency = std::string("USD");
		c->secType = std::string("CMDTY");
		c->exchange = std::string("");
		break;

	default:
	special:
		// otherwise we're pretty well off with a ccy pair
		c->symbol = std::string(bas->sym);
		c->currency = std::string(trm->sym);
		c->secType = std::string("CASH");
		c->exchange = std::string(exch);
		break;
	}
	return 0;
}


// out converters
static ssize_t
tws_cont_to_fix(char *restrict buf, size_t bsz, tws_cont_t src)
{
	IB::Contract *c = (IB::Contract*)src;
	const char *sym = c->localSymbol.c_str();
	const long int conid = c->conId;
	const char *sectyp = c->secType.c_str();
	const char *exch = c->exchange.c_str();

	return snprintf(buf, bsz, "\
<SecDef><Instrmt\
 Sym=\"%s\"\
 ID=\"%ld\" Src='M'\
 SecTyp=\"%s\"\
 Exch=\"%s\"/>\
</SecDef>", sym, conid, sectyp, exch);
}


tws_cont_t
tws_make_cont(void)
{
	return (tws_cont_t)new IB::Contract;
}

tws_cont_t
tws_dup_cont(tws_const_cont_t x)
{
	const IB::Contract *ibc = (const IB::Contract*)x;
	IB::Contract *res = new IB::Contract;

	*res = *ibc;
	return (tws_cont_t)res;
}

void
tws_free_cont(tws_cont_t c)
{
	if (c) {
		delete (IB::Contract*)c;
	}
	return;
}

tws_sdef_t
tws_make_sdef(void)
{
	return (tws_sdef_t)new IB::ContractDetails;
}

void
tws_free_sdef(tws_sdef_t cd)
{
	if (cd) {
		delete (IB::ContractDetails*)cd;
	}
	return;
}

tws_sdef_t
tws_dup_sdef(tws_const_sdef_t x)
{
	const IB::ContractDetails *ibc = (const IB::ContractDetails*)x;
	IB::ContractDetails *res = new IB::ContractDetails;

	*res = *ibc;
	return (tws_sdef_t)res;
}

tws_const_cont_t
tws_sdef_get_cont(tws_const_sdef_t x)
{
	const IB::ContractDetails *ibcd = (const IB::ContractDetails*)x;
	return (tws_const_cont_t)&ibcd->summary;
}


int
tws_cont_x(tws_cont_t tgt, unsigned int nsid, unsigned int aid, const char *val)
{
	switch ((tx_nsid_t)nsid) {
	case TX_NS_TWSXML_0_1:
		return tws_cont_tx(tgt, aid, val);
	case TX_NS_FIXML_5_0:
		return tws_cont_fix(tgt, aid, val);
	case TX_NS_SYMSTR:
		return tws_cont_symstr(tgt, aid, val);
	default:
		return -1;
	}
}

// until there's a better place
const char*
tws_cont_nick(tws_const_cont_t cont)
{
	static char nick[64];
	const IB::Contract *c = (const IB::Contract*)cont;

	if (c->localSymbol.length() > 0) {
		return c->localSymbol.c_str();
	} else if (c->secType == std::string("CASH")) {
		const char *bas = c->symbol.c_str();
		const char *trm = c->currency.c_str();

		if (bas && *bas) {
			memcpy(nick, bas, 3);
			nick[3] = '.';
			memcpy(nick + 4, trm, 3);
			nick[7] = '\0';
		} else if (trm && *trm) {
			memcpy(nick, trm, 3);
			nick[3] = '\0';
		}
		return nick;
	}
	return NULL;
}

ssize_t
tws_cont_y(char *restrict buf, size_t bsz, unsigned int nsid, tws_cont_t c)
{
	switch ((tx_nsid_t)nsid) {
	case TX_NS_TWSXML_0_1:
		return 0;
	case TX_NS_FIXML_5_0:
		return tws_cont_to_fix(buf, bsz, c);
	case TX_NS_SYMSTR:
		return 0;
	default:
		return -1;
	}
}

// gen-tws-cont-glu.cpp ends here
