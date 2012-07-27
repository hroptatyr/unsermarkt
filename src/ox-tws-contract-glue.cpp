/*** ox-tws-contract-glue.cpp -- ctor'ing and dtor'ing ib contracts
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

/* the tws api */
#include <twsapi/Contract.h>
#include "ox-tws-wrapper.h"
#include "iso4217.h"
#include "wrp-debug.h"

#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define glu_debug(args...)
# define wrp_debug(args...)
# define assert(x)
#endif	/* DEBUG_FLAG */

tws_instr_t
tws_assemble_instr(const char *sym)
{
	static const char fxvirt[] = "IDEALPRO";
	static const char fxconv[] = "FXCONV";
	IB::Contract *res;
	const_iso_4217_t bas;
	const_iso_4217_t trm;
	const char *exch = fxvirt;;

	if ((bas = find_iso_4217_by_name(sym)) == NULL) {
		return NULL;
	}
	switch (*(sym += 3)) {
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
		sym++;
		break;
	default:
		break;
	}
	if ((trm = find_iso_4217_by_name(sym)) == NULL) {
		return NULL;
	}

special:
	// otherwise we're pretty well off with a ccy pair
	res = new IB::Contract();

	res->symbol = std::string(bas->sym);
	res->currency = std::string(trm->sym);
	res->secType = std::string("CASH");
	res->exchange = std::string(exch);
	glu_debug((void*)res, "created");
	return (tws_instr_t)res;
}

void
tws_disassemble_instr(tws_instr_t ins)
{
	IB::Contract *ibi = (IB::Contract*)ins;

	if (ibi) {
		glu_debug((void*)ibi, "deleting");
		delete ibi;
	}
	return;
}

/* ox-tws-contract-glue.cpp ends here */
