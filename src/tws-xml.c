/*** tws-xml.c -- conversion between IB/API structs and xml
 *
 * Copyright (C) 2011-2012 Ruediger Meier
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
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include <expat.h>
#include "prchunk.h"
#include "tws-xml.h"

struct xml_clo_s {
	int level;
	void(*cb)(tws_xml_req_t, void*);
	void *clo;
	struct tws_xml_req_s req[1];
};

static tws_xml_req_typ_t
parse_req_typ(const char *typ)
{
	if (!strcmp(typ, "market_data")) {
		return TWS_XML_REQ_TYP_MKT_DATA;
	}
	return TWS_XML_REQ_TYP_UNK;
}

static tws_cont_t
el_req_con(void *clo, const char **attr)
{
/* this is actually glue code between tws-cont and tws-xml */
	tws_cont_t x = make_cont();

	for (const char **p = attr; p && *p; p += 2) {
		tws_cont_build(x, p[0], p[1]);
	}
	return x;
}

static void
el_sta(void *clo, const char *elem, const char **attr)
{
	struct xml_clo_s *this = clo;

	if (this->level == 0 && !strcmp(elem, "TWSXML")) {
		/* aaah very good */
		this->level = 1;
		return;
	} else if (this->level == 0) {
		return;
	}

	if (this->level = 1 && !strcmp(elem, "request")) {
		memset(this->req, 0, sizeof(*this->req));

		for (const char **p = attr; p && *p; p += 2) {
			/* find the type */
			if (!strcmp(p[0], "type")) {
				this->req->typ = parse_req_typ(p[1]);
				break;
			}
		}

		this->level = 2;
	} else if (this->level == 2) {
		switch (this->req->typ) {
		case TWS_XML_REQ_TYP_UNK:
			break;
		case TWS_XML_REQ_TYP_MKT_DATA:
		case TWS_XML_REQ_TYP_HIST_DATA:
		case TWS_XML_REQ_TYP_CON_DTLS:
			/* yay */
			if (!strcmp(elem, "reqContract")) {
				/* even yay'er */
				this->req->qry.mkt_data.ins =
					el_req_con(clo, attr);
			}
			break;
		case TWS_XML_REQ_TYP_PLC_ORDER:
			break;
		}
	}
	return;
}

static void
el_end(void *clo, const char *elem)
{
	struct xml_clo_s *this = clo;

	if (this->level < 0) {
		;
	} else if (this->level > 0 && !strcmp(elem, "TWSXML")) {
		this->level = 0;
	} else if (!strcmp(elem, "request")) {
		if (this->cb) {
			this->cb(this->req, this->clo);
		}
		this->level = 1;
	}
	return;
}

static int
proc_line(XML_Parser hdl, const char *line, size_t llen)
{
	struct xml_clo_s *clo = XML_GetUserData(hdl);

	if (XML_Parse(hdl, line, llen, XML_FALSE) == XML_STATUS_ERROR) {
		return -1;
	} else if (clo->level < 0) {
		XML_StopParser(hdl, XML_TRUE);
	}
	return 0;
}


int
tws_xml_parse(const char *fn, void(*cb)(tws_xml_req_t, void*), void *cb_clo)
{
	XML_Parser hdl;
	void *pctx;
	int fd;
	struct xml_clo_s clo = {
		.level = 0,
		.cb = cb,
		.clo = cb_clo,
	};
	int res = 0;

	if (fn == NULL) {
		fd = STDIN_FILENO;
	} else if ((fd = open(fn, O_RDONLY)) < 0) {
		res = -1;
		goto super_fucked;
	}

	/* using the prchunk reader now */
	if ((pctx = init_prchunk(fd)) == NULL) {
		res = -1;
		goto fucked;
	}

	/* parser init */
	if ((hdl = XML_ParserCreate(NULL)) == NULL) {
		res = -1;
		goto fucked;
	}
	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, &clo);

	while (prchunk_fill(pctx) >= 0) {
		for (char *line; prchunk_haslinep(pctx);) {
			size_t llen;

			llen = prchunk_getline(pctx, &line);
			proc_line(hdl, line, llen);
		}
	}
	/* get rid of resources */
	free_prchunk(pctx);
	XML_ParserFree(hdl);

fucked:
	close(fd);
super_fucked:
	return res;
}


#if defined STANDALONE
static void
me_req(tws_xml_req_t req, void *clo)
{
	fprintf(stderr, "request type %u\n", req->typ);
	switch (req->typ) {
	case TWS_XML_REQ_TYP_MKT_DATA:
		fprintf(stderr, "aaah mkt data %p\n", req->qry.mkt_data.ins);
		break;
	default:
		break;
	}
	return;
}

int
main(int argc, char *argv[])
{
	int res = 0;

	if (tws_xml_parse(argv[1], me_req, NULL) < 0) {
		res = 1;
	}
	return res;
}
#endif	/* STANDALONE */

/* tws-xml.c ends here */
