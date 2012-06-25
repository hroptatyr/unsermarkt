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
#include <string>

/* the tws api */
#include <twsapi/Contract.h>
#include "ox-tws-wrapper.h"
#include "iso4217.h"

#if defined DEBUG_FLAG
# include <assert.h>
# define OX_DEBUG(args...)	fprintf(LOGERR, args)
#else  /* !DEBUG_FLAG */
# define OX_DEBUG(args...)
# define assert(x)
#endif	/* DEBUG_FLAG */

tws_instr_t
tws_assemble_instr(const char *sym)
{
	IB::Contract *res;
	const_iso_4217_t bas;
	const_iso_4217_t trm;

	if ((bas = find_iso_4217_by_name(sym)) == NULL) {
		return NULL;
	}
	switch (*(sym += 3)) {
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

	// otherwise we're pretty well off with a ccy pair
	res = new IB::Contract();

	res->symbol = std::string(bas->sym);
	res->currency = std::string(trm->sym);
	res->secType = std::string("CASH");
	res->exchange = std::string("IDEALPRO");
	OX_DEBUG("[glue/contract]: created %p\n", res);
	return (tws_instr_t)res;
}

void
tws_disassemble_instr(tws_instr_t ins)
{
	IB::Contract *ibi = (IB::Contract*)ins;

	if (ibi) {
		delete ibi;
	}
	return;
}

/* ox-tws-contract-glue.cpp ends here */
