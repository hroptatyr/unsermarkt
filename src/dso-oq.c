/*** dso-oq.c -- order queuing
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include <unserding/unserding-ctx.h>
#include <unserding/unserding-cfg.h>
#include <unserding/module.h>
#include "nifty.h"
/* order matching engine */
#include "oq.h"

#define MOD_PRE		"mod/oq"

/* some forwards and globals */
static int handle_data(int fd, char *msg, size_t msglen);
static void handle_close(int fd);
/* push register */
static void memorise_htpush(int fd);
static void forget_htpush(int fd);
/* push new status to everyone */
static void upstatus(void);
static void prhttphdr(int fd);

static int handle_wsget(int fd, char *msg, size_t msglen);
static int wsclop(const char *msg, size_t msglen);
static int handle_wsclo(int fd, char *msg, size_t msglen);

static umoq_t q = NULL;


/* our connectivity cruft */
#include "dso-oq-con6ity.c"


/* our websocket support */
#include "htws.c"


/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
static int
handle_data(int fd, char *msg, size_t msglen)
{
	static const char get_cookie[] = "GET /";

	if (strncmp(msg, get_cookie, sizeof(get_cookie) - 1) == 0) {
		UM_DEBUG(MOD_PRE ": htws push request\n");
		if (handle_wsget(fd, msg, msglen) < 0) {
			return -1;
		}
		prstatus(fd);
		return 0;

	} else if (wsclop(msg, msglen)) {
		/* websocket closing challenge */
		UM_DEBUG(MOD_PRE ": htws closing request\n");
		return handle_wsclo(fd, msg, msglen);

	} else if (msglen % sizeof(struct umo_s) == 0) {
		/* try and get a bunch of orders */
		for (int i = 0; i < msglen / sizeof(struct umo_s); i++) {
			oq_add_order(q, (umo_t)msg + i);
		}
		upstatus();
		return 0;

	} else {
		UM_DEBUG(MOD_PRE ": unknown crap on the wire\n");
		return -1;
	}
}

static void
handle_close(int fd)
{
	/* delete fd from our htpush cache */
	forget_htpush(fd);
	return;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	UM_DEBUG(MOD_PRE ": loading ...");
	/* connect to scscp and say ehlo */
	oqsock = listener();
	/* set up the IO watcher and timer */
	init_watchers(ctx->mainloop, oqsock);
	UM_DBGCONT("loaded\n");

	/* initialising the order queue */
	if ((settings = udctx_get_setting(ctx)) != NULL) {
		int sid = udcfg_tbl_lookup_i(ctx, settings, "secu_id");
		int fid = udcfg_tbl_lookup_i(ctx, settings, "fund_id");
		UM_DEBUG(MOD_PRE ": found secu_id/fund_id %d/%d\n", sid, fid);
		q = make_oq(sid, fid);
	}
	/* clean up */
	udctx_set_setting(ctx, NULL);
	return;
}

void
reinit(void *UNUSED(clo))
{
	UM_DEBUG(MOD_PRE ": reloading ...done\n");
	return;
}

void
deinit(void *clo)
{
	ud_ctx_t ctx = clo;

	UM_DEBUG(MOD_PRE ": unloading ...");
	deinit_watchers(ctx->mainloop);
	oqsock = -1;
	if (q != NULL) {
		free_oq(q);
	}
	UM_DBGCONT("done\n");
	return;
}

/* dso-oq.c ends here */
