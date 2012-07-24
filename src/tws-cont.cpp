/*** tws-cont.cpp -- glue between C and tws contracts
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
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <string>

/* the tws api */
#include <twsapi/Contract.h>
#include "proto-twsxml-attr.h"
#include "tws-cont.h"

tws_cont_t
make_cont(void)
{
	IB::Contract *res = new IB::Contract();
	return res;
}

void
free_cont(tws_cont_t c)
{
	IB::Contract *tmp = (IB::Contract*)c;
	delete tmp;
	return;
}

int
tws_cont_build(tws_cont_t tgt, tws_xml_aid_t aid, const char *val)
{
	IB::Contract *c = (IB::Contract*)tgt;

	switch (aid) {
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

const char*
tws_cont_nick(tws_cont_t cont)
{
	static char nick[64];
	IB::Contract *c = (IB::Contract*)cont;

	if (c->localSymbol.length() > 0) {
		return c->localSymbol.c_str();
	} else if (c->secType == std::string("CASH")) {
		const char *bas = c->symbol.c_str();
		const char *trm = c->currency.c_str();
		snprintf(nick, sizeof(nick), "%s.%s", bas, trm);
		return nick;
	}
	return NULL;
}

/* tws-cont.cpp ends here */
