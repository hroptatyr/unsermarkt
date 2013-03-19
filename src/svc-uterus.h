/*** svc-uterus.h -- uterus service goodies
 *
 * Copyright (C) 2013 Sebastian Freundt
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
#if !defined INCLUDED_svc_uterus_h_
#define INCLUDED_svc_uterus_h_

#include <unistd.h>
#include <stdint.h>

#include <unserding/unserding.h>
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
#elif defined HAVE_UTERUS_H
# include <uterus.h>
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

/* ute services come in 2 flavours little endian "ut" and big endian "UT" */
#define UTE_CMD_LE	0x7574
#define UTE_CMD_BE	0x5554
#if defined WORDS_BIGENDIAN
# define UTE_CMD	UTE_CMD_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE_CMD	UTE_CMD_LE
#endif	/* WORDS_BIGENDIAN */
/* command to dispatch meta info */
#define UTE_QMETA	0x7572
#define UTE_ALLOC	(UTE_CMD + 0x02)

/**
 * Message we pass around on UTE_QMETA channel. */
struct um_qmeta_s {
	uint32_t idx;
	size_t symlen;
	const char *sym;
	size_t urilen;
	const char *uri;
};

extern int um_pack_brag(ud_sock_t s, const struct um_qmeta_s msg[static 1]);

extern int
um_chck_msg_brag(
	struct um_qmeta_s *restrict tgt, const struct ud_msg_s msg[static 1]);


static inline int
um_pack_scom(ud_sock_t s, scom_t q, size_t z)
{
	return ud_pack_msg(
		s, (struct ud_msg_s){
			.svc = UTE_CMD,
			.data = q,
			.dlen = z,
		});
}

static inline int
um_pack_sl1t(ud_sock_t s, const_sl1t_t q)
{
	return ud_pack_msg(
		s, (struct ud_msg_s){
			.svc = UTE_CMD,
			.data = q,
			.dlen = sizeof(*q),
		});
}

static inline int
um_pack_alloc(ud_sock_t s, const_sl1t_t q)
{
	return ud_pack_msg(
		s, (struct ud_msg_s){
			.svc = UTE_ALLOC,
			.data = q,
			.dlen = sizeof(*q),
		});
}

#endif	/* INCLUDED_svc_uterus_h_ */
