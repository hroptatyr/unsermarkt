/*** dso-oq-order.h -- ctors and dtors for orders. */

#if !defined INCLUDED_dso_oq_order_h_
#define INCLUDED_dso_oq_order_h_

#include <stdint.h>
#include <sushi/m30.h>

typedef struct umo_s *umo_t;
#define um_order_s	umo_s
#define um_order_t	umo_t

typedef enum {
	OTYPE_UNK,
	OTYPE_MKT,
	OTYPE_LIM,
	NOTYPES
} otype_t;

typedef enum {
	OSIDE_UNK,
	OSIDE_SELL,
	OSIDE_BUY,
	NOSIDES,
} oside_t;

/* order modifiers */
typedef enum {
	OTYMOD_UNK,
	/* good till day */
	OTYMOD_GTD,
	/* good till cancelled */
	OTYMOD_GTC,
	/* fill-or-kill */
	OTYMOD_FOK,
	/* immediate-or-cancel, like FOK + partial fills allowed */
	OTYMOD_IOC,
	/* on-open orders */
	OTYMOD_OO,
	/* on-close orders */
	OTYMOD_OC,
	/* on-auction in general */
	OTYMOD_OA,
	/* stop orders */
	OTYMOD_STOP,
	/* market-if-touched orders */
	OTYMOD_MIT,
	NOTYMODS
} otymod_t;

struct um_order_s {
	uint32_t agent_id;
	uint32_t secu_id;
	/* price, 0 if MKT, otherwise limit price */
	m30_t p;
	/* quantity */
	m30_t q;
};


static inline void
make_order(umo_t o, uint32_t aid, uint32_t sid, oside_t s, m30_t p, m30_t q)
{
	o->agent_id = aid;
	o->secu_id = sid << 1;
	o->p = p;
	o->q = q;

	switch (s) {
	case OSIDE_UNK:
	case NOSIDES:
	default:
		o->secu_id = -1;
		return;
	case OSIDE_BUY:
		break;
	case OSIDE_SELL:
		o->secu_id |= 1;
	}
	return;
}

#endif	/* INCLUDED_dso_oq_order_h_ */
