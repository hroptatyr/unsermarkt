/*** tws-xml.h -- conversion between IB/API structs and xml
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

#if !defined INCLUDED_tws_xml_h_
#define INCLUDED_tws_xml_h_

#include "tws-cont.h"

/* tws xml is still non-normative */
typedef enum {
	TWS_XML_REQ_TYP_UNK,
	TWS_XML_REQ_TYP_MKT_DATA,
	TWS_XML_REQ_TYP_HIST_DATA,
	TWS_XML_REQ_TYP_CON_DTLS,
	TWS_XML_REQ_TYP_PLC_ORDER,
} tws_xml_req_typ_t;

typedef struct tws_xml_req_s *tws_xml_req_t;

union tws_xml_qry_u {
	/* mkt data */
	struct {
		tws_cont_t ins;
	} mkt_data;
	struct {
		tws_cont_t ins;
	} hist_data;
	struct {
		tws_cont_t ins;
	} con_dtls;
};

union tws_xml_rsp_u {
};

struct tws_xml_req_s {
	tws_xml_req_typ_t typ;
	union tws_xml_qry_u qry;
	union tws_xml_rsp_u rsp;
};

extern int
tws_xml_parse(const char *fn, void(*cb)(tws_xml_req_t, void*), void *clo);

#endif	/* INCLUDED_tws_xml_h_ */
