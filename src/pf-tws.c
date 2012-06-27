/*** pf-tws.c -- portfolio management through tws
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
#include <stdio.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <fcntl.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include <netinet/in.h>
#include <netdb.h>
#include <sys/mman.h>

#include <unserding/unserding.h>
#include <unserding/protocore.h>

/* the tws api */
#include "pf-tws-wrapper.h"
#include "pf-tws-private.h"
#include "nifty.h"

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

#if defined DEBUG_FLAG
# include <assert.h>
# define PF_DEBUG(args...)	fprintf(logerr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define PF_DEBUG(args...)
# define assert(x)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
void *logerr;

struct comp_s {
	struct in6_addr addr;
	uint16_t port;
};


/* the actual core */
#define POS_RPT		(0x757a)
#define POS_RPT_RPL	(UDPC_PKT_RPL(POS_RPT))

static struct comp_s counter = {0};


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
	case POS_RPT:
		break;
	case POS_RPT_RPL:
		break;
	default:
		break;
	}

out_revok:
	return;
}

static void
cake_cb(EV_P_ ev_io *w, int revents)
{
	my_tws_t tws = w->data;

	if (revents & EV_READ) {
		tws_recv(tws);
	}
	if (revents & EV_WRITE) {
		tws_send(tws);
	}
	return;
}

static void
prep_cb(EV_P_ ev_prepare *UNUSED(w), int UNUSED(revents))
{
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
#include "pf-tws-clo.h"
#include "pf-tws-clo.c"
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
		PF_DEBUG("daemonisation successful %d\n", pid);
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
	logerr = fopen("/tmp/pf-tws.log", "w+");
#else  /* !DEBUG_FLAG */
	logerr = fdopen(fd, "w");
#endif	/* DEBUG_FLAG */
	return pid;
}

int
main(int argc, char *argv[])
{
	/* args */
	struct pf_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	/* ev goodies */
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	/* tws specific stuff */
	struct my_tws_s tws[1];
	const char *host;
	uint16_t port;
	int client;
	ev_io cake[1];
	ev_prepare prp[1];
	/* our beef channels */
	size_t nbeef = 0;
	ev_io *beef = NULL;
	/* final result */
	int res = 0;

	/* big assignment for logging purposes */
	logerr = stderr;

	/* parse the command line */
	if (pf_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* snarf host name and port */
	if (argi->tws_host_given) {
		host = argi->tws_host_arg;
	} else {
		host = "localhost";
	}
	if (argi->tws_port_given) {
		port = (uint16_t)argi->tws_port_arg;
	} else {
		port = (uint16_t)7474;
	}
	if (argi->tws_client_id_given) {
		client = argi->tws_client_id_arg;
	} else {
		struct timeval now[1];

		(void)gettimeofday(now, NULL);
		client = now->tv_sec;
	}

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	if ((beef = malloc(nbeef * sizeof(*beef))) == NULL) {
		res = 1;
		goto out;
	}

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

	/* we init this late, because the tws connection is not a requirement
	 * and we may have support for changing/closing/reopening it later
	 * on anyway, maybe through a nice shell, who knows */
	{
		int s;

		if (init_tws(tws) < 0) {
			res = 1;
			goto unroll;
		} else if ((s = tws_connect(tws, host, port, client)) < 0) {
			res = 1;
			goto past_loop;
		}

		/* init a watcher */
		cake->data = tws;
		ev_io_init(cake, cake_cb, s, EV_READ);
		ev_io_start(EV_A_ cake);

		/* pre and post poll hooks */
		prp->data = tws;
		ev_prepare_init(prp, prep_cb);
		ev_prepare_start(EV_A_ prp);

		/* also hand out details to the COUNTER struct */
		{
			union ud_sockaddr_u sa;
			socklen_t ss = sizeof(sa);

			getsockname(s, &sa.sa, &ss);
			counter.addr = sa.sa6.sin6_addr;
			counter.port = htons(port);
		}
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

past_loop:
	(void)fini_tws(tws);

	/* detaching beef channels */
	for (size_t i = 0; i < nbeef; i++) {
		ud_chan_t c = beef[i].data;

		ev_io_stop(EV_A_ beef + i);
		ud_chan_fini(c);
	}
	/* free beef resources */
	free(beef);

unroll:
	/* destroy the default evloop */
	ev_default_destroy();
out:
	pf_parser_free(argi);
	return res;
}

/* pf-tws.c ends here */
