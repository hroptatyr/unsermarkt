/*** um-netdania.c -- leech some netdania resources
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

#include <sys/time.h>
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

/* to get a take on them m30s and m62s */
#define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
# include <uterus/m62.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
# include <m62.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

#include "svc-uterus.h"
#include "boobs.h"
#include "nifty.h"
#include "um-netdania.h"

#if defined DEBUG_FLAG
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */

/* tolerate this many seconds without quotes */
#define MAX_AGE			(60.0)

typedef const struct nd_pkt_s *nd_pkt_t;

struct nd_pkt_s {
	struct sockaddr ss;
	socklen_t sz;
	size_t bsz;
	char *buf;
};


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("um-netdania: ", stderr);
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
init_sockaddr(
	struct sockaddr_storage *sa, socklen_t *sz,
	const char *name, uint16_t port)
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
	*sz = res->ai_addrlen;
	((struct sockaddr_in6*)sa)->sin6_port = htons(port);
	freeaddrinfo(res);
	return 0;
}


static char iobuf[4096];
static char **gsyms;
static size_t ngsyms = 0;
static struct timeval last_brag[1] = {0, 0};

#define BRAG_INTV	(10)

static int
init_nd(void)
{
	static const char host[] = "balancer.netdania.com";
	struct sockaddr_storage sa[1];
	socklen_t sz[1];
	int res;

	if (init_sockaddr(sa, sz, host, 80) < 0) {
		error("Error resolving host %s", host);
		return -1;
	} else if ((res = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		error("Error getting socket");
		return -1;
	} else if (connect(res, (void*)sa, sz[0]) < 0) {
		error("Error connecting to sock %d", res);
		close(res);
		return -1;
	}
	return res;
}

static int
subs_nd(int s, char **syms, size_t nsyms)
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
41;42;43;44;45;46;47;48;49;50;51;52;53;54;55;56;57;58;59;60;61;62;63;64;\
65;66;67;68;69;70;71;72;73;74;75;76;77;78;79;80;81;82;83;84;85;86;87;88;\
89;90;91;92;93;94;95;96;97;98;99;";
#define MAX_NSYMS	(99)
	static const char sym[] = "&sym=";
	char *p = iobuf;
	unsigned int len;

	if (nsyms == 0) {
		return -1;
	} else if (nsyms > MAX_NSYMS) {
		/* for now */
		nsyms = MAX_NSYMS;
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
dump_job_raw(nd_pkt_t j)
{
	int was_print = 0;

	for (unsigned int i = 0; i < j->bsz; i++) {
		uint8_t c = j->buf[i];

		if (isprint(c)) {
			if (!was_print) {
				fputc('"', stdout);
			}
			fputc(c, stdout);
			was_print = 1;
		} else {
			if (was_print) {
				fputc('"', stdout);
				fputc(' ', stdout);
			}
			fprintf(stdout, "%02x ", c);
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

static int
fput_sub(uint16_t sub, char **p, const char *ep, FILE *out)
{
	size_t len;
	const char *str = NULL;

	if (*(*p)++ == 0) {
		len = read_u8(p);
		str = *p;
		if ((*p += len) > ep) {
			/* string is incomplete */
			UM_DEBUG("string incomplete\n");
			return -1;
		}
	} else {
		UM_DEBUG("not a string type: %x\n", (*p)[-1]);
		return -1;
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
		fputc('V', out);
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
	case ND_SUB_1WBID:
		fputs("1Wb:", out);
		break;
	case ND_SUB_1WASK:
		fputs("1Wa:", out);
		break;
	case ND_SUB_2WBID:
		fputs("2Wb:", out);
		break;
	case ND_SUB_2WASK:
		fputs("2Wa:", out);
		break;
	case ND_SUB_3WBID:
		fputs("3Wb:", out);
		break;
	case ND_SUB_3WASK:
		fputs("3Wa:", out);
		break;
	case ND_SUB_1MBID:
		fputs("1Mb:", out);
		break;
	case ND_SUB_1MASK:
		fputs("1Ma:", out);
		break;
	case ND_SUB_2MBID:
		fputs("2Mb:", out);
		break;
	case ND_SUB_2MASK:
		fputs("2Ma:", out);
		break;
	case ND_SUB_3MBID:
		fputs("3Mb:", out);
		break;
	case ND_SUB_3MASK:
		fputs("3Ma:", out);
		break;
	case ND_SUB_4MBID:
		fputs("4Mb:", out);
		break;
	case ND_SUB_4MASK:
		fputs("4Ma:", out);
		break;
	case ND_SUB_5MBID:
		fputs("5Mb:", out);
		break;
	case ND_SUB_5MASK:
		fputs("5Ma:", out);
		break;
	case ND_SUB_6MBID:
		fputs("6Mb:", out);
		break;
	case ND_SUB_6MASK:
		fputs("6Ma:", out);
		break;
	case ND_SUB_7MBID:
		fputs("7Mb:", out);
		break;
	case ND_SUB_7MASK:
		fputs("7Ma:", out);
		break;
	case ND_SUB_8MBID:
		fputs("8Mb:", out);
		break;
	case ND_SUB_8MASK:
		fputs("8Ma:", out);
		break;
	case ND_SUB_9MBID:
		fputs("9Mb:", out);
		break;
	case ND_SUB_9MASK:
		fputs("9Ma:", out);
		break;
	case ND_SUB_10MBID:
		fputs("10Mb:", out);
		break;
	case ND_SUB_10MASK:
		fputs("10Ma:", out);
		break;
	case ND_SUB_11MBID:
		fputs("11Mb:", out);
		break;
	case ND_SUB_11MASK:
		fputs("11Ma:", out);
		break;
	case ND_SUB_1YBID:
		fputs("12Mb:", out);
		break;
	case ND_SUB_1YASK:
		fputs("12Ma:", out);
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
	return 0;
}

static int MAYBE_NOINLINE
dump_TF_MSG(char **q, size_t len)
{
	char *p = *q;
	char *ep = p + len;
	uint32_t rid;
	uint8_t nrec;
	uint8_t i;

	if (UNLIKELY(p + 5 >= ep)) {
		/* no need to continue */
		return -1;
	} else if (UNLIKELY((rid = read_u32(&p) - 1) > ngsyms)) {
		/* we're fucked */
		return -1;
	}

	/* print the symbol so we know what this was supposed to be */
	fputs(gsyms[rid], stdout);
	fputc('\t', stdout);

	/* and the number of records */
	nrec = read_u8(&p);
	for (i = 0; i < nrec && p <= ep - 4; i++) {
		uint16_t sub = read_u16(&p);

		if (fput_sub(sub, &p, ep, stdout) < 0) {
			break;
		}
		fputc(' ', stdout);
	}
	fputc('\n', stdout);
	if (i == nrec) {
		*q = p;
		return 0;
	} else {
		/* we could not decode all records, leave q untouched */
		return -1;
	}
}

static size_t MAYBE_NOINLINE
dump_job(nd_pkt_t j)
{
	enum {
		TF_UNK = 0x00,
		TF_MSG = 0x01,
		TF_NAUGHT = 0x0c,
	};
	char *p = j->buf, *ep = p + j->bsz;

	while (p < ep) {
		switch (*p++) {
		case TF_UNK: {
			uint8_t len = (uint8_t)(p + 1 < ep ? read_u8(&p) : 0);

			if (UNLIKELY(p + len > ep)) {
				len = ep - p;
			}

			fputs("GENERIC\t", stdout);
			fwrite(p, sizeof(char), len, stdout);
			fputc('\n', stdout);
			p += len;
			break;
		}

		case TF_MSG:
			if (dump_TF_MSG(&p, ep - p) < 0) {
				return p - 1 - j->buf;
			}
			break;

		case 0x0a: {
			/* uint32_t? */
			uint32_t v = read_u32(&p);
			fprintf(stdout, "0x0a MSG\t%u\n", v);
			break;
		}
		case TF_NAUGHT:
			break;
		default:
			/* hard to decide what to do */
			break;
		}
	}
	fflush(stdout);
	return j->bsz;
}

static void
brag(ud_sock_t s)
{
	for (size_t i = 0; i < ngsyms; i++) {
		struct um_qmeta_s brg = {
			.idx = (uint32_t)(i + 1),
			.sym = gsyms[i],
			.symlen = strlen(gsyms[i]),
			.uri = NULL,
			.urilen = 0U,
		};

		um_pack_brag(s, &brg);
	}
	ud_flush(s);
	return;
}

static void
inspect_rec(char **p, struct sl1t_s *l1t, size_t nl1t)
{
	/* next up the identifier */
	uint32_t rid = read_u32(p);
	/* number of records */
	uint8_t nrec = read_u8(p);
	/* data to fill in */
	long int sec = 0;
	unsigned int msec = 0;

	for (uint8_t i = 0; i < nrec; i++) {
		uint16_t sub = read_u16(p);
		/* value handling */
		size_t len;
		const char *str = NULL;
		char save;

		if (*(*p)++) {
			continue;
		}
		/* read the string */
		len = read_u8(p);
		str = *p;
		*p += len;
		save = **p, **p = '\0';

		switch ((nd_sub_t)sub) {
			char *q;
		case ND_SUB_TIME:
		case ND_SUB_MSTIME:
			if ((sec = strtol(str, &q, 10), *q)) {
				sec = 0;
				msec = 0;
			} else if ((nd_sub_t)sub == ND_SUB_MSTIME) {
				msec = sec % 1000;
				sec = sec / 1000;
			}
			break;
		case ND_SUB_BID:
		case ND_SUB_ASK:
		case ND_SUB_TRA:
		case ND_SUB_BSZ:
		case ND_SUB_ASZ:
		case ND_SUB_LAST:
		case ND_SUB_LSZ: {
			m30_t v = ffff_m30_get_s(&str);

			switch ((nd_sub_t)sub) {
			case ND_SUB_BID:
				l1t[0].bid = v.u;
				sl1t_set_ttf(l1t + 0, SL1T_TTF_BID);
				break;
			case ND_SUB_ASK:
				l1t[1].ask = v.u;
				sl1t_set_ttf(l1t + 1, SL1T_TTF_ASK);
				break;
			case ND_SUB_TRA:
			case ND_SUB_LAST:
				l1t[2].tra = v.u;
				sl1t_set_ttf(l1t + 2, SL1T_TTF_TRA);
				break;
			case ND_SUB_BSZ:
				l1t[0].bsz = v.u;
				break;
			case ND_SUB_ASZ:
				l1t[1].asz = v.u;
				break;
			case ND_SUB_LSZ:
				l1t[2].tsz = v.u;
				break;
			}
			break;
		}
		case ND_SUB_VOL: {
			m62_t v = ffff_m62_get_s(&str);

			l1t[3].w[0] = v.u;
			sl1t_set_ttf(l1t + 3, SL1T_TTF_VOL);
			break;
		}
		default:
			break;
		}

		/* re-instantiate the saved character */
		**p = save;
	}

	if (sec == 0) {
		/* ticks without time stamp are fucking useless */
		return;
	}
	for (size_t i = 0; i < nl1t; i++) {
		if (l1t[i].v[0]) {
			sl1t_set_stmp_sec(l1t + i, sec);
			sl1t_set_stmp_msec(l1t + i, (uint16_t)msec);
			sl1t_set_tblidx(l1t + i, (uint16_t)rid);
		}
	}
	return;
}

static void
send_job(ud_sock_t s, nd_pkt_t j)
{
	enum {
		TF_UNK = 0x00,
		TF_MSG = 0x01,
		TF_NAUGHT = 0x0c,
	};
	char *p = j->buf, *ep = p + j->bsz;
	/* unserding goodness */
	struct sl1t_s l1t[4];

	while (p < ep) {
		if (*p++ == TF_MSG) {
			memset(l1t, 0, sizeof(l1t));
			inspect_rec(&p, l1t, countof(l1t));

			for (size_t i = 0; i < countof(l1t); i++) {
				if (scom_thdr_tblidx(AS_SCOM(l1t + i))) {
					um_pack_sl1t(s, l1t + i);
				}
			}
		} else {
			break;
		}
	}
	ud_flush(s);
	return;
}

/* helpers for the worker function */
static int rawp = 0;
static size_t nquo = 0;

/* the actual worker function */
static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	/* a job */
	static struct nd_pkt_s j[1] = {{
			.buf = iobuf,
			.bsz = sizeof(iobuf),
		}};
	ssize_t nrd;
	struct timeval now[1];
	ud_sock_t s = w->data;

	nrd = recvfrom(w->fd, j->buf, j->bsz, 0, (void*)&j->ss, (void*)&j->sz);

	/* handle the reading */
	if (UNLIKELY(nrd < 0)) {
		goto out_revok;
	} else if (nrd == 0) {
		/* no need to bother */
		goto out_revok;
	}

	/* check if we need bragging */
	if (gettimeofday(now, NULL) < 0) {
		/* time is fucked */
		;
	} else if (now->tv_sec - last_brag->tv_sec > BRAG_INTV) {
		brag(s);
		/* keep track of last brag date */
		*last_brag = *now;
	}

	/* prepare the job */
	UM_DEBUG("read %zd/%zu\n", nrd, j->bsz);
	nrd += sizeof(iobuf) - j->bsz;
	j->bsz = nrd;
	j->buf = iobuf;
	if (LIKELY(!rawp)) {
		size_t nproc;
		size_t nmove;

		nproc = dump_job(j);
		j->bsz = nproc;
		send_job(s, j);

		if ((nmove = nrd - nproc) > 0) {
			memmove(iobuf, j->buf + nproc, nmove);
		}
		j->buf = iobuf + nmove;
		j->bsz = sizeof(iobuf) - nmove;
	} else {
		/* send fuckall in raw mode, just dump it */
		dump_job_raw(j);
		j->bsz = sizeof(iobuf);
	}

	/* update the quote counter */
	nquo++;
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

static void
keep_alive_cb(EV_P_ ev_timer *w, int UNUSED(revents))
{
	if (nquo) {
		/* everything in order */
		nquo = 0;
		ev_timer_again(EV_A_ w);
		return;
	}
	/* otherwise there's been no quotes  */
	UM_DEBUG("no data for %f seconds, unrolling...\n", MAX_AGE);
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
#include "um-netdania-clo.h"
#include "um-netdania-clo.c"
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
	ev_timer keep_alive[1];
	/* unserding resources */
	ud_sock_t s;
	/* netdania resources */
	int nd_sock;

	/* parse the command line */
	if (nd_parser(argc, argv, argi)) {
		exit(1);
	}
	/* start with the context assignments */
	rawp = argi->raw_given;

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
	{
		struct ud_sockopt_s opt = {UD_PUB};

		if (argi->beef_given) {
			opt.port = (short unsigned int)argi->beef_arg;
		} else {
			opt.port = 7868U/*ND*/;
		}

		if ((s = ud_socket(opt)) == NULL) {
			perror("cannot initialise ud socket");
			goto nopub;
		}
	}

	/* connect to netdania balancer */
	if ((nd_sock = init_nd()) < 0) {
		goto out;
	}

	beef->data = s;
	ev_io_init(beef, mon_beef_cb, nd_sock, EV_READ);
	ev_io_start(EV_A_ beef);

	/* quickly perform the subscription */
	subs_nd(nd_sock, argi->inputs, argi->inputs_num);

	/* set a timer to see if we lack quotes */
	ev_timer_init(keep_alive, keep_alive_cb, MAX_AGE, MAX_AGE);
	ev_timer_start(EV_A_ keep_alive);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	ev_io_stop(EV_A_ beef);
	close(nd_sock);

out:
	/* detach ud resources */
	ud_close(s);

nopub:
	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	nd_parser_free(argi);

	/* unloop was called, so exit */
	return 0;
}

/* um-netdania.c ends here */
