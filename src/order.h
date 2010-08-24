/*** dso-oq-order.h -- ctors and dtors for orders. */

#if !defined INCLUDED_dso_oq_order_h_
#define INCLUDED_dso_oq_order_h_

#include <stdint.h>
#define WITH_M30_CMP
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "um-types.h"

typedef struct umo_s *umo_t;
#define um_order_s	umo_s
#define um_order_t	umo_t

typedef enum {
	OTYPE_UNK,
	OTYPE_MKT,
	OTYPE_LIM,
	/* market to limit orders, only clears the top level then takes
	 * that price and puts a limit order at that price for the rest
	 * of the order */
	OTYPE_MTL,
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

/* public part */
struct umo_s {
	agtid_t agent_id;
	secid_t secu_id;
	/* price, 0 if MKT, otherwise limit price */
	m30_t p;
	/* quantity */
	uint32_t q;
	/* all the rest */
	uint32_t side:2;
	uint32_t type:2;
	uint32_t tymod:28;
};


/* getters */
static inline m30_t
um_order_price(umo_t o)
{
	return o->p;
}

static inline oside_t
um_order_side(umo_t o)
{
	return (oside_t)o->side;
}

static inline otype_t
um_order_type(umo_t o)
{
	return (otype_t)o->type;
}

#endif	/* INCLUDED_dso_oq_order_h_ */
