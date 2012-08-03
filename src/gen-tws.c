/*** gen-tws.c -- generic tws c api
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
/* for gmtime_r */
#include <time.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#if defined STANDALONE
# include <sys/epoll.h>
#endif	/* STANDALONE */

/* the tws api */
#include "gen-tws.h"
#include "gen-tws-cont.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define GEN_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define GEN_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;


#if defined STANDALONE
#include <argp.h>

const char *argp_program_version = "gen-tws " VERSION;
const char *argp_program_bug_address = NULL;
static char doc[] = "\
Test driver for the gen-tws C wrapper.\n\
";

static struct argp_option options[] = {
	{"help", 'h', 0, 0, "Print this help screen"},
	{"version", 'V', 0, 0, "Print version number"},
	{0, '\0', 0, 0, "TWS options", 1},
	{"host", 'h' + 256, "STR", 0, "TWS host"},
	{"port", 'p' + 256, "NUM", 0, "TWS port"},
	{"client-id", 'c' + 256, "NUM", 0, "TWS client id"},
	{0},
};

struct my_args_s {
	const char *host;
	short unsigned int port;
	int client;
};

/* error() impl */
static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("[quo-tws] ", stderr);
	vfprintf(logerr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ? eno : errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static error_t
popt(int key, char *arg, struct argp_state *state)
{
	struct my_args_s *args = state->input;

	switch (key) {
	case 'h' + 256:
		args->host = arg;
		break;
	case 'p' + 256: {
		char *p;
		long int foo;

		if ((foo = strtol(arg, &p, 0)) && !*p) {
			args->port = (short unsigned int)foo;
		}
		break;
	}
	case 'c' + 256: {
		char *p;
		long int foo;

		if ((foo = strtol(arg, &p, 0)) && !*p) {
			args->client = (int)foo;
		}
		break;
	}

	case 'h':
		argp_state_help(state, state->out_stream, ARGP_HELP_STD_HELP);
		exit(0);
	case 'V':
		puts(argp_program_version);
		exit(0);

	case ARGP_KEY_ARG:
		break;

	case ARGP_KEY_END:
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static void
infra_cb(tws_t tws, tws_cb_t what, struct tws_infra_clo_s clo)
{
	switch (what) {
	case TWS_CB_INFRA_ERROR:
		error(0, "tws %p: oid %u  code %u: %s",
			tws, clo.oid, clo.code, (const char*)clo.data);
		break;
	case TWS_CB_INFRA_CONN_CLOSED:
		error(0, "tws %p: connection closed", tws);
		break;
	default:
		error(0, "%p infra called: what %u  oid %u  code %u  data %p",
			tws, what, clo.oid, clo.code, clo.data);
		break;
	}
	return;
}

#if defined HAVE_TWSAPI_HANDSHAKE
static int
rslv(struct addrinfo **res, const char *host, short unsigned int port)
{
	char strport[32];
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	/* Convert the port number into a string. */
	snprintf(strport, sizeof(strport), "%hu", port);

	return getaddrinfo(host, strport, &hints, res);
}

static int
conn(struct addrinfo *ais)
{
	int s = -1;

	for (struct addrinfo *aip = ais; aip; aip = aip->ai_next) {
		if ((s = socket(aip->ai_family, aip->ai_socktype, 0)) < 0) {
			/* great way to start the day */
			;
		} else if (connect(s, aip->ai_addr, aip->ai_addrlen) < 0) {
			/* bugger */
			close(s);
		} else {
			/* yippie */
			break;
		}
	}
	freeaddrinfo(ais);
	return s;
}
#endif	/* HAVE_TWSAPI_HANDSHAKE */

#if defined HAVE_EXPAT_H
static const char xmpl_cont[] = "\
<TWSXML xmlns="">\n\
  <request type=\"market_data\">\n\
    <query>\n\
      <reqContract symbol=\"EUR\" currency=\"USD\" secType=\"CASH\"\n\
        exchange=\"IDEALPRO\"/>\n\
    </query>\n\
  </request>\n\
</TWSXML>\n\
";
#endif	/* HAVE_EXPAT_H */

int
main(int argc, char *argv[])
{
	struct my_args_s args = {
		.host = "quant",
		.port = 7474,
		.client = 3333,
	};
	struct argp aprs = {options, popt, NULL, doc};
	struct tws_s tws[1] = {{0}};
	struct epoll_event ev[1];
#if defined HAVE_TWSAPI_HANDSHAKE
	struct addrinfo *ais = NULL;
#endif	/* HAVE_TWSAPI_HANDSHAKE */
	int epfd;
	int s;
	int res = 0;

	logerr = stderr;
	argp_parse(&aprs, argc, argv, ARGP_NO_HELP, 0, &args);

#if defined HAVE_TWSAPI_HANDSHAKE
	/* we check if we can connect to the host/port in question
	 * BEFORE populating (and hence ctor'ing) the tws object */
	if (rslv(&ais, args.host, args.port) < 0) {
		error(0, "cannot resolve [%s]:%hu", args.host, args.port);
		res = 1;
		goto fini;
	} else if ((s = conn(ais)) < 0) {
		error(0, "cannot connect to [%s]:%hu", args.host, args.port);
		res = 1;
		goto fini;
	}

	if (init_tws(tws, s, args.client) < 0) {
		return 1;
	}
#else  /* !HAVE_TWSAPI_HANDSHAKE */
	/* in the traditional connection procedure the tws object must
	 * be initialised first as the underlying worker functions are
	 * part of the EPosixClientSocket blackbox and need ctor'ing */
	if (init_tws(tws, 0, 0) < 0) {
		return 1;
	}

	if ((s = tws_connect(tws, args.host, args.port, args.client)) < 0) {
		error(0, "cannot connect to [%s]:%hu", args.host, args.port);
		res = 1;
		goto fini;
	}
#endif	/* HAVE_TWSAPI_HANDSHAKE */

	if ((epfd = epoll_create(1)) < 0) {
		res = 1;
		goto disc;
	}
	/* add s to epoll descriptor */
	ev->events = EPOLLIN | EPOLLHUP;
	epoll_ctl(epfd, EPOLL_CTL_ADD, s, ev);

	/* add some callbacks */
	tws->infra_cb = infra_cb;

#if defined HAVE_TWSAPI_HANDSHAKE
	/* handshake first */
	do {
		switch (tws_start(tws)) {
		case -1:
			/* fuck */
			error(0, "handshake with [%s]:%hu/%i failed",
			      args.host, args.port, args.client);
			res = 1;
			goto clos;
		case 1:
			/* handshake's finished yay */
			goto main_loop;
		case 0:
			break;
		}
	} while (epoll_wait(epfd, ev, 1, 2000) > 0);

main_loop:
#endif	/* HAVE_TWSAPI_HANDSHAKE */
	while (epoll_wait(epfd, ev, 1, 2000) > 0) {
		if (ev->events & EPOLLHUP) {
			break;
		}
		if (ev->events & EPOLLIN) {
			tws_recv(tws);
		}
		if (1) {
			tws_send(tws);
		}
	}

#if defined HAVE_EXPAT_H
/* test contract builder */
	{
		tws_cont_t x = tws_cont(xmpl_cont, sizeof(xmpl_cont));
		fprintf(logerr, "built contract %p\n", x);
		tws_free_cont(x);
	}
#endif	/* HAVE_EXPAT_H */

disc:
#if defined HAVE_TWSAPI_HANDSHAKE
	if (tws_stop(tws) < 0) {
		res = 1;
		goto clos;
	}
#else  /* HAVE_TWSAPI_HANDSHAKE */
	if (tws_disconnect(tws) < 0) {
		res = 1;
		goto fini;
	}
#endif	/* HAVE_TWSAPI_HANDSHAKE */

#if defined HAVE_TWSAPI_HANDSHAKE
/* in this case we need to close our socket ourselves of course */
clos:
	close(s);
#endif	/* HAVE_TWSAPI_HANDSHAKE */

fini:
	if (fini_tws(tws) < 0) {
		res = 1;
	}
	return res;
}
#endif	/* STANDALONE */

/* gen-tws.c ends here */
