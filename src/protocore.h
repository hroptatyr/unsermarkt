/*** protocore.h -- unserding protocol guts
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

#if !defined INCLUDED_protocore_h_
#define INCLUDED_protocore_h_

#include <stdbool.h>
#include <stdint.h>
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */

/***
 * The unserding protocol in detail:
 *
 * - A simple packet is always 4096 bytes long
 * - Simple packets always read:
 *   offs 0x000 uint16_t FROM	system id, value 0 means not assigned yet
 *   offs 0x002 uint16_t TO	system id, value 0 means everyone
 *   offs 0x004 uint16_t TYPE	skdfj, see below
 *   offs 0x006 uint16_t PAD	just padding, must be magic number 0xbeef
 *
 ***/

typedef uint16_t ud_sysid_t;
typedef uint16_t ud_pkt_ty_t;

/* Simple packets, proto version 0.1 */
/**
 * HY packet, used to say `hy' to all attached servers and clients. */
#define UDPC_PKT_HY		(ud_pkt_ty_t)(htons(0x0001))

#define UDPC_SIMPLE_PKTLEN	4096
#define UDPC_MAGIC_NUMBER	(uint16_t)(htons(0xbeef))
#define UDPC_PKTSRC_UNK		(ud_sysid_t)0x00
#define UDPC_PKTDST_ALL		(ud_sysid_t)0x00

/**
 * Return true if the packet PKT is meant for us.
 * inline me? */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_for_us_p(const char *pkt, ud_sysid_t id);
/**
 * Copy the `hy' packet into PKT. */
extern inline void __attribute__((always_inline, gnu_inline))
udpc_hy_pkt(char *restrict pkt, ud_sysid_t id);


/* inlines */
extern inline bool __attribute__((always_inline, gnu_inline))
udpc_pkt_for_us_p(const char *pkt, ud_sysid_t id)
{
	const uint16_t *tmp = (const void*)pkt;
	/* check magic number */
	if (tmp[3] == UDPC_MAGIC_NUMBER) {
		if (tmp[1] == UDPC_PKTDST_ALL || tmp[1] == id) {
			return true;
		} else {
			return false;
		}
	}
	return false;
}

extern inline void __attribute__((always_inline, gnu_inline))
udpc_hy_pkt(char *restrict pkt, ud_sysid_t id)
{
	uint16_t *restrict tmp = (void*)pkt;
	tmp[0] = id;
	tmp[1] = UDPC_PKTDST_ALL;
	tmp[2] = UDPC_PKT_HY;
	tmp[3] = UDPC_MAGIC_NUMBER;
	return;
}

#endif	/* INCLUDED_protocore_h_ */