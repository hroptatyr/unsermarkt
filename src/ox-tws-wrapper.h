/*** ox-tws-wrapper.h -- order execution through tws
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
#if !defined INCLUDED_ox_tws_wrapper_h_
#define INCLUDED_ox_tws_wrapper_h_

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

typedef struct my_tws_s *my_tws_t;
typedef struct tws_order_s *tws_order_t;
/* abstract type for ib contracts */
typedef void *tws_instr_t;

typedef unsigned int tws_oid_t;

struct my_tws_s {
	tws_oid_t next_oid;
	unsigned int time;
	void *wrp;
	void *cli;
	void *oq;
};

struct tws_order_s {
	/** 0 means let the tws decide */
	tws_oid_t oid;
	/** ib's notion of this order */
	tws_instr_t c;
	/** this will be generally a sl1t_t */
	void *o;
};


extern void *logerr;

extern int init_tws(my_tws_t);
extern int fini_tws(my_tws_t);

extern int tws_connect(my_tws_t, const char *host, uint16_t port, int client);
extern int tws_disconnect(my_tws_t);

extern int tws_recv(my_tws_t);
extern int tws_send(my_tws_t);

/* testing */
extern int tws_put_order(my_tws_t, tws_order_t);
extern int tws_get_order(my_tws_t, tws_order_t, tws_oid_t oid);

/* builder and dismantler for ib contracts */
extern tws_instr_t tws_assemble_instr(const char *sym);
extern void tws_disassemble_instr(tws_instr_t);

extern int tws_reconcile(my_tws_t);

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ox_tws_wrapper_h_ */
