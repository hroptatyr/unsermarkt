/*** ox-matchall.c -- trivial order execution
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
#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <sys/mman.h>
#include <unserding/unserding.h>
#include <unserding/protocore.h>
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
#else
# error uterus headers are mandatory
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */
#include "nifty.h"
#include "match.h"

#if defined DEBUG_FLAG
# define OX_DEBUG(args...)	fprintf(logerr, args)
#else  /* !DEBUG_FLAG */
# define OX_DEBUG(args...)
#endif	/* DEBUG_FLAG */
static FILE *logerr;


/* the actual core */
#define UTE_LE		(0x7574)
#define UTE_BE		(0x5554)
#define QMETA		(0x7572)
#define QMETA_RPL	(UDPC_PKT_RPL(QMETA))
#if defined WORDS_BIGENDIAN
# define UTE		UTE_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE		UTE_LE
#endif	/* WORDS_BIGENDIAN */
#define UTE_RPL		(UDPC_PKT_RPL(UTE))
/* unsermarkt match messages */
#define UMM		(0x7576)
#define UMM_RPL		(UDPC_PKT_RPL(UMM))

static size_t umm_pno = 0;

static inline void
udpc_seria_add_scom(udpc_seria_t sctx, scom_t s, size_t len)
{
	memcpy(sctx->msg + sctx->msgoff, s, len);
	sctx->msgoff += len;
	return;
}

static inline void
udpc_seria_add_umm(udpc_seria_t sctx, umm_pair_t p)
{
	memcpy(sctx->msg + sctx->msgoff, p, sizeof(*p));
	sctx->msgoff += sizeof(*p);
	return;
}

static void
snarf_data(job_t j, ud_chan_t c)
{
	static char rpl[UDPC_PKTLEN];
	static char umm[UDPC_PKTLEN];
	struct udpc_seria_s ser[2];
	char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
	size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);
	uint8_t cno;
	uint32_t pno;

	if (UNLIKELY(plen == 0)) {
		return;
	}

#define PKT(x)		((ud_packet_t){sizeof(x), x})
	cno = udpc_pkt_cno(PKT(rpl));
	pno = udpc_pkt_pno(PKT(rpl));

	/* this is the normal trade announcement on the beef channel */
	udpc_make_pkt(PKT(rpl), cno, pno, UTE_RPL);
	udpc_set_data_pkt(PKT(rpl));
	udpc_seria_init(ser + 0, UDPC_PAYLOAD(rpl), UDPC_PAYLLEN(sizeof(rpl)));

	/* this is the match message */
	udpc_make_pkt(PKT(umm), 0, umm_pno++, UMM);
	udpc_set_data_pkt(PKT(umm));
	udpc_seria_init(ser + 1, UDPC_PAYLOAD(umm), UDPC_PAYLLEN(sizeof(umm)));

	for (scom_thdr_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
	     sp < ep;
	     sp += scom_tick_size(sp) *
		     (sizeof(struct sndwch_s) / sizeof(*sp))) {
		uint16_t ttf = scom_thdr_ttf(sp);

		switch (ttf) {
			struct timeval now[1];
			struct sl1t_s tmp[1];
			struct umm_pair_s mmp[1];
		case SL1T_TTF_BID:
		case SL1T_TTF_ASK:
		case SL2T_TTF_BID:
		case SL2T_TTF_ASK:
			/* make sure it isn't a cancel */
			if (UNLIKELY(((sl1t_t)sp)->qty == 0)) {
				/* too bad, it's a cancel */
				break;
			}

			/* get current stamp */
			gettimeofday(now, NULL);

			/* prepare a match message, always big-endian? */
			memcpy(mmp, sp, sizeof(*tmp));

			/* buyer ... */
			if (ttf == SL1T_TTF_BID || ttf == SL2T_TTF_BID) {
				mmp->agt[0].addr = j->sa.sa6.sin6_addr;
				mmp->agt[0].port = j->sa.sa6.sin6_port;
			}
			/* ... and seller */
			if (ttf == SL1T_TTF_BID || ttf == SL2T_TTF_BID) {
				mmp->agt[1].addr = j->sa.sa6.sin6_addr;
				mmp->agt[1].port = j->sa.sa6.sin6_port;
			}

			/* and serialise it */
			udpc_seria_add_umm(ser + 1, mmp);

			/* prepare the reply message */
			memcpy(tmp, sp, sizeof(*tmp));
			sl1t_set_ttf(tmp, SL1T_TTF_TRA);
			sl1t_set_stmp_sec(tmp, now->tv_sec);
			sl1t_set_stmp_msec(tmp, now->tv_usec / 1000);

			/* and off it goes */
			udpc_seria_add_scom(ser, AS_SCOM(tmp), sizeof(*tmp));
			break;
		default:
			break;
		}
	}
	ud_chan_send_ser(c, ser + 0);
	ud_chan_send_ser(c, ser + 1);
	return;
}


static void
beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nrd;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	nrd = recvfrom(w->fd, j->buf, sizeof(j->buf), 0, &j->sa.sa, &lsa);

	/* handle the reading */
	if (UNLIKELY(nrd < 0)) {
		goto out_revok;
	} else if (nrd == 0) {
		/* no need to bother */
		goto out_revok;
	} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, j->buf})) {
		goto out_revok;
	}

	/* preapre a job */
	j->blen = nrd;

	/* intercept special channels */
	switch (udpc_pkt_cmd(JOB_PACKET(j))) {
	case UTE:
		snarf_data(j, w->data);
		break;
	default:
		break;
	}

out_revok:
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
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
#include "ox-matchall-clo.h"
#include "ox-matchall-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

static pid_t
detach(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return -1;
	case 0:
		break;
	default:
		/* i am the parent */
		OX_DEBUG("daemonisation successful %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return -1;
	}
	/* close standard tty descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	/* reattach them to /dev/null */
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
	}
#if defined DEBUG_FLAG
	logerr = fopen("/tmp/ox-matchall.log", "w");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	/* args */
	struct ox_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	/* our beef channels */
	size_t nbeef = 0;
	ev_io *beef = NULL;
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (ox_parser(argc, argv, argi)) {
		exit(1);
	}

	if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	ev_signal_init(sighup_watcher, sigall_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	union __chan_u {
		ud_chan_t c;
		void *p;
	};
	{
		union __chan_u x = {ud_chan_init(UD_NETWORK_SERVICE)};
		int s = ud_chan_init_mcast(x.c);

		beef->data = x.p;
		ev_io_init(beef, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		union __chan_u x = {ud_chan_init(argi->beef_arg[i])};
		int s = ud_chan_init_mcast(x.c);

		beef[i + 1].data = x.p;
		ev_io_init(beef + i + 1, beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_chan_t c = beef[i].data;

		ev_io_stop(EV_A_ beef + i);
		ud_chan_fini(c);
	}
	/* free beef resources */
	free(beef);

	/* destroy the default evloop */
	ev_default_destroy();

out:
	/* kick the config context */
	ox_parser_free(argi);

	/* unloop was called, so exit */
	return res;
}

/* ox-matchall.c ends here */
