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
/* maximum number of http clients */
#define MAX_CLIENTS	(8)

/* some forwards and globals */
static int handle_data(int fd, char *msg, size_t msglen);
static void handle_close(int fd);
static umoq_t q = NULL;
static int htpush[MAX_CLIENTS];


/* our connectivity cruft */
#include "dso-oq-con6ity.c"


/* htpush connexions */
static void
memorise_htpush(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (htpush[slot] <= 0) {
			break;
		}
	}
	if (slot == MAX_CLIENTS) {
		/* no more room */
		return;
	}
	htpush[slot] = fd;
	return;
}

static void
forget_htpush(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (htpush[slot] == fd) {
			htpush[slot] = 0;
			return;
		}
	}
	return;
}

/* order queue */
static void
prhttphdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 200 OK\r\n\
Date: Tue, 24 Aug 2010 21:51:08 GMT\r\n\
Server: unsermarkt/0.1\r\n\ 
Transfer-Encoding: chunked\r\n\
Content-Type: multipart/x-mixed-replace;boundary=\"umbdry\"\r\n\r\n";
	write(fd, httphdr, sizeof(httphdr) - 1);
	return;
}

static char mbuf[4096], *mptr;

static void
append(const char *ptr, size_t len)
{
	memcpy(mptr, ptr, len);
	mptr += len - 1;
}

static void
prstcb(char side, uml_t l)
{
	char pri[32];

	ffff_m30_s(pri, l->p);
	mptr += sprintf(mptr, "%c(%s, %u);", side, pri, l->q);
	return;
}

static void
prstbcb(uml_t l, void *UNUSED(clo))
{
	prstcb('b', l);
	return;
}

static void
prstacb(uml_t l, void *UNUSED(clo))
{
	prstcb('a', l);
	return;
}

static void
prscript_beg(void)
{
	static const char tag[] = "<script>";
	append(tag, sizeof(tag));
	return;
}

static void
prscript_end(void)
{
	static const char tag[] = "</script>\n";
	append(tag, sizeof(tag));
	return;
}

static void
prclear(void)
{
	static const char clr[] = "clr();";
	append(clr, sizeof(clr));
	return;
}

static void
prpublish(void)
{
	static const char pub[] = "pub();";
	append(pub, sizeof(pub));
	return;
}

static void
prbdry(void)
{
	static const char bdry[] = "--umbdry\r\n";
	append(bdry, sizeof(bdry));
	return;
}

static void
prcty(void)
{
	static const char cty[] = "Content-Type: application/xml\n\n\
<?xml version='1.0'?>\n";
	append(cty, sizeof(cty));
	return;
}

static void
prfinale(void)
{
	*mptr++ = '\r';
	*mptr++ = '\n';
	return;
}

static void
prep_status(void)
{
	/* reset the buffer */
	mptr = mbuf;
	/* write an initial tag and clear the hash table */
	prbdry();
	prcty();
	prscript_beg();
	prclear();
	/* go through all bids, then all asks */
	oq_trav_bids(q, prstbcb, NULL);
	oq_trav_asks(q, prstacb, NULL);
	prpublish();
	prscript_end();
	prbdry();
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	char len[16];
	size_t lenlen;
	size_t chlen = mptr - mbuf;

	/* compute the chunk length */
	lenlen = snprintf(len, sizeof(len), "%zx\r\n", chlen);
	write(fd, len, lenlen);

	/* put the final \r\n */
	prfinale();
	write(fd, mbuf, mptr - mbuf);
	return;
}

static void
upstatus(void)
{
/* for all fds in the htpush queue print the status */
	prep_status();
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (htpush[i] > 0) {
			prstatus(htpush[i]);
		}
	}
	return;
}

/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
static int
handle_data(int fd, char *msg, size_t msglen)
{
#define GET_COOKIE	"GET /"
#define HEAD_COOKIE	"HEAD /"

	if (strncmp(msg, GET_COOKIE, sizeof(GET_COOKIE) - 1) == 0) {
		UM_DEBUG(MOD_PRE ": http push request\n");
		/* keep the connection open so we can push stuff */
		memorise_htpush(fd);
		/* obviously a browser managed to connect to us,
		 * print an initial status */
		prhttphdr(fd);
		prep_status();
		prstatus(fd);
		return 0;
	} else if (strncmp(msg, HEAD_COOKIE, sizeof(HEAD_COOKIE) - 1) == 0) {
		/* obviously a browser managed to connect to us,
		 * print the current order queue and fuck off */
		prhttphdr(fd);
		return -1;
	}

	/* else try and get the order */
	if (msglen == sizeof(struct umo_s)) {
		oq_add_order(q, (umo_t)msg);
		upstatus();
	}
	return 0;
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
