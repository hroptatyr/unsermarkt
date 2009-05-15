/*** module.c -- the module loader
 *
 * Copyright (C) 2005 - 2009 Sebastian Freundt
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
#endif	/* HAVE_CONFIG_H */
#if defined HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if defined HAVE_STDIO_H
# include <stdio.h>
#endif
#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif
#if defined HAVE_STDBOOL_H
# include <stdbool.h>
#endif
#if defined HAVE_STRING_H
# include <string.h>
#endif
/* check for me */
#include <ltdl.h>

#include "module.h"
#include "unserding-dbg.h"

/**
 * \addtogroup dso
 * \{ */

typedef struct ud_mod_s *ud_mod_t;

struct ud_mod_s {
	void *handle;
	void (*initf)(void*);
	void (*finif)(void*);
	void (*relof)(void*);
	ud_mod_t next;
};

static ud_mod_t ud_mods = NULL;

static ud_mod_t
been_there(void *handle)
{
	for (ud_mod_t m = ud_mods; m; m = m->next) {
		if (m->handle == handle) {
			return m;
		}
	}
	return NULL;
}

/**
 * Open NAME, call `init(CLO)' there. */
void*
open_aux(const char *name, void *clo)
{
	char minit[] = "init\0\0\0\0\0\0\0";
	char mdeinit[] = "deinit\0\0\0\0\0";
	char mreinit[] = "reinit\0\0\0\0\0";
	struct ud_mod_s tmpmod;
	ud_mod_t m;

	tmpmod.handle = lt_dlopenext(name);
	if (tmpmod.handle == NULL) {
		perror("unserding: cannot open module");
		return NULL;
	}

	if ((m = been_there(tmpmod.handle)) != NULL) {
		goto reinit;
	}

	if ((tmpmod.initf = lt_dlsym(tmpmod.handle, minit)) == NULL) {
		lt_dlclose(tmpmod.handle);
		perror("unserding: cannot open module, init() not found");
		return NULL;
	}

	if ((tmpmod.finif = lt_dlsym(tmpmod.handle, mdeinit)) == NULL) {
		;
	}

	if ((tmpmod.relof = lt_dlsym(tmpmod.handle, mreinit)) == NULL) {
		;
	}

	/* lock the master list? */
	tmpmod.next = ud_mods;

	/* reserve a new slot in our big list */
	m = malloc(sizeof(*m));
	/* fill him in */
	*m = tmpmod;
	/* call the init() function */
	(*m->initf)(clo);
	return ud_mods = m;

reinit:
	fprintf(stderr, "reloading\n");
	if (m->relof != NULL) {
		(*m->relof)(clo);
	}
	return m;
}

void
close_aux(void *mod, void *clo)
{
	ud_mod_t m = mod;

	if (m->finif != NULL) {
		(*m->finif)(clo);
	}
	lt_dlclose(m->handle);

	m->handle = NULL;
	m->initf = NULL;
	m->finif = NULL;
	m->relof = NULL;
	return;
}

void*
find_sym(void *handle, const char *sym_name)
{
	return lt_dlsym(handle, sym_name);
}


static int
dump_cb(ud_mod_t m, FILE *whither)
{
	const lt_dlinfo *mi = lt_dlgetinfo(m->handle);
	fprintf(whither, "%s\n", mi->name);
	return 0;
}

void
ud_mod_dump(FILE *whither)
{
	/* say ello to the log output */
	fprintf(whither, "[unserding/modules] dump requested\n");
	/* make sure we let them know about the module path */
	fprintf(whither, "searchpath: \"%s\"\n", lt_dlgetsearchpath());
	/* now dump the module names */
	for (ud_mod_t m = ud_mods; m; m = m->next) {
		if (m->handle == NULL) {
			continue;
		}
		dump_cb(m, whither);
	}
	return;
}


void
ud_init_modules(const char *const *rest, void *clo)
{
	/* linux only */
	char buffer[BUFSIZ], *s;

	/* initialise the dl system */
	lt_dlinit();

	readlink("/proc/self/exe", buffer, BUFSIZ);
	if ((s = strrchr(buffer, '/')) != NULL) {
		*s = '\0';
	}

	if (lt_dlsetsearchpath(buffer)) {
		return;
	}

	/* now load modules */
	if (rest == NULL) {
		/* no modules at all */
		return;
	}
	/* initialise all modules specified on the command line
	 * one of which is assumed to initialise the global deposit somehow
	 * if not, we care fuckall, let the bugger crash relentlessly */
	for (const char *const *mod = rest; *mod; mod++) {
		UD_DEBUG("loading module \"%s\"\n", *mod);
		open_aux(*mod, clo);
	}
	return;
}

void
ud_deinit_modules(void *clo)
{
	for (ud_mod_t m = ud_mods; m; m = m->next) {
		UD_DEBUG("unloading module %p\n", m);
		close_aux(m, clo);
	}
	return;
}

/**
 * \} */

/* modules.c ends here */
