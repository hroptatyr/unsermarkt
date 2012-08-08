/*** gen-tws.h -- generic tws c api
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
#if !defined INCLUDED_gen_tws_h_
#define INCLUDED_gen_tws_h_

#include <stdbool.h>

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

typedef struct tws_s *tws_t;

typedef unsigned int tws_oid_t;
typedef unsigned int tws_time_t;
typedef unsigned int tws_tick_type_t;

typedef enum {
	TWS_CB_UNK,

	/* PREs */
	/* .val types */
	TWS_CB_PRE_PRICE,
	TWS_CB_PRE_SIZE,
	TWS_CB_PRE_GENERIC,
	TWS_CB_PRE_SNAP_END,
	/* .i types */
	TWS_CB_PRE_MKT_DATA_TYPE,
	/* .str types */
	TWS_CB_PRE_STRING,
	TWS_CB_PRE_FUND_DATA,
	/* .data types */
	TWS_CB_PRE_CONT_DTL,
	TWS_CB_PRE_CONT_DTL_END,
	TWS_CB_PRE_OPTION,
	TWS_CB_PRE_EFP,
	TWS_CB_PRE_UPD_MKT_DEPTH,
	TWS_CB_PRE_HIST_DATA,
	TWS_CB_PRE_REALTIME_BAR,

	/* TRDs */
	TWS_CB_TRD_ORD_STATUS,
	TWS_CB_TRD_OPEN_ORD,
	TWS_CB_TRD_OPEN_ORD_END,

	/* POSTs */
	TWS_CB_POST_EXEC_DTL,
	TWS_CB_POST_EXEC_DTL_END,
	TWS_CB_POST_MNGD_AC,
	TWS_CB_POST_ACUP,
	TWS_CB_POST_ACUP_END,

	/* INFRAs */
	TWS_CB_INFRA_ERROR,
	TWS_CB_INFRA_CONN_CLOSED,
	TWS_CB_INFRA_READY,
} tws_cb_t;

/* tick types */
#define TWS_TICK_TYPE_BID	(1)
#define TWS_TICK_TYPE_BSZ	(0)
#define TWS_TICK_TYPE_ASK	(2)
#define TWS_TICK_TYPE_ASZ	(3)
#define TWS_TICK_TYPE_TRA	(4)
#define TWS_TICK_TYPE_TSZ	(5)
#define TWS_TICK_TYPE_HIGH	(6)
#define TWS_TICK_TYPE_LOW	(7)
#define TWS_TICK_TYPE_VOL	(8)
/* close price at the end of day */
#define TWS_TICK_TYPE_CLOSE	(9)
/* opening price at the beginning of the day */
#define TWS_TICK_TYPE_OPEN	(14)

/* we split the callbacks into 4 big groups, just like fix:
 * pre_trade, trade, post_trade, infra */
struct tws_pre_clo_s {
	tws_oid_t oid;
	tws_tick_type_t tt;
	union {
		double val;
		const char *str;
		const void *data;
		int i;
	};
};

struct tws_trd_clo_s {
	tws_oid_t oid;
	const void *data;
};

struct tws_post_clo_s {
	tws_oid_t oid;
	const void *data;
};

struct tws_infra_clo_s {
	tws_oid_t oid;
	tws_oid_t code;
	const void *data;
};

struct tws_s {
	void *priv;
	void(*pre_cb)(tws_t, tws_cb_t, struct tws_pre_clo_s);
	void(*trd_cb)(tws_t, tws_cb_t, struct tws_trd_clo_s);
	void(*post_cb)(tws_t, tws_cb_t, struct tws_post_clo_s);
	void(*infra_cb)(tws_t, tws_cb_t, struct tws_infra_clo_s);

	/* flexible array at the end, so users can extend this */
	char user[0];
};


/* structs specific to some callbacks data field */
struct tws_post_acup_clo_s {
	const char *ac_name;
	const void *cont;
	double pos;
	double val;
};

struct tws_post_acup_end_clo_s {
	const char *ac_name;
};


/* infra structure requests */
extern int init_tws(tws_t, int sock, int client);
extern int fini_tws(tws_t);
extern void rset_tws(tws_t);

extern int tws_connect(tws_t, const char *host, uint16_t port, int client);
extern int tws_disconnect(tws_t);

#if defined HAVE_TWSAPI_HANDSHAKE
/* handshake api */
extern int tws_started_p(tws_t);
extern int tws_start(tws_t);
extern int tws_stop(tws_t);
#endif	/* HAVE_TWSAPI_HANDSHAKE */

extern int tws_recv(tws_t);
extern int tws_send(tws_t);

extern bool tws_needs_recv_p(tws_t);
extern bool tws_needs_send_p(tws_t);

extern int tws_ready_p(tws_t);

/* pre requests */
/**
 * Subscribe to quote updates under the given id with contract data in DATA. */
extern int tws_req_quo(tws_t, tws_oid_t, const void *data);

/**
 * Unsubscribe from quote updates identified by the given oid. */
extern int tws_rem_quo(tws_t, tws_oid_t);

/**
 * Like `tws_req_quo()' but generate (and return) the oid. */
extern tws_oid_t tws_gen_quo(tws_t, const void *data);


/* trd requests */
/**
 * Place an order with order data DATA on contract CONT ref'd by the oid. */
extern int tws_put_order(tws_t, tws_oid_t, const void *cont, const void *data);

/**
 * Cancel the order with the given oid. */
extern int tws_rem_order(tws_t, tws_oid_t);

/**
 * Like `tws_put_order()` but generate (and return) a suitable oid. */
extern tws_oid_t tws_gen_order(tws_t, const void *cont, const void *data);

/* post requests */
/**
 * Subscribe to account updates for the account in DATA (a string), or
 * if NULL all accounts. */
extern int tws_req_ac(tws_t, const void *data);

/**
 * Unsubscribe from account updates referenced by the account name in DATA. */
extern int tws_rem_ac(tws_t, const void *data);

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_gen_tws_h_ */
