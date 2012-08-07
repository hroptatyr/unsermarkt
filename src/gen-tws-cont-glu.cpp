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
#include <twsapi/Contract.h>

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


tws_cont_t
tws_make_cont(void)
{
	return (tws_cont_t)new IB::Contract;
}

void
tws_free_cont(tws_cont_t c)
{
	if (c) {
		delete (IB::Contract*)c;
	}
	return;
}

int
tws_cont_x(tws_cont_t tgt, unsigned int nsid, unsigned int aid, const char *val)
{
	switch ((tx_nsid_t)nsid) {
	case TX_NS_TWSXML_0_1:
		return tws_cont_tx(tgt, aid, val);
	case TX_NS_FIXML_5_0:
		return tws_cont_fix(tgt, aid, val);
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

		memcpy(nick, bas, 3);
		nick[3] = '.';
		memcpy(nick + 4, trm, 3);
		nick[7] = '\0';
		return nick;
	}
	return NULL;
}

// gen-tws-cont-glu.cpp ends here
