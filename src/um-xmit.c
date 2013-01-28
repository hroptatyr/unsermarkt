/*** um-xmit.c -- transmission of ute files through unserding
 *
 * Copyright (C) 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <sys/epoll.h>
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#include <unserding/unserding.h>

#include "svc-uterus.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect(!!(_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect(!!(_x), 0)
#endif
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*(x)))
#endif	/* !countof */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#if defined DEBUG_FLAG
# define XMIT_DEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define XMIT_DEBUG(args...)
#endif	/* DEBUG_FLAG */
#define XMIT_STUP(arg)		fputc(arg, stdout)

#define UD_CMD_QMETA	(0x7572)
#define PKT(x)		(ud_packet_t){sizeof(x), x}

struct xmit_s {
	ud_sock_t ud;
	utectx_t ute;
	size_t nsyms;
	float speed;
	bool restampp;
	int epfd;
};

static jmp_buf jb;


static void
handle_sigint(int signum)
{
	longjmp(jb, signum);
	return;
}

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("[um-xmit]: ", stderr);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

#if !defined UTE_ITER
# define UTE_ITER(i, __ctx)						\
	for (size_t __i = 0, __tsz;					\
	     __i < ute_nticks(__ctx); __i += __tsz)			\
		for (scom_t i = ute_seek(__ctx, __i); i;		\
		     __tsz = scom_tick_size(i), i = 0)
#endif	/* !UTE_ITER */

static unsigned int pno = 0;
static size_t nt = 0;

static useconds_t
tv_diff(struct timeval *t1, struct timeval *t2)
{
	useconds_t res = (t2->tv_sec - t1->tv_sec) * 1000000;
	res += (t2->tv_usec - t1->tv_usec);
	return res;
}

static int
bang_qmeta(struct um_qmeta_s *restrict t, const struct xmit_s *ctx, uint32_t i)
{
	const char *sym;
	size_t len;

	if ((uint32_t)(i - 1) >= ctx->nsyms) {
		return -1;
	} else if ((sym = ute_idx2sym(ctx->ute, (uint16_t)i)) == NULL) {
		return -1;
	} else if ((len = strlen(sym)) == 0U) {
		return -1;
	}
	/* bang index and pack */
	t->idx = (uint16_t)i;
	t->sym = sym;
	t->symlen = len;
	t->uri = NULL;
	t->urilen = 0U;
	return 0;
}

static void
party(const struct xmit_s *ctx, useconds_t tm)
{
	struct epoll_event ev[1];
	struct timeval tv[2];
	int mil = tm / 1000;
	int mic = tm % 1000;

	gettimeofday(tv + 0, NULL);
	while (epoll_wait(ctx->epfd, ev, 1, mil) > 0) {
		struct ud_msg_s msg[1];
		useconds_t elps;

		/* otherwise be nosey and look at the packet */
		if ((ev->events & EPOLLIN) == 0) {
			/* must be HUP or ERR, don't bother reading it */
			;
		}

		while (ud_chck_msg(msg, ev->data.ptr) >= 0) {
			struct um_qmeta_s brg[1];

			if (msg->svc != UTE_QMETA) {
				/* fuck right off */
				continue;
			} else if (um_chck_msg_brag(brg, msg) < 0) {
				/* don't know, something's not right */
				continue;
			} else if (brg->symlen || brg->urilen) {
				/* not a genuine request */
				continue;
			} else if (bang_qmeta(brg, ctx, brg->idx) < 0) {
				/* packing failed */
				continue;
			}

			/* pack the reply */
			um_pack_brag(ctx->ud, brg);
		}

		/* make sure replies get sent */
		ud_flush(ctx->ud);

		/* check how long it took us */
		gettimeofday(tv + 1, NULL);
		elps = tv_diff(tv + 0, tv + 1);
		if (tm > elps + 1000) {
			mil = (tm - elps) / 1000 - 1;
			mic = (tm - elps) % 1000;
		} else if (tm > elps) {
			mil = 0;
			mic = (tm - elps);
		}
	}
	usleep(mic);
	return;
}

static void
shout_syms(const struct xmit_s *ctx)
{
/* convenience has us shouting out all the symbols (along with their
 * indices) in advance so that logging monitors actually know what
 * we're on about */
	for (size_t i = 1; i <= ctx->nsyms; i++) {
		struct um_qmeta_s brg[1];

		if (bang_qmeta(brg, ctx, i) < 0) {
			/* packing failed */
			continue;
		}

		/* pack the guy */
		um_pack_brag(ctx->ud, brg);
	}
	/* make sure it gets sent */
	ud_flush(ctx->ud);
	return;
}


/* ute services come in 2 flavours little endian "ut" and big endian "UT" */
#define UTE_CMD_LE	0x7574
#define UTE_CMD_BE	0x5554
#if defined WORDS_BIGENDIAN
# define UTE_CMD	UTE_CMD_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE_CMD	UTE_CMD_LE
#endif	/* WORDS_BIGENDIAN */

/* in usecs */
#define SHOUT_INTV	(10 * 1000 * 1000)

static void
work(const struct xmit_s *ctx)
{
	time_t reft = 0;
	unsigned int refm = 0;
	unsigned int sleep_since_last_shout = 0;
	const unsigned int speed = (unsigned int)(1000 * ctx->speed);

	UTE_ITER(ti, ctx->ute) {
		time_t stmp = scom_thdr_sec(ti);
		unsigned int msec = scom_thdr_msec(ti);
		char status;

		if (UNLIKELY(!reft)) {
			/* singleton */
			reft = stmp;
		}
		/* disseminate */
		if (((stmp > reft || msec > refm) && (status = '!', 1))) {
			XMIT_STUP(status);
			ud_flush(ctx->ud);
			XMIT_STUP('\n');
		}
		/* sleep, well maybe */
		if (stmp > reft || msec > refm) {
			useconds_t slp;

			/* and party hard for some microseconds */
			slp = ((stmp - reft) * 1000 + msec - refm) * speed;
			for (unsigned int i = slp, j = 0; i; i /= 2, j++) {
				XMIT_STUP('0' + (j % 10));
			}
			XMIT_STUP('\n');

			party(ctx, slp);
			if ((sleep_since_last_shout += slp) > SHOUT_INTV) {
				shout_syms(ctx);
				sleep_since_last_shout = 0;
			}

			refm = msec;
			reft = stmp;
		}
		/* add the scom in question to the pool */
		XMIT_STUP('+');
		{
			size_t bs = scom_byte_size(ti);

			if (ctx->restampp) {
				struct sndwch_s nuti[4];
				struct timeval now[1];

				gettimeofday(now, NULL);
				memcpy(nuti, ti, bs);

				/* replace the time stamp */
				AS_SCOM_THDR(nuti)->sec = now->tv_sec;
				AS_SCOM_THDR(nuti)->msec = now->tv_usec / 1000;

				/* reassign ptrs */
				ti = (const void*)nuti;
			}

			/* add the tick */
			um_pack_scom(ctx->ud, ti, bs);
		}
		nt++;
	}
	XMIT_STUP('/');
	ud_flush(ctx->ud);
	XMIT_STUP('\n');
	return;
}

static int
pre_work(const struct xmit_s *ctx)
{
	shout_syms(ctx);
	return 0;
}

static int
post_work(const struct xmit_s *UNUSED(ctx))
{
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "um-xmit-clo.h"
#include "um-xmit-clo.c"
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
	struct gengetopt_args_info argi[1];
	struct xmit_s ctx[1];
	short unsigned int port = 8584;
	int res = 0;

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		error(0, "need input file");
		res = 1;
		goto fr_out;
	} else if ((ctx->ute = ute_open(argi->inputs[0], UO_RDONLY)) == NULL) {
		error(0, "cannot open file '%s'", argi->inputs[0]);
		res = 1;
		goto fr_out;
	}

	if (argi->beef_given) {
		port = (short unsigned int)argi->beef_arg;
	}

	/* set signal handler */
	signal(SIGINT, handle_sigint);

	/* obtain a new handle, somehow we need to use the port number innit? */
	ctx->ud = ud_socket((struct ud_sockopt_s){UD_PUB, .port = port});
	if (UNLIKELY(ctx->ud == NULL)) {
		error(0, "cannot obtain unserding socket");
		goto ut_out;
	}

	/* also accept connections on that socket and the mcast network */
	if ((ctx->epfd = epoll_create(2)) < 0) {
		error(0, "cannot instantiate epoll on um-xmit socket");
		goto ud_out;
	} else {
		struct epoll_event ev[1];

		ev->events = EPOLLIN;
		ev->data.ptr = ctx->ud;
		epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->ud->fd, ev);
	}

	/* the actual work */
	switch (setjmp(jb)) {
	case 0:
		ctx->speed = argi->speed_arg;
		ctx->restampp = argi->restamp_given;
		ctx->nsyms = ute_nsyms(ctx->ute);
		if (pre_work(ctx) == 0) {
			/* do the actual work */
			work(ctx);
		}
	case SIGINT:
	default:
		if (post_work(ctx) < 0) {
			res = 1;
		}
		printf("sent %zu ticks in %u packets\n", nt, pno);
		break;	
	}

	/* close epoll */
	close(ctx->epfd);

ud_out:
	/* and lose the unserding handle again */
	ud_close(ctx->ud);

ut_out:
	/* and close the file */
	ute_close(ctx->ute);

fr_out:
	/* free up command line parser resources */
	cmdline_parser_free(argi);
out:
	return res;
}

/* um-xmit.c ends here */
