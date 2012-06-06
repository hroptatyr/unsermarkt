/*** netdania.c -- leech some netdania resources
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <stdarg.h>
#include <errno.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

#include <uterus.h>
/* to get a take on them m30s and m62s */
#define DEFINE_GORY_STUFF
#include <m30.h>
#include <m62.h>

#include "boobs.h"
#include "netdania.h"

static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("netdania: ", stderr);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static int
init_sockaddr(ud_sockaddr_t sa, const char *name, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(name, NULL, &hints, &res)) {
		return -1;
	}
	memcpy(sa, res->ai_addr, res->ai_addrlen);
	sa->sa6.sin6_port = htons(port);
	freeaddrinfo(res);
	return 0;
}


static char iobuf[4096];
static const char **gsyms;
static size_t ngsyms = 0;

static int
init_nd(void)
{
	static const char host[] = "balancer.netdania.com";
	union ud_sockaddr_u sa[1];
	int res;

	if (init_sockaddr(sa, host, 80) < 0) {
		error("Error resolving host %s", host);
		return -1;
	} else if ((res = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		error("Error getting socket");
		return -1;
	} else if (connect(res, &sa->sa, sizeof(*sa)) < 0) {
		error("Error connecting to sock %d", res);
		close(res);
		return -1;
	}
	return res;
}

static int
subs_nd(int s, const char **syms, size_t nsyms)
{
	static const char pre[] = "\
GET /StreamingServer/StreamingServer?xstream&group=www.netdania.com&user=.&pass=.&appid=quotelist_awt&xcmd&type=1&reqid=";
	static const char post[] = "\
User-Agent: Streaming Agent\r\n\
Host: balancer.netdania.com\r\n\
\r\n\
";
	static const char trick[] = "1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16;\
17;18;19;20;21;22;23;24;25;26;27;28;29;30;31;32;33;34;35;36;37;38;39;40;\
41;42;43;44;45;46;47;48;49;50;51;52;53;54;55;56;57;58;59;60;61;62;63;64;";
	static const char sym[] = "&sym=";
	char *p = iobuf;
	unsigned int len;

	if (nsyms == 0) {
		return -1;
	} else if (nsyms > 64) {
		/* for now */
		nsyms = 64;
	}

	/* for later */
	gsyms = syms;
	ngsyms = nsyms;

	memcpy(p, pre, len = sizeof(pre) - 1);
	p += len;

	len = 2 * nsyms - 1 + (nsyms < 10 ? 0 : nsyms - 9);
	memcpy(p, trick, len);
	p += len;

	memcpy(p, sym, len = sizeof(sym) - 1);
	p += len;

	for (size_t i = 0; i < nsyms; i++) {
		memcpy(p, syms[i], len = strlen(syms[i]));
		p += len;

		*p++ = ';';
	}
	p[-1] = '\r';
	*p++ = '\n';

	memcpy(p, post, len = sizeof(post));
	p += len - 1/*\nul*/;

	/* assume the socket is safe to send to */
	send(s, iobuf, p - iobuf, 0);
	return 0;
}

static void
__attribute__((unused))
dump_job_raw(job_t j)
{
	int was_print = 0;

	for (unsigned int i = 0; i < j->blen; i++) {
		if (isprint(j->buf[i])) {
			if (!was_print) {
				fputc('"', stdout);
			}
			fputc(j->buf[i], stdout);
			was_print = 1;
		} else {
			if (was_print) {
				fputc('"', stdout);
				fputc(' ', stdout);
			}
			fprintf(stdout, "%02x ", j->buf[i]);
			was_print = 0;
		}
	}
	if (was_print) {
		fputc('"', stdout);
	}
	fputc('\n', stdout);
	return;
}

static uint32_t
read_u32(char **p)
{
	uint32_t *q = (void*)*p;
	uint32_t res = be32toh(*q++);
	*p = (void*)q;
	return res;
}

static uint16_t
read_u16(char **p)
{
	uint16_t *q = (void*)*p;
	uint16_t res = be16toh(*q++);
	*p = (void*)q;
	return res;
}

static uint8_t
read_u8(char **p)
{
	uint8_t *q = (void*)*p;
	uint8_t res = *q++;
	*p = (void*)q;
	return res;
}

static void
fput_sub(uint16_t sub, char **p, FILE *out)
{
	size_t len;
	const char *str = NULL;

	if (*(*p)++ == 0) {
		len = read_u8(p);
		str = *p;
		*p += len;
	}

	/* values come from:
	 * http://www.netdania.com/Products/\
	 * live-streaming-currency-exchange-rates/\
	 * real-time-forex-charts/FinanceChart.aspx */
	switch ((nd_sub_t)sub) {
	case ND_SUB_BID:
		fputc('b', out);
		break;
	case ND_SUB_ASK:
		fputc('a', out);
		break;
	case ND_SUB_BSZ:
		fputc('B', out);
		break;
	case ND_SUB_ASZ:
		fputc('A', out);
		break;
	case ND_SUB_TRA:
		fputc('t', out);
		break;
	case ND_SUB_TIME:
		fputc('@', out);
		break;

	case ND_SUB_CLOSE:
		fputc('c', out);
		break;
	case ND_SUB_HIGH:
		fputc('h', out);
		break;
	case ND_SUB_LOW:
		fputc('l', out);
		break;
	case ND_SUB_OPEN:
		fputc('o', out);
		break;
	case ND_SUB_VOL:
		fputc('v', out);
		break;
	case ND_SUB_LAST:
		fputc('x', out);
		break;
	case ND_SUB_LSZ:
		fputc('X', out);
		break;
	case ND_SUB_OI:
		fputs("oi:", out);
		break;
	case ND_SUB_STL:
		fputs("stl:", out);
		break;
	case ND_SUB_AGENT:
		fputs("agent:", out);
		break;
	case ND_SUB_NAME:
		fputs("name:", out);
		fputc('"', out);
		fwrite(str, sizeof(char), len, out);
		fputc('"', out);
		str = NULL;
		break;
	case ND_SUB_ISIN:
		fputs("isin:", out);
		break;

	case ND_SUB_NANO:
		fputs("nano:", out);
		break;
	case ND_SUB_MSTIME:
		fputc('@', out);
		fwrite(str, sizeof(char), len - 3, out);
		fputc('.', out);
		fwrite(str + len - 3, sizeof(char), 3, out);
		str = NULL;
		break;
	case ND_SUB_ONBID:
		fputs("ONb:", out);
		break;
	case ND_SUB_ONASK:
		fputs("ONa:", out);
		break;
	case ND_SUB_SNBID:
		fputs("SNb:", out);
		break;
	case ND_SUB_SNASK:
		fputs("SNa:", out);
		break;
	case ND_SUB_TNBID:
		fputs("TNb:", out);
		break;
	case ND_SUB_TNASK:
		fputs("TNa:", out);
		break;

	case ND_SUB_CHG_ABS:
		fputs("chg$:", out);
		break;
	case ND_SUB_CHG_PCT:
		fputs("chg%:", out);
		break;
	case ND_SUB_YCHG_PCT:
		fputs("Ychg%:", out);
		break;
	case ND_SUB_52W_HIGH:
		fputs("52wh:", out);
		break;
	case ND_SUB_52W_LOW:
		fputs("52wl:", out);
		break;
	default:
		fprintf(out, "%04x?", sub);
		break;
	}
	if (str) {
		fwrite(str, sizeof(char), len, out);
	}
	return;
}

static void
dump_job(job_t j)
{
	enum {
		TF_UNK = 0x00,
		TF_MSG = 0x01,
		TF_NAUGHT = 0x0c,
	};
	char *p = j->buf, *ep = p + j->blen;

	while (p < ep) {
		switch (*p++) {
		case TF_UNK: {
			uint8_t len = read_u8(&p);

			fputs("GENERIC\t", stdout);
			fwrite(p, sizeof(char), len, stdout);
			fputc('\n', stdout);
			p += len;
			break;
		}
		case TF_MSG: {
			/* next up the identifier */
			uint32_t rid = read_u32(&p);
			/* number of records */
			uint8_t nrec = read_u8(&p);

			if (rid-- > ngsyms) {
				/* we're fucked */
				break;
			}

			/* record count */
			fputs(gsyms[rid], stdout);
			fputc('\t', stdout);
			for (uint8_t i = 0; i < nrec; i++) {
				uint16_t sub = read_u16(&p);

				fput_sub(sub, &p, stdout);
				fputc(' ', stdout);
			}
			fputc('\n', stdout);
			break;
		}
		case TF_NAUGHT:
			break;
		default:
			/* hard to decide what to do */
			break;
		}
	}
	return;
}

/* the actual worker function */
static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nread;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	j->sock = w->fd;
	nread = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nread < 0)) {
		goto out_revok;
	} else if (nread == 0) {
		/* no need to bother */
		goto out_revok;
	}

	j->blen = nread;
	dump_job(j);

out_revok:
	return;
}

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "netdania-clo.h"
#include "netdania-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* args */
	struct nd_args_info argi[1];
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_io beef[1];
	/* unserding resources */
	ud_chan_t hdl;
	int nd_sock;

	/* parse the command line */
	if (nd_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach to the beef channel */
	hdl = ud_chan_init(argi->beef_given ? argi->beef_arg : 8584/*UT*/);

	/* connect to netdania balancer */
	nd_sock = init_nd();
	ev_io_init(beef, mon_beef_cb, nd_sock, EV_READ);
	ev_io_start(EV_A_ beef);

	/* quickly perform the subscription */
	subs_nd(nd_sock, (const char**)argi->inputs, argi->inputs_num);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	ev_io_stop(EV_A_ beef);
	close(nd_sock);

	/* detach ud resources */
	ud_chan_fini(hdl);

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	nd_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* netdania.c ends here */
