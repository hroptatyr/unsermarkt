/*** dso-uschi.c -- settlement and clearing house
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
/* settlement and clearing */
#include "uschi.h"
/* connexion tracking */
#include "um-conn.h"
/* websockets */
#include "htws.h"

#define MOD_PRE		"mod/uschi"
#define UM_PORT		(12769)

/* some forwards and globals */
static int handle_data(int fd, char *msg, size_t msglen);
static void handle_close(int fd);
/* push new status to everyone */
static void prhttphdr(int fd);

static uschi_t h = NULL;


/* our connectivity cruft */
#include "dso-oq-con6ity.c"


/* our websocket support */
#include "htws.c"

static int status_updated = 0;

static void
prxmlhdr(void)
{
	static const char hdr[] = "<?xml version=\"1.0\"?>\n";
	append(hdr, sizeof(hdr) - 1);
	return;
}

static void
pr_otag(void)
{
	static const char tag[] = "<uschi>";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
pr_ctag(void)
{
	static const char tag[] = "</uschi>\n";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
prep_htws_status(void)
{
	/* htws mode */
	reset();
	*mptr++ = 0x00;
	prxmlhdr();
	pr_otag();
	/* go through all bids, then all asks */
	//oq_trav_bids(q, prstbcb, NULL);
	//oq_trav_asks(q, prstacb, NULL);
	pr_ctag();
	*mptr++ = 0xff;
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	/* check if status needs updating */
	if (!status_updated) {
		prep_htws_status();
		status_updated = 1;
	}
	write(fd, mbuf, mptr - mbuf);
	return;
}


/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
static int
handle_data(int fd, char *msg, size_t msglen)
{
	if (htws_get_p(msg, msglen)) {
		UM_DEBUG(MOD_PRE ": htws push request\n");
		status_updated = 0;
		if (htws_handle_get(fd, msg, msglen) < 0) {
			return -1;
		}
		prstatus(fd);
		return 0;

	} else if (htws_clo_p(msg, msglen)) {
		/* websocket closing challenge */
		UM_DEBUG(MOD_PRE ": htws closing request\n");
		return htws_handle_clo(fd, msg, msglen);

#if 0
	} else if (msglen % sizeof(struct umo_s) == 0) {
		/* try and get a bunch of orders */
		for (int i = 0; i < msglen / sizeof(struct umo_s); i++) {
			oq_add_order(q, (umo_t)msg + i);
		}
		upstatus();
		return 0;
#endif
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
		const char *dbpath = NULL;
		udcfg_tbl_lookup_s(&dbpath, ctx, settings, "dbpath");
		h = make_uschi(dbpath);
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
	if (h != NULL) {
		free_uschi(h);
	}
	UM_DBGCONT("done\n");
	return;
}

/* dso-oq.c ends here */
