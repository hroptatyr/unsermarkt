/*** dso-unsermarkt.c -- settlement and clearing house plus order queue
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
/* order queue */
#include "oq.h"
/* settlement and clearing */
#include "uschi.h"
/* for connexion tracking */
#include "um-conn.h"
/* websockets */
#include "htws.h"

#define MOD_PRE		"mod/unsermarkt"
#define UM_PORT		(8788)

/* some forwards and globals */
static int handle_data(int fd, char *msg, size_t msglen);
static void handle_close(int fd);
/* push new status to everyone */
static void prhttphdr(int fd);

static uschi_t h = NULL;
static umoq_t q = NULL;


/* our connectivity cruft */
#include "dso-oq-con6ity.c"

static void
memorise_agent(int fd, agtid_t a)
{
	um_conn_t slot = um_conn_memorise(fd, a);
	slot->agent = a;
	slot->flags = 1;
	return;
}

static agtid_t
find_agent_by_fd(int fd)
{
	um_conn_t slot = um_conn_find_by_fd(fd);
	return slot ? slot->agent : 0;
}

#define forget_agent	um_conn_forget


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

static void
upstatus(void)
{
/* for all fds in the htpush queue print the status */
	/* flag status as outdated */
	status_updated = 0;
	FOR_EACH_CONN(c) {
		prstatus(c->fd);
	}
	return;
}


/* simple protocol:
 * Commands:
 * EHLO <agent_name> register as agent
 * LISTEN register to get quote pushes
 * ORDER <instr> B|S <qty> [<pri>|MTL]
 * KTHX close connection
 * CANCEL <order_id> cancel order by oid
 * QUOTES <instr> get the quotes for INSTR
 **/
static int
EHLO_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "EHLO ", 5) == 0;
}

static int
handle_EHLO(int fd, char *msg, size_t msglen)
{
/* we now ask uschi for the agent_id of the agent in question */
	char *agtstr = msg + 5;
	char *eol;
	agtid_t a;

	if ((eol = memchr(msg, '\n', msglen)) == NULL) {
		eol = msg + msglen;
	}
	/* otherwise \n was found */
	*eol = '\0';
	if ((a = uschi_get_agent(h, agtstr)) == 0) {
		static const char err[] = "Ne'er heard of you, go away\n";
		write(fd, err, sizeof(err) - 1);
		return -1;
	}
	/* otherwise make sure we remember him */
	memorise_agent(fd, a);
	/* shamelessly reuse msg buffer */
	msglen = snprintf(msg, 64, "EHLO %u\n", a);
	write(fd, msg, msglen);
	return 0;
}

static int
KTHX_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "KTHX", 4) == 0;
}

static int
handle_KTHX(int fd, char *UNUSED(msg), size_t UNUSED(msglen))
{
	forget_agent(fd);
	return -1;
}

static int
LISTEN_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "LISTEN", 6) == 0;
}

static int
handle_LISTEN(int fd, const char *UNUSED(msg), size_t UNUSED(msglen))
{
	memorise_htpush(fd);
	return 0;
}

static int
QUOTES_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "QUOTES", 6) == 0;
}

static int
handle_QUOTES(int fd, const char *UNUSED(msg), size_t UNUSED(msglen))
{
/* actually the client can specify which instruments to quote,
 * we don't care tho */
	prstatus(fd);
	return 0;
}

static int
ORDER_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "ORDER ", 6) == 0;
}

static int
handle_ORDER(int fd, agtid_t a, char *msg, size_t msglen)
{
	char *cursor;
	struct umo_s o[1] = {0};
	oid_t oid;

	/* start off with the instr id, later we may allow symbols as well */
	cursor = msg + 6;
	o->instr_id = strtoul(cursor, &cursor, 10);

	/* cursor should now be ' ' */
	if (o->instr_id == 0 || *cursor++ != ' ') {
		return -1;
	}

	/* obtain side */
	switch (*cursor++) {
	case 'b':
	case 'B':
		/* buy order */
		o->side = OSIDE_BUY;
		break;
	case 's':
	case 'S':
		/* sell order */
		o->side = OSIDE_SELL;
		break;
	default:
		return -1;
	}

	/* zap to next space */
	for (; *cursor && !(*cursor == ' ' || *cursor == '\t'); cursor++);
	/* zap to next thing beyond that space */
	for (; *cursor && (*cursor == ' ' || *cursor == '\t'); cursor++);

	/* must be quantity */
	o->q = strtoul(cursor, &cursor, 10);

	/* zap to next thing beyond that space */
	for (; *cursor && (*cursor == ' ' || *cursor == '\t'); cursor++);

	if (*cursor == '\0') {
		/* market order */
		o->p.v = 0;
		o->type = OTYPE_MKT;
	} else if (strncasecmp(cursor, "MTL", 3) == 0) {
		/* market to limit order, what a shame */
		o->p.v = 0;
		o->type = OTYPE_MTL;
	} else if ((o->p = ffff_m30_get_s(&cursor)).v != 0) {
		/* limit order, p should point to the price now */
		o->type = OTYPE_LIM;
	} else {
		/* pure bollocks */
		return -1;
	}

	/* looks good so far, propagate the agent id */
	o->agent_id = a;
	/* finally set order modifier, only one we support atm is GTC */
	o->tymod = OTYMOD_GTC;

	/* everything seems in order, just send the fucker off and pray */
	oid = oq_add_order(q, o);
	/* we bluntly reuse the msg buffer */
	msglen = snprintf(msg, msglen, "%u\n", oid);
	/* give a bit of feedback */
	write(fd, msg, msglen);
	return 0;
}

static int
CANCEL_p(const char *msg, size_t UNUSED(msglen))
{
	return strncasecmp(msg, "CANCEL ", 7) == 0;
}

static int
handle_CANCEL(int fd, agtid_t agt, char *msg, size_t UNUSED(msglen))
{
	static const char err[] = "nothing cancelled because you blow\n";
	static const char suc[] = "order cancelled\n";
	struct umo_s o[1];
	oid_t id;

	if ((id = strtoul(msg + 7, NULL, 10)) == 0) {
		goto errout;
	}
	*o = oq_get_order(q, id);
	if (o->agent_id == agt) {
		/* only cancel stuff that belongs to the agent */
		if (UNLIKELY(oq_cancel_order(q, id) < 0)) {
			goto errout;
		}
	}
	write(fd, suc, sizeof(suc) - 1);
	return 0;
errout:
	write(fd, err, sizeof(err) - 1);
	return 0;
}


/**
 * Take the stuff in MSG of size MSGLEN coming from FD and process it.
 * Return values <0 cause the handler caller to close down the socket. */
static int
handle_data(int fd, char *msg, size_t msglen)
{
	agtid_t aid;

	/* certain commands are available to registered agents only,
	 * means we look for the agent id now and pass it along
	 * to the handlers */
	aid = find_agent_by_fd(fd);

	if (!aid && htws_get_p(msg, msglen)) {
		UM_DEBUG(MOD_PRE ": htws push request\n");
		status_updated = 0;
		if (htws_handle_get(fd, msg, msglen) < 0) {
			return -1;
		}
		prstatus(fd);
		return 0;

	} else if (!aid && htws_clo_p(msg, msglen)) {
		/* websocket closing challenge */
		UM_DEBUG(MOD_PRE ": htws closing request\n");
		return htws_handle_clo(fd, msg, msglen);

	} else if (!aid && EHLO_p(msg, msglen)) {
		return handle_EHLO(fd, msg, msglen);

	} else if (KTHX_p(msg, msglen)) {
		/* agent forgetting challenge */
		UM_DEBUG(MOD_PRE ": agent says goodbye\n");
		return handle_KTHX(fd, msg, msglen);

	} else if (aid && QUOTES_p(msg, msglen)) {
		return handle_QUOTES(fd, msg, msglen);

	} else if (!aid && LISTEN_p(msg, msglen)) {
		return handle_LISTEN(fd, msg, msglen);

	} else if (!aid) {
		/* just ignore the bollocks */
		return 0;
	}

	/* now following multi-command messages */
	for (char *eom = msg + msglen, *eol; msg < eom; msg = eol + 1) {
		/* just make sure we know our boundaries */
		if ((eol = memchr(msg, '\n', eom - msg)) == NULL) {
			eol = eom;
		}
		/* easier for them handlers when the string is \0 term'd */
		*eol = '\0';

		/* multi-command message checks */
		if (ORDER_p(msg, eol - msg)) {
			handle_ORDER(fd, aid, msg, eol - msg);

		} else if (CANCEL_p(msg, eol - msg)) {
			handle_CANCEL(fd, aid, msg, eol - msg);

		} else {
			UM_DEBUG(MOD_PRE ": unknown crap on the wire\n");
			return -1;
		}
	}
	/* something must have happened to the order queue */
	upstatus();
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
		const char *dbpath = NULL;
		udcfg_tbl_lookup_s(&dbpath, ctx, settings, "dbpath");
		h = make_uschi(dbpath);
		/* hardcoded securities */
		q = make_oq(2, 1);
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

/* dso-unsermarkt.c ends here */
