/*** dso-oq.c -- order queuing
 *
 * Copyright (C) 2010 Sebastian Freundt
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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include "module.h"
#include "unserding.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-nifty.h"
#include "unserding-ctx.h"
#include "ud-sock.h"

#define MOD_PRE		"mod/oq"

/* some forwards */
static int handle_data(int fd, char *msg, size_t msglen);


/* include the order ctors and dtors */
#include "dso-oq-order.h"


/* our connectivity cruft */
#include "dso-oq-con6ity.c"


/* order queue */
static void
prhttphdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 200 OK\r\n\
Content-Type: text/html; charset=ASCII\r\n\
\r\n";
	write(fd, httphdr, sizeof(httphdr));
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	static const char nooq[] = "no orders yet\n";

	prhttphdr(fd);
	write(fd, nooq, sizeof(nooq));
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
		/* obviously a browser managed to connect to us,
		 * print the current order queue and fuck off */
		prstatus(fd);
		return -1;
	} else if (strncmp(msg, HEAD_COOKIE, sizeof(HEAD_COOKIE) - 1) == 0) {
		/* obviously a browser managed to connect to us,
		 * print the current order queue and fuck off */
		prhttphdr(fd);
		return -1;
	}

	/* else just print the buffer */
	write(STDERR_FILENO, msg, msglen);
	return 0;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG(MOD_PRE ": loading ...");
	/* connect to scscp and say ehlo */
	oqsock = listener();
	/* set up the IO watcher and timer */
	init_watchers(ctx->mainloop, oqsock);
	UD_DBGCONT("loaded\n");
	return;
}

void
reinit(void *UNUSED(clo))
{
	UD_DEBUG(MOD_PRE ": reloading ...done\n");
	return;
}

void
deinit(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG(MOD_PRE ": unloading ...");
	deinit_watchers(ctx->mainloop, oqsock);
	oqsock = -1;
	UD_DBGCONT("done\n");
	return;
}

/* dso-oq.c ends here */
