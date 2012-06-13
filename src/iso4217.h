/*** iso4217.h -- currency symbols
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@fresse.org>
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

#if !defined INCLUDED_iso4217_h_
#define INCLUDED_iso4217_h_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct iso_4217_s *iso_4217_t;
typedef const struct iso_4217_s *const_iso_4217_t;
/* for code where pointers are inapt provide an index */
typedef unsigned int iso_4217_id_t;
/* the official currency code */
typedef short unsigned int iso_4217_code_t;
/* the official currency abbreviated name, iso 3166 + one character */
typedef const char iso_4217_sym_t[4];

/**
 * Structure for iso4217 symbols and codes.
 * This thing is a pun really, cast to char* to use it as symbol directly.
 * The symbol strings are nul terminated, so occupy exactly 4 bytes.
 * The code numbers should be padded with naughts to the left if used in
 * official contexts. */
struct iso_4217_s {
	/** 4217 symbol, this is an iso3166 country code plus one character */
	const char sym[4];
	/** 4217 code */
	const iso_4217_code_t cod:10;
	/* exponent (of 10) to get to the minor currency, 2 = 100, 3 = 1000 */
	const signed char exp:4;
	/* official name, VLA(!), not really though see alignment */
	const char *name;
} __attribute__((aligned(64)));

/**
 * ISO4217 symbols and code. */
extern const struct iso_4217_s iso_4217[];


#define ISO_4217(_x)		((const_iso_4217_t)&iso_4217[_x])

#define ISO_4217_SYM(_x)	((const char*)&iso_4217[_x].sym)

static inline const char __attribute__((always_inline))*
iso_4217_sym(iso_4217_id_t slot)
{
	return ISO_4217_SYM(slot);
}

#define ISO_4217_COD(_x)	((short unsigned int)iso_4217[_x].cod)

static inline iso_4217_code_t __attribute__((always_inline))
iso_4217_cod(iso_4217_id_t slot)
{
	return ISO_4217_COD(slot);
}

#define ISO_4217_EXP(_x)	((signed char)iso_4217[_x].exp)

static inline signed char __attribute__((always_inline))
iso_4217_exp(iso_4217_id_t slot)
{
	return ISO_4217_EXP(slot);
}

#define ISO_4217_NAME(_x)	((const char*)iso_4217[_x].name)

static inline const char __attribute__((always_inline))*
iso_4217_name(iso_4217_id_t slot)
{
	return ISO_4217_NAME(slot);
}

/**
 * Return the id (the index into the global 4217 array) of PTR. */
static inline iso_4217_id_t __attribute__((always_inline))
iso_4217_id(const_iso_4217_t ptr)
{
	return (iso_4217_id_t)(ptr - iso_4217);
}

/**
 * Return a iso_4217_t object given NAME. */
extern const_iso_4217_t find_iso_4217_by_name(const char *name);


/* frequently used currencies */
/* EUR */
#define ISO_4217_EUR_IDX	((iso_4217_id_t)48)
#define ISO_4217_EUR		(ISO_4217(ISO_4217_EUR_IDX))
#define ISO_4217_EUR_SYM	(ISO_4217_SYM(ISO_4217_EUR_IDX))
/* USD */
#define ISO_4217_USD_IDX	((iso_4217_id_t)149)
#define ISO_4217_USD		(ISO_4217(ISO_4217_USD_IDX))
#define ISO_4217_USD_SYM	(ISO_4217_SYM(ISO_4217_USD_IDX))
/* GBP */
#define ISO_4217_GBP_IDX	((iso_4217_id_t)51)
#define ISO_4217_GBP		(ISO_4217(ISO_4217_GBP_IDX))
#define ISO_4217_GBP_SYM	(ISO_4217_SYM(ISO_4217_GBP_IDX))
/* CAD */
#define ISO_4217_CAD_IDX	((iso_4217_id_t)26)
#define ISO_4217_CAD		(ISO_4217(ISO_4217_CAD_IDX))
#define ISO_4217_CAD_SYM	(ISO_4217_SYM(ISO_4217_CAD_IDX))
/* AUD */
#define ISO_4217_AUD_IDX	((iso_4217_id_t)7)
#define ISO_4217_AUD		(ISO_4217(ISO_4217_AUD_IDX))
#define ISO_4217_AUD_SYM	(ISO_4217_SYM(ISO_4217_AUD_IDX))
/* NZD */
#define ISO_4217_NZD_IDX	((iso_4217_id_t)110)
#define ISO_4217_NZD		(ISO_4217(ISO_4217_NZD_IDX))
#define ISO_4217_NZD_SYM	(ISO_4217_SYM(ISO_4217_NZD_IDX))
/* KRW */
#define ISO_4217_KRW_IDX	((iso_4217_id_t)78)
#define ISO_4217_KRW		(ISO_4217(ISO_4217_KRW_IDX))
#define ISO_4217_KRW_SYM	(ISO_4217_SYM(ISO_4217_KRW_IDX))
/* JPY */
#define ISO_4217_JPY_IDX	((iso_4217_id_t)72)
#define ISO_4217_JPY		(ISO_4217(ISO_4217_JPY_IDX))
#define ISO_4217_JPY_SYM	(ISO_4217_SYM(ISO_4217_JPY_IDX))
/* INR */
#define ISO_4217_INR_IDX	((iso_4217_id_t)66)
#define ISO_4217_INR		(ISO_4217(ISO_4217_INR_IDX))
#define ISO_4217_INR_SYM	(ISO_4217_SYM(ISO_4217_INR_IDX))
/* HKD */
#define ISO_4217_HKD_IDX	((iso_4217_id_t)59)
#define ISO_4217_HKD		(ISO_4217(ISO_4217_HKD_IDX))
#define ISO_4217_HKD_SYM	(ISO_4217_SYM(ISO_4217_HKD_IDX))
/* CNY */
#define ISO_4217_CNY_IDX	((iso_4217_id_t)33)
#define ISO_4217_CNY		(ISO_4217(ISO_4217_CNY_IDX))
#define ISO_4217_CNY_SYM	(ISO_4217_SYM(ISO_4217_CNY_IDX))
/* RUB */
#define ISO_4217_RUB_IDX	((iso_4217_id_t)122)
#define ISO_4217_RUB		(ISO_4217(ISO_4217_RUB_IDX))
#define ISO_4217_RUB_SYM	(ISO_4217_SYM(ISO_4217_RUB_IDX))
/* SEK */
#define ISO_4217_SEK_IDX	((iso_4217_id_t)128)
#define ISO_4217_SEK		(ISO_4217(ISO_4217_SEK_IDX))
#define ISO_4217_SEK_SYM	(ISO_4217_SYM(ISO_4217_SEK_IDX))
/* NOK */
#define ISO_4217_NOK_IDX	((iso_4217_id_t)108)
#define ISO_4217_NOK		(ISO_4217(ISO_4217_NOK_IDX))
#define ISO_4217_NOK_SYM	(ISO_4217_SYM(ISO_4217_NOK_IDX))
/* DKK */
#define ISO_4217_DKK_IDX	((iso_4217_id_t)41)
#define ISO_4217_DKK		(ISO_4217(ISO_4217_DKK_IDX))
#define ISO_4217_DKK_SYM	(ISO_4217_SYM(ISO_4217_DKK_IDX))
/* ISK */
#define ISO_4217_ISK_IDX	((iso_4217_id_t)69)
#define ISO_4217_ISK		(ISO_4217(ISO_4217_ISK_IDX))
#define ISO_4217_ISK_SYM	(ISO_4217_SYM(ISO_4217_ISK_IDX))
/* EEK */
#define ISO_4217_EEK_IDX	((iso_4217_id_t)44)
#define ISO_4217_EEK		(ISO_4217(ISO_4217_EEK_IDX))
#define ISO_4217_EEK_SYM	(ISO_4217_SYM(ISO_4217_EEK_IDX))
/* HUF */
#define ISO_4217_HUF_IDX	((iso_4217_id_t)63)
#define ISO_4217_HUF		(ISO_4217(ISO_4217_HUF_IDX))
#define ISO_4217_HUF_SYM	(ISO_4217_SYM(ISO_4217_HUF_IDX))
/* MXN */
#define ISO_4217_MXN_IDX	((iso_4217_id_t)101)
#define ISO_4217_MXN		(ISO_4217(ISO_4217_MXN_IDX))
#define ISO_4217_MXN_SYM	(ISO_4217_SYM(ISO_4217_MXN_IDX))
/* BRL */
#define ISO_4217_BRL_IDX	((iso_4217_id_t)20)
#define ISO_4217_BRL		(ISO_4217(ISO_4217_BRL_IDX))
#define ISO_4217_BRL_SYM	(ISO_4217_SYM(ISO_4217_BRL_IDX))
/* CLP */
#define ISO_4217_CLP_IDX	((iso_4217_id_t)32)
#define ISO_4217_CLP		(ISO_4217(ISO_4217_CLP_IDX))
#define ISO_4217_CLP_SYM	(ISO_4217_SYM(ISO_4217_CLP_IDX))
/* CHF */
#define ISO_4217_CHF_IDX	((iso_4217_id_t)29)
#define ISO_4217_CHF		(ISO_4217(ISO_4217_CHF_IDX))
#define ISO_4217_CHF_SYM	(ISO_4217_SYM(ISO_4217_CHF_IDX))
/* CHF */
#define ISO_4217_SGD_IDX	((iso_4217_id_t)129)
#define ISO_4217_SGD		(ISO_4217(ISO_4217_SGD_IDX))
#define ISO_4217_SGD_SYM	(ISO_4217_SYM(ISO_4217_SGD_IDX))

/* precious metals */
/* gold */
#define ISO_4217_XAU_IDX	((iso_4217_id_t)154)
#define ISO_4217_XAU		(ISO_4217(ISO_4217_XAU_IDX))
#define ISO_4217_XAU_SYM	(ISO_4217_SYM(ISO_4217_XAU_IDX))
/* silver */
#define ISO_4217_XAG_IDX	((iso_4217_id_t)153)
#define ISO_4217_XAG		(ISO_4217(ISO_4217_XAG_IDX))
#define ISO_4217_XAG_SYM	(ISO_4217_SYM(ISO_4217_XAG_IDX))
/* platinum */
#define ISO_4217_XPT_IDX	((iso_4217_id_t)165)
#define ISO_4217_XPT		(ISO_4217(ISO_4217_XPT_IDX))
#define ISO_4217_XPT_SYM	(ISO_4217_SYM(ISO_4217_XPT_IDX))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_iso4217_h_ */
