
#if !defined INCLUDED_oq_h_
#define INCLUDED_oq_h_

#include "order.h"

typedef struct umoq_s *umoq_t;

/* level struct */
typedef struct uml_s *uml_t;

typedef enum {
	OSTATUS_UNK,
	/* order is new and untouched */
	OSTATUS_NEW,
	/* order has been filled partially */
	OSTATUS_PFILLED,
	/* order has been filled */
	OSTATUS_FILLED,
	/* order has been cancelled */
	OSTATUS_CANC,
	/* order has been replaced */
	OSTATUS_REPLC,
	/* order has been rejected */
	OSTATUS_REJ,
	/* order has expired */
	OSTATUS_EXP,
	/* order has been suspended */
	OSTATUS_SUSP,
	NOSTATUS
} umost_t;

/**
 * Level structure, this accumulates all orders with the same price. */
struct uml_s {
	/* price */
	m30_t p;
	/* total quantity */
	uint32_t q;
};


/* ctor/dtor,
 * we expect one order queue per security, so the queue is completely
 * oblivious to security ids */
extern umoq_t make_oq(insid_t secu_id, insid_t fund_id);
extern void free_oq(umoq_t);


/* order queue operations */
/**
 * Add an order to the queue and return an order id. */
extern oid_t oq_add_order(umoq_t, umo_t);

/**
 * Return the order matching OID. */
extern struct umo_s oq_get_order(umoq_t, oid_t);

/**
 * Cancel the order with the order id OID. */
extern int oq_cancel_order(umoq_t, oid_t);

/**
 * Return the status of the order OID. */
extern umost_t oq_get_status(umoq_t, oid_t);

/**
 * Suspend the order with the order id OID. */
extern int oq_suspend_order(umoq_t, oid_t);

/**
 * Resume the order with the order id OID. */
extern int oq_resume_order(umoq_t, oid_t);

/**
 * For status stuff. */
extern int oq_trav_bids(umoq_t, void(*cb)(uml_t, void*), void *closure);
extern int oq_trav_asks(umoq_t, void(*cb)(uml_t, void*), void *closure);

#endif	/* !INCLUDED_oq_h_ */
