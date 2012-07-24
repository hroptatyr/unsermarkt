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
tws_cont_build(tws_cont_t tgt, const char *slot, const char *val)
{
	IB::Contract *c = (IB::Contract*)tgt;

	if (!strcmp(slot, "symbol")) {
		c->symbol = std::string(val);
	} else if (!strcmp(slot, "currency")) {
		c->currency = std::string(val);
	} else if (!strcmp(slot, "secType")) {
		c->secType = std::string(val);
	} else if (!strcmp(slot, "exchange")) {
		c->exchange = std::string(val);
	}
	return 0;
}

/* tws-cont.cpp ends here */
