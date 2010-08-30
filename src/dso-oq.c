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
/* for our websockets */
#include "htws.h"

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

static umoq_t q = NULL;


/* our connectivity cruft */
#include "dso-oq-con6ity.c"


/* our websocket support */
#include "htws.c"

static int status_updated = 0;

static void
prstcb(char side, uml_t l)
{
	char pri[32];

	ffff_m30_s(pri, l->p);
	mptr += sprintf(mptr, "<%c p=\"%s\" q=\"%u\"/>", side, pri, l->q);
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
pr_otag(void)
{
	static const char tag[] = "<quotes>";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
pr_ctag(void)
{
	static const char tag[] = "</quotes>\n";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
prbdry(void)
{
	static const char bdry[] = "--umbdry\r\n";
	append(bdry, sizeof(bdry) - 1);
	return;
}

static void
prcty(void)
{
	static const char cty[] = "Content-Type: application/xml\n\n\
<?xml version='1.0'?>\n";
	append(cty, sizeof(cty) - 1);
	return;
}

static void
prxmlhdr(void)
{
	static const char hdr[] = "<?xml version=\"1.0\"?>\n";
	append(hdr, sizeof(hdr) - 1);
	return;
}

static void __attribute__((unused))
prep_http_status(void)
{
	/* http mode */
	reset();
	prbdry();
	prcty();
	pr_otag();
	/* go through all bids, then all asks */
	oq_trav_bids(q, prstbcb, NULL);
	oq_trav_asks(q, prstacb, NULL);
	pr_ctag();
	prbdry();
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
	oq_trav_bids(q, prstbcb, NULL);
	oq_trav_asks(q, prstacb, NULL);
	pr_ctag();
	*mptr++ = 0xff;
	return;
}

#if 0
static void
prhtwshdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 101 WebSocket Protocol Handshake\r\n\
Date: Tue, 24 Aug 2010 21:51:08 GMT\r\n\
Server: unsermarkt/0.1\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Origin: http://www.unserding.org:12768\r\n\
Sec-WebSocket-Location: ws://www.unserding.org:12768\r\n\
Transfer-Encoding: chunked\r\n\
Connection: Keep-Alive\r\n\
Content-Type: multipart/x-mixed-replace;boundary=\"umbdry\"\r\n\r\n";
	write(fd, httphdr, sizeof(httphdr) - 1);
	return;
}
#endif

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	/* check if status needs updating */
	if (!status_updated) {
		prep_htws_status();
		status_updated = 1;
	}

#if 0
/* only in http mode */
	{
		char len[16];
		size_t lenlen;
		size_t chlen = mptr - mbuf;

		/* compute the chunk length */
		lenlen = snprintf(len, sizeof(len), "%zx\r\n", chlen);
		write(fd, len, lenlen);
		/* put the final \r\n */
		*mptr++ = '\r';
		*mptr++ = '\n';
	}
#else
/* htws mode */
	;
#endif
	write(fd, mbuf, mptr - mbuf);
	return;
}

static void
upstatus(void)
{
/* for all fds in the htpush queue print the status */
	/* flag status as outdated */
	status_updated = 0;
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
	if (htws_get_p(msg, msglen)) {
		UM_DEBUG(MOD_PRE ": htws push request\n");
		/* first of all render the status bit void
		 * coz the htws handler uses the same buffer */
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

	} else if (msglen % sizeof(struct umo_s) == 0) {
		/* try and get a bunch of orders */
		for (int i = 0; i < msglen / sizeof(struct umo_s); i++) {
			oq_add_order(q, (umo_t)msg + i);
		}
		upstatus();
		return 0;

	} else if (strncasecmp(msg, "LISTEN", 6) == 0) {
		memorise_htpush(fd);
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
