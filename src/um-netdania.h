/*** netdania.h -- leech some netdania resources
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
#if !defined INCLUDED_netdania_h_
#define INCLUDED_netdania_h_

/* see http://www.netdania.com/Products/live-streaming-currency-exchange-rates/real-time-forex-charts/FinanceChart.aspx */
typedef enum {
	ND_SUB_UNK = 0,
	ND_SUB_CLOSE = 1,
	ND_SUB_HIGH = 2,
	ND_SUB_LOW = 3,
	ND_SUB_OPEN = 4,
	ND_SUB_VOL = 5,
	ND_SUB_LAST = 6,
	ND_SUB_OI = 7,
	ND_SUB_LSZ = 8,

	ND_SUB_BID = 10,
	ND_SUB_ASK = 11,
	ND_SUB_BSZ = 12,
	ND_SUB_ASZ = 13,
	ND_SUB_CHG_ABS = 14,
	ND_SUB_CHG_PCT = 15,
	ND_SUB_TRA = 16,
	ND_SUB_TIME = 17,

	ND_SUB_STL = 20,
	ND_SUB_52W_HIGH = 21,
	ND_SUB_52W_LOW = 22,
	ND_SUB_AGENT = 23,

	ND_SUB_NAME = 25,

	ND_SUB_YCHG_PCT = 32,

	ND_SUB_ISIN = 39,

	ND_SUB_ONBID = 47,
	ND_SUB_ONASK = 48,
	ND_SUB_SNBID = 49,
	ND_SUB_SNASK = 50,
	ND_SUB_TNBID = 51,
	ND_SUB_TNASK = 52,
	ND_SUB_1WBID = 53,
	ND_SUB_1WASK = 54,
	ND_SUB_2WBID = 55,
	ND_SUB_2WASK = 56,
	ND_SUB_3WBID = 57,
	ND_SUB_3WASK = 58,
	ND_SUB_1MBID = 59,
	ND_SUB_1MASK = 60,
	ND_SUB_2MBID = 61,
	ND_SUB_2MASK = 62,
	ND_SUB_3MBID = 63,
	ND_SUB_3MASK = 64,
	ND_SUB_4MBID = 65,
	ND_SUB_4MASK = 66,
	ND_SUB_5MBID = 67,
	ND_SUB_5MASK = 68,
	ND_SUB_6MBID = 69,
	ND_SUB_6MASK = 70,
	ND_SUB_7MBID = 71,
	ND_SUB_7MASK = 72,
	ND_SUB_8MBID = 73,
	ND_SUB_8MASK = 74,
	ND_SUB_9MBID = 75,
	ND_SUB_9MASK = 76,
	ND_SUB_10MBID = 77,
	ND_SUB_10MASK = 78,
	ND_SUB_11MBID = 79,
	ND_SUB_11MASK = 80,
	ND_SUB_1YBID = 81,
	ND_SUB_1YASK = 82,

	ND_SUB_BIDASK = 106,

	ND_SUB_NANO = 0x03e8,
	ND_SUB_MSTIME = 0x03ed,
} nd_sub_t;

#endif	/* INCLUDED_netdania_h_ */
