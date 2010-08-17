/*** unsermarktd.c -- unsermarkt network service daemon
 *
 * Copyright (C) 2008 - 2010 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <popt.h>

/* our master include file */
#include "unserding.h"
/* context goodness, passed around internally */
#include "unserding-ctx.h"
/* our private bits */
#include "unserding-private.h"
/* proto stuff */
#include "protocore.h"
/* module handling */
#include "module.h"
/* worker pool */
#include "wpool.h"

#define UM_VERSION		"v0.1"

FILE *logout;


typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};

struct ud_loopclo_s {
	/** loop lock */
	pthread_mutex_t lolo;
	/** just a cond */
	pthread_cond_t loco;
};


/* module services */
struct ev_cb_clo_s {
	union {
		ev_idle idle;
		ev_timer timer;
	};
	void(*cb)(void*);
	void *clo;
};

static void
std_idle_cb(EV_P_ ev_idle *ie, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)ie;

	/* stop the idle event */
	ev_idle_stop(EV_A_ ie);

	/* call the call back */
	clo->cb(clo->clo);

	/* clean up */
	free(clo);
	return;
}

static void
std_timer_once_cb(EV_P_ ev_timer *te, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)te;

	/* stop the timer event */
	ev_timer_stop(EV_A_ te);

	/* call the call back */
	clo->cb(clo->clo);

	/* clean up */
	free(clo);
	return;
}

static void
std_timer_every_cb(EV_P_ ev_timer *te, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)te;

	/* call the call back */
	clo->cb(clo->clo);
	return;
}

void
schedule_once_idle(void *ctx, void(*cb)(void *clo), void *clo)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_idle_init((ev_idle*)f, std_idle_cb);
	ev_idle_start(ud_ctx->mainloop, (void*)f);
	return;
}

void
schedule_timer_once(void *ctx, void(*cb)(void *clo), void *clo, double in)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_timer_init((ev_timer*)f, std_timer_once_cb, in, 0.0);
	ev_timer_start(ud_ctx->mainloop, (void*)f);
	return;
}

void*
schedule_timer_every(void *ctx, void(*cb)(void *clo), void *clo, double every)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_timer_init((ev_timer*)f, std_timer_every_cb, every, every);
	ev_timer_start(ud_ctx->mainloop, (void*)f);
	return f;
}

void
unsched_timer(void *ctx, void *timer)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = timer;

	ev_timer_stop(ud_ctx->mainloop, timer);
	f->cb = NULL;
	f->clo = NULL;
	free(timer);
	return;
}


static ev_signal ALGN16(__sigint_watcher)[1];
static ev_signal ALGN16(__sighup_watcher)[1];
static ev_signal ALGN16(__sigterm_watcher)[1];
static ev_signal ALGN16(__sigpipe_watcher)[1];
static ev_signal ALGN16(__sigusr2_watcher)[1];
static ev_async ALGN16(__wakeup_watcher)[1];
ev_async *glob_notify;

/* worker magic */
static int nworkers = 1;

/* the global job queue */
jpool_t gjpool;
/* holds worker pool */
wpool_t gwpool;

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("SIGPIPE caught, doing nothing\n");
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("SIGHUP caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigusr2_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
#if 0
	open_aux("dso-cli", NULL);
	ud_mod_dump(logout);
#endif
	return;
}


static void
triv_cb(EV_P_ ev_async *UNUSED(w), int UNUSED(revents))
{
	return;
}


/* helper for daemon mode */
static bool daemonisep = 0;
static bool prefer6p = 0;

static int
daemonise(void)
{
	int fd;
	pid_t UNUSED(pid);

	switch (pid = fork()) {
	case -1:
		return false;
	case 0:
		break;
	default:
		UD_DEBUG("Successfully bore a squaller: %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return false;
	}
	for (int i = getdtablesize(); i>=0; --i) {
		/* close all descriptors */
		close(i);
	}
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) {
			(void)close(fd);
		}
	}
	logout = fopen("/tmp/unsermarkt.log", "w");
	return 0;
}


/* helper function for the worker pool */
static int
get_num_proc(void)
{
#if defined HAVE_PTHREAD_AFFINITY_NP
	long int self = pthread_self();
	cpu_set_t cpuset;

	if (pthread_getaffinity_np(self, sizeof(cpuset), &cpuset) == 0) {
		int ret = cpuset_popcount(&cpuset);
		if (ret > 0) {
			return ret;
		} else {
			return 1;
		}
	}
#endif	/* HAVE_PTHREAD_AFFINITY_NP */
#if defined _SC_NPROCESSORS_ONLN
	return sysconf(_SC_NPROCESSORS_ONLN);
#else  /* !_SC_NPROCESSORS_ONLN */
/* any ideas? */
	return 1;
#endif	/* _SC_NPROCESSORS_ONLN */
}


/* the popt helper */
static void
hlp(poptContext con, UNUSED(enum poptCallbackReason foo),
    struct poptOption *key, UNUSED(const char *arg), UNUSED(void *data))
{
	if (key->shortName == 'h') {
		poptPrintHelp(con, stdout, 0);
	} else if (key->shortName == 'V') {
		fprintf(stdout, "unsermarktd " UM_VERSION "\n");
	} else {
		poptPrintUsage(con, stdout, 0);
	}

#if !defined(__LCLINT__)
        /* XXX keep both splint & valgrind happy */
	con = poptFreeContext(con);
#endif
	exit(0);
	return;
}

static struct poptOption srv_opts[] = {
	{"prefer-ipv6", '6', POPT_ARG_NONE,
	 &prefer6p, 0,
	 "Prefer ipv6 traffic to ipv4 if applicable..", NULL},
	{"daemon", 'd', POPT_ARG_NONE,
	 &daemonisep, 0,
	 "Detach from tty and run as daemon.", NULL},
	{"workers", 'w', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,
	 &nworkers, 0,
	 "Number of concurrent worker threads.", NULL},
        POPT_TABLEEND
};

static struct poptOption help_opts[] = {
	{NULL, '\0', POPT_ARG_CALLBACK, (void*)hlp, 0, NULL, NULL},
	{"help", 'h', 0, NULL, '?', "Show this help message", NULL},
	{"version", 'V', 0, NULL, 'V', "Print version string and exit.", NULL},
	{"usage", '\0', 0, NULL, 'u', "Display brief usage message", NULL},
	POPT_TABLEEND
};

static const struct poptOption ud_opts[] = {
        {NULL, '\0', POPT_ARG_INCLUDE_TABLE, srv_opts, 0,
	 "Server Options", NULL},
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, help_opts, 0,
	 "Help options", NULL},
        POPT_TABLEEND
};

static const char *const*
ud_parse_cl(size_t argc, const char *argv[])
{
        poptContext opt_ctx;

        UD_DEBUG("parsing command line options\n");
        opt_ctx = poptGetContext(NULL, argc, argv, ud_opts, 0);
        poptSetOtherOptionHelp(
		opt_ctx,
		"[server-options] "
		"module [module [...]]");

        /* auto-do */
        while (poptGetNextOpt(opt_ctx) > 0) {
                /* Read all the options ... */
                ;
        }
        return poptGetArgs(opt_ctx);
}

#define GLOB_CFG_PRE	"/etc/unserding"
#if !defined MAX_PATH_LEN
# define MAX_PATH_LEN	64
#endif	/* !MAX_PATH_LEN */

/* do me properly */
static const char cfg_glob_prefix[] = GLOB_CFG_PRE;

#if defined USE_LUA
static const char cfg_file_name[] = "unsermarkt.lua";

static void
ud_expand_user_cfg_file_name(char *tgt)
{
	char *p;

	/* get the user's home dir */
	p = stpcpy(tgt, getenv("HOME"));
	*p++ = '/';
	*p++ = '.';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static void
ud_expand_glob_cfg_file_name(char *tgt)
{
	char *p;

	/* get the user's home dir */
	strncpy(tgt, cfg_glob_prefix, sizeof(cfg_glob_prefix));
	p = tgt + sizeof(cfg_glob_prefix);
	*p++ = '/';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static void
ud_read_config(ud_ctx_t ctx)
{
	char cfgf[MAX_PATH_LEN];

        UD_DEBUG("reading configuration from config file ...");
	lua_config_init(&ctx->cfgctx);

	/* we prefer the user's config file, then fall back to the
	 * global config file if that's not available */
	ud_expand_user_cfg_file_name(cfgf);
	if (read_lua_config(ctx->cfgctx, cfgf)) {
		UD_DBGCONT("done\n");
		return;
	}
	/* otherwise there must have been an error */
	ud_expand_glob_cfg_file_name(cfgf);
	if (read_lua_config(ctx->cfgctx, cfgf)) {
		UD_DBGCONT("done\n");
		return;
	}
	UD_DBGCONT("failed\n");
	return;
}

static void
ud_free_config(ud_ctx_t ctx)
{
	lua_config_deinit(&ctx->cfgctx);
	return;
}
#endif


/* static module loader */
static void
ud_init_statmods(void *UNUSED(clo))
{
	return;
}

static void
ud_deinit_statmods(void *UNUSED(clo))
{
	return;
}


int
main(int argc, const char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal *sigint_watcher = __sigint_watcher;
	ev_signal *sighup_watcher = __sighup_watcher;
	ev_signal *sigterm_watcher = __sigterm_watcher;
	ev_signal *sigpipe_watcher = __sigpipe_watcher;
	ev_signal *sigusr2_watcher = __sigusr2_watcher;
	const char *const *UNUSED(rest);
	struct ud_ctx_s __ctx[1] = {{0}};
	struct ud_handle_s UNUSED(__hdl[1]);

	/* whither to log */
	logout = stderr;
	/* obtain the number of cpus */
	nworkers = get_num_proc();

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);

	/* try and read the context file */
	ud_read_config(__ctx);

	daemonisep |= udcfg_glob_lookup_b(__ctx, "daemonise");
	prefer6p |= udcfg_glob_lookup_b(__ctx, "prefer_ipv6");

	/* run as daemon, do me properly */
	if (daemonisep) {
		daemonise();
	}
	/* check if nworkers is not too large */
	if (nworkers > MAX_WORKERS) {
		nworkers = MAX_WORKERS;
	}
	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);
	__ctx->mainloop = loop;

	/* initialise modules */
	ud_init_modules(rest, &__ctx);

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
	/* initialise a SIGUSR2 handler */
	ev_signal_init(sigusr2_watcher, sigusr2_cb, SIGUSR2);
	ev_signal_start(EV_A_ sigusr2_watcher);

	/* initialise a wakeup handler */
	glob_notify = __wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

#if 0
	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	ud_attach_mcast(EV_A_ prefer6p);
#endif

	/* static modules */
	ud_init_statmods(__ctx);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* deinitialise modules */
	ud_deinit_modules(__ctx);

	/* pong service */
	ud_deinit_statmods(__ctx);

#if 0
	/* close the socket */
	ud_detach_mcast(EV_A);
#endif

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	ud_free_config(__ctx);

	/* close our log output */	
	fflush(logout);
	fclose(logout);
	/* unloop was called, so exit */
	return 0;
}

/* unsermarktd.c ends here */