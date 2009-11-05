/*** dso-pong.c -- pong service
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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
#endif	/* HAVE_CONFIG_H */
#include "unserding.h"
#include "unserding-ctx.h"
#include "unserding-nifty.h"
#include "unserding-private.h"
#include "seria-proto-glue.h"
#include "svc-pong.h"

#define TRUNC_HOST_NAME_LEN	16

static ud_handle_t my_hdl;
static size_t my_hnmlen;
static char my_hname[TRUNC_HOST_NAME_LEN];

static void
hrclock_stamp(struct timespec *ts)
{
	clock_gettime(CLOCK_REALTIME, ts);
	return;
}

static void
ping(job_t j)
{
	struct udpc_seria_s sctx;
	struct timespec ts;

	/* clear out the packet */
	clear_pkt(&sctx, j);
	/* escrow hostname, mac-addr, score and time */
	hrclock_stamp(&ts);
	udpc_seria_add_str(&sctx, my_hname, my_hnmlen);
	udpc_seria_add_ui32(&sctx, ts.tv_sec);
	udpc_seria_add_ui32(&sctx, ts.tv_nsec);
	udpc_seria_add_byte(&sctx, (ud_pong_score_t)my_hdl->score);
	/* off we go */
	send_pkt(&sctx, j);
	return;
}

static void
pong(job_t UNUSED(j))
{
	UD_DEBUG("spurious pong caught\n");
	return;
}

#if 0
static inline ud_pong_score_t
udctx_score(ud_ctx_t ctx)
{
	return ctx->hdl->score;
}
#endif	/* 0 */


void
dso_pong_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;

	/* obtain our host name */
	(void)gethostname(my_hname, sizeof(my_hname));
	my_hname[sizeof(my_hname)-1] = '\0';
	my_hnmlen = strlen(my_hname);
	/* tick service */
	ud_set_service(UD_SVC_PING, ping, pong);

	/* score has been obtained in init_*_handle() already */
	my_hdl = ctx->hdl;
	UD_DEBUG("dso-pong: nego'd me a score of %d\n", my_hdl->score);
	return;
}

void
dso_pong_LTX_deinit(void *UNUSED(clo))
{
	ud_set_service(UD_SVC_PING, NULL, NULL);
	return;
}

/* dso-pong.c ends here */
