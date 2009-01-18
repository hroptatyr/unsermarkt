/*** instr.c -- unserding instruments
 *
 * Copyright (C) 2008 Sebastian Freundt
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"

static inline ud_tlv_t
make_tlv(ud_tag_t tag, uint8_t size)
{
	ud_tlv_t res = (void*)malloc(sizeof(ud_tag_t) + size);
	res->tag = tag;
	return res;
}

static inline ud_tlv_t
make_class(const char *cls, uint8_t size)
{
	ud_tlv_t res = make_tlv(UD_TAG_CLASS, size+1);
	res->data[0] = (char)size;
	memcpy(res->data + 1, cls, size);
	return res;
}

void
init_instr(void)
{
	static const char instr[] = "instrument";
	ud_tlv_t tmp = make_tlv(UD_TAG_CLASS, countof_m1(instr));

	UD_DEBUG("adding instruments\n");
#if 0
	instr = ud_cat_add_child(ud_catalogue, "instruments", UD_CF_JUSTCAT);
	equit = ud_cat_add_child(instr, "equity", UD_CF_JUSTCAT);
	commo = ud_cat_add_child(instr, "commodity", UD_CF_JUSTCAT);
	curnc = ud_cat_add_child(instr, "currency", UD_CF_JUSTCAT);
	intrs = ud_cat_add_child(instr, "interest", UD_CF_JUSTCAT);
	deriv = ud_cat_add_child(instr, "derivative", UD_CF_JUSTCAT);
#endif
	return;
}

/* instr.c ends here */
