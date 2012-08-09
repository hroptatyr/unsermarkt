/*** gen-tws-order-glu.cpp -- generic tws c api order builder
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

#include "proto-tx-ns.h"
#include "proto-fixml-attr.h"
#include "proto-twsxml-attr.h"

#include "gen-tws-cont.h"
#include "gen-tws-order.h"
#include "gen-tws-order-glu.h"

static int
tws_order_tx(tws_order_t tgt, unsigned int aid, const char *val)
{
	IB::Order *o = (IB::Order*)tgt;

	switch ((tws_xml_aid_t)aid) {
	case TX_ATTR_ACTION:
		o->action = std::string(val);
		break;
	case TX_ATTR_TIF:
		o->tif = std::string(val);
		break;
	case TX_ATTR_ORDERTYPE:
		o->orderType = std::string(val);
		break;
	case TX_ATTR_OCAGROUP:
		o->ocaGroup = std::string(val);
		break;
	case TX_ATTR_ACCOUNT:
		o->account = std::string(val);
		break;

	case TX_ATTR_TOTALQUANTITY: {
		char *p;
		if (!(o->totalQuantity = strtol(val, &p, 10)) && p == NULL) {
			return -1;
		}
		break;
	}
	case TX_ATTR_AUXPRICE: {
		char *p;
		if (!(o->auxPrice = strtod(val, &p)) && p == NULL) {
			return -1;
		}
		break;
	}
	case TX_ATTR_LMTPRICE: {
		char *p;
		if (!(o->lmtPrice = strtod(val, &p)) && p == NULL) {
			return -1;
		}
		break;
	}
	default:
		return -1;
	}
	return 0;
}


tws_order_t
tws_make_order(void)
{
	return (tws_order_t)new IB::Order;
}

void
tws_free_order(tws_order_t o)
{
	if (o) {
		delete (IB::Order*)o;
	}
	return;
}

int
tws_order_x(tws_order_t tgt, unsigned int nsid, unsigned int aid, const char *v)
{
	switch ((tx_nsid_t)nsid) {
	case TX_NS_TWSXML_0_1:
		return tws_order_tx(tgt, aid, v);
	case TX_NS_FIXML_5_0:
	default:
		return -1;
	}
}

int
tws_order_sl1t(tws_order_t o, const void *data)
{
	IB::Order *ibo = (IB::Order*)o;
	const_sl1t_t s = (const_sl1t_t)data;

	if (ibo == NULL || s == NULL) {
		// bugger off right away
		return -1;
	}

	switch (sl1t_ttf(s)) {
	case SL1T_TTF_BID:
	case SL2T_TTF_BID:
		ibo->action = "BUY";
		break;
	case SL1T_TTF_ASK:
	case SL2T_TTF_ASK:
		ibo->action = "SELL";
		break;
	default:
		// oh oh
		ibo->action = "CANCEL";
		break;
	}
	// make sure we pick the right order type
	if (s->pri == SL1T_PRC_MKT) {
		ibo->orderType = "MKT";
	} else {
		m30_t m = ffff_m30_get_ui32(s->pri);

		ibo->orderType = "LMT";
		ibo->lmtPrice = ffff_m30_d(m);
	}
	// quantity is always important
	if (s->qty) {
		m30_t m = ffff_m30_get_ui32(s->qty);

		// as this is currency only, we're probably talking lots
		ibo->totalQuantity = ffff_m30_d(m);
	} else {
		/* cancel? */
		ibo->action = "CANCEL";
		ibo->totalQuantity = 0;
	}
	return 0;
}

int
tws_check_order(tws_order_t order, tws_const_cont_t cont)
{
// grrrr
// quick sanity check, mainly for fx lots
	const IB::Contract *ib_c = (const IB::Contract*)cont;
	IB::Order *ib_o = (IB::Order*)order;

	if (strcmp(ib_c->secType.c_str(), "CASH") == 0) {
		if (ib_o->totalQuantity < 1000) {
			// they probably mean lots
			ib_o->totalQuantity *= 100000;
		}
	}
	return 0;
}

// gen-tws-order-glu.cpp ends here