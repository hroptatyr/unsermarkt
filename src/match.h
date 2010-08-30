/*** match.h -- ctors and dtors for matches. */

#if !defined INCLUDED_dso_oq_match_h_
#define INCLUDED_dso_oq_match_h_

#include "m30.h"
#include "um-types.h"

typedef struct umm_s *umm_t;

struct umm_s {
	/* buyer and seller order ids */
	oid_t ob, os;
	/* buyer and seller agent ids */
	agtid_t ab, as;
	/* instr ids, buyer/seller */
	insid_t ib, is;
	/* agreed upon price */
	m30_t p;
	/* agreed upon quantity */
	uint32_t q;
};

#endif	/* !INCLUDED_dso_oq_match_h_ */
