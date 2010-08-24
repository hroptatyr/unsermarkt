/*** match.h -- ctors and dtors for matches. */

#if !defined INCLUDED_dso_oq_match_h_
#define INCLUDED_dso_oq_match_h_

typedef struct umm_s *umm_t;

struct umm_s {
	/* buyer and seller order ids */
	oid_t ob, os;
	/* buyer and seller agent ids */
	agtid_t ab, as;
	/* agreed upon price */
	m30_t p;
	/* agreed upon quantity */
	uint32_t q;
	/* security id */
	secid_t secu_id;
	/* funding id */
	secid_t fund_id;
};

#endif	/* !INCLUDED_dso_oq_match_h_ */
