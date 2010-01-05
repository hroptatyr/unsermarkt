/*** dso-tseries-ute.c -- ute tick files
 *
 * Copyright (C) 2009 Sebastian Freundt
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

#include <sushi/sl1t.h>
#include <sushi/sl1tfile.h>
/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "unserding-private.h"
/* for higher level packet handling */
#include "seria-proto-glue.h"
#include "tseries-private.h"


/* ugly */
#include "uteseries.c"


static void *my_ctx;

void
fetch_urn_ute(void)
{
/* make me thread-safe and declare me */
	if (my_ctx == NULL) {
		return;
	}

	UD_DEBUG("inspecting sl1t ...");
	fill_urns(my_ctx);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: loading ...");
	my_ctx = open_ute_file(my_hardcoded_file);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: unloading ...");
	if (my_ctx != NULL) {
		close_ute_file(my_ctx);
	}
	UD_DBGCONT("done\n");
	return;
}

/* dso-tseries-ute.c ends here */
