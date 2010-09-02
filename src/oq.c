/**
 * Commentary:
 * Here's roughly what this does: Maintain two queues, the order queue and
 * the level queue.  The order queue */

#include <stdlib.h>
#include <string.h>
/* rudi's favourite */
#include <assert.h>
//#include <stdio.h>

/* very abstract list provider */
#include "mmls.c"

/* for orders */
#include "order.h"
/* for matches */
#include "match.h"
/* our stuff */
#include "oq.h"

#define INITIAL_NUMOQ	(1024)

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define xnew(_x)	(malloc(sizeof(_x)))
#define xfree(_x)	(free(_x))

/**
 * Helper structure to store a bit more than just the umo order. */
typedef struct umoq_o_s *umoq_o_t;

/**
 * Matching cache.  This is to give the settlement module a more succinct
 * list of events. */
typedef struct umoq_m_s *umoq_m_t;

/**
 * Helper structure around an uml_s level struct. */
typedef struct umoq_l_s *umoq_l_t;

struct umoq_m_s {
	umoq_m_t next;
	umoq_m_t prev;
	struct umm_s m[1];
};

/* like order_s but has status and oid slots and can be chained through NEXT */
struct umoq_o_s {
	umoq_o_t next;
	struct umo_s o[1];
	uint32_t oid;
	umost_t st;
	/* up pointer to the level structure */
	umoq_l_t lev;
};

/* helper structure to chain uml_s levels together */
struct umoq_l_s {
	umoq_l_t next;
	struct uml_s l[1];
	/* guard pointer to umoq_o chain, this does not point to the
	 * first umoq_o cell at the given price level but to the
	 * last cell of the previous price level. */
	umoq_o_t ord;
};

/* the overall pointer structure is liek this:
 * -bids-> o11 -> o12 -> o21 -> o22 -> o31
 *    ^           ^              ^
 *    +----+      |     +--------+
 *         |      |     |
 * -blev-> l1 -> l2 -> l3
 *
 * And the other
 *
 * where o1 and o2 belong to l1, o3 and o4 belong to l2 and o5 belongs to l3.
 * Pointers generally point to the guard cell. */

struct umoq_s {
	mmls_t ols;
	mmls_t lls;
	/* bid orders, sorted by price, desc */
	struct umoq_o_s ob[1];
	/* ask orders, sorted by price, asc */
	struct umoq_o_s oa[1];
	/* suspended orders */
	struct umoq_o_s os[1];
	/* order id counter, should be global */
	uint32_t oid;

	/* level slots, accum'd,
	 * levels are guarded by lb[0] and lb[1] or
	 * la[0] and la[1] respectively
	 * all levels are in between them */
	struct umoq_l_s lb[2];
	struct umoq_l_s la[2];

	/* matching queue */
	mmls_t mls;
	struct umoq_m_s ms[2];

	/* security id and funding id */
	insid_t secid;
	insid_t funid;

	/* match callback, other cbs could be order status changes */
	void(*match_cb)(umm_t, void*);
	void *match_clo;
};


/* helper functions */
static umoq_o_t
best_ask(umoq_t q)
{
	return q->oa->next;
}

static umoq_o_t
best_bid(umoq_t q)
{
	return q->ob->next;
}

static umoq_o_t
susp_head(umoq_t q)
{
/* return the pointer to the first suspended order */
	return q->os->next;
}

static umoq_o_t
pop_o(umoq_t q)
{
	umoq_o_t res = mmls_pop_cell(q->ols);
	return res;
}

static void
push_o(umoq_t q, umoq_o_t o)
{
	o->next = (void*)0xdeadbeef;
	mmls_push_cell(q->ols, o);
	return;
}

static umoq_l_t
pop_l(umoq_t q)
{
	umoq_l_t res = mmls_pop_cell(q->lls);
	return res;
}

static void
push_l(umoq_t q, umoq_l_t l)
{
	l->next = (void*)0xdeadbeef;
	mmls_push_cell(q->lls, l);
	return;
}

static umoq_m_t
pop_m(umoq_t q)
{
	umoq_m_t res = mmls_pop_cell(q->mls);
	return res;
}

static void
push_m(umoq_t q, umoq_m_t m)
{
	m->next = (void*)0xdeadbeef;
	mmls_push_cell(q->mls, m);
	return;
}


static umoq_l_t
ins_new_level_after(umoq_t q, umoq_l_t l, m30_t p)
{
	umoq_l_t new = pop_l(q);
	/* insert after l */
	new->next = l->next, l->next = new;
	new->l->p = p;
	new->l->q = 0;
	new->ord = new->next->ord;
	return new;
}

static umoq_l_t
rem_order_from_level(umoq_t q, umoq_o_t io)
{
	umoq_l_t il = io->lev;

	assert(il->l->p.v == io->o->p.v);
	assert(il->l->q >= io->o->q);
	if ((il->l->q -= io->o->q) == 0) {
		umoq_l_t prev = il->ord->lev;
		/* discard the whole level? */
		prev->next = il->next;
		il->next->ord = il->ord;
		push_l(q, il);
	}
	return il;
}

static umoq_o_t
find_by_side_by_oid(umoq_o_t s, oid_t oid)
{
	for (; s; s = s->next) {
		if (s->oid == oid) {
			return s;
		}
	}
	return NULL;
}

static umoq_o_t
find_by_oid(umoq_t q, oid_t oid)
{
	umoq_o_t res;
	/* try and find it in the bids, then asks, then suspended orders */
	if ((res = find_by_side_by_oid(best_bid(q), oid)) != NULL) {
		return res;
	} else if ((res = find_by_side_by_oid(best_ask(q), oid)) != NULL) {
		return res;
	} else if ((res = find_by_side_by_oid(susp_head(q), oid)) != NULL) {
		return res;
	}
	return NULL;
}

static umoq_o_t
rem_by_side_by_oid(umoq_o_t s, oid_t oid)
{
/* remove the order with the order id OID and return it's umoq_o cell
 * Note: we require S to have a guard pointer at the beginning,
 * so S itself cannot be subject to deletion. */
	for (; s->next; s = s->next) {
		if (s->next->oid == oid) {
			umoq_o_t res = s->next;
			s->next = s->next->next, res->next = NULL;
			return res;
		}
	}
	return NULL;
}

static umoq_o_t
rem_by_oid(umoq_t q, oid_t oid)
{
/* like find_by_oid but returns the predecessor */
	umoq_o_t res;

	if ((res = rem_by_side_by_oid(q->ob, oid)) != NULL) {
		/* update the corresponding level */
		rem_order_from_level(q, res);
		return res;
	} else if ((res = rem_by_side_by_oid(q->oa, oid)) != NULL) {
		/* update the corresponding level */
		rem_order_from_level(q, res);
		return res;
	} else if ((res = rem_by_side_by_oid(q->os, oid)) != NULL) {
		/* no level to update here */
		return res;
	}
	return NULL;
}

static umoq_l_t
bid_side_level(umoq_t q, m30_t p)
{
/* returns the cons cell that has a better price than L, or NULL if
 * it's the best price */
	umoq_l_t l;

	/* look for a suitable cons cell */
	for (l = q->lb; l->next != q->lb + 1; l = l->next) {
		m30_t pp = l->next->l->p;
		if (pp.v == p.v) {
			return l->next;
		} else if (pp.v < p.v) {
			break;
		}
	}
	/*  the price level not existant, create one */
	return ins_new_level_after(q, l, p);
}

static umoq_l_t
ask_side_level(umoq_t q, m30_t p)
{
/* returns the cons cell that has a better price than L, or NULL if
 * it's the best price */
	umoq_l_t l;

	/* look for a suitable cons cell */
	for (l = q->la; l->next != q->la + 1; l = l->next) {
		m30_t pp = l->next->l->p;
		if (pp.v == p.v) {
			return l->next;
		} else if (pp.v > p.v) {
			break;
		}
	}
	/*  the price level not existant, create one */
	return ins_new_level_after(q, l, p);
}

static int
add_order(umoq_t q, umoq_o_t io)
{
	umoq_l_t lev;
	umoq_o_t prev;

	/* we should try n match the order before we try anything else */
	switch (um_order_side(io->o)) {
	case OSIDE_BUY:
		lev = bid_side_level(q, io->o->p);
		/* when there's a level there should be a slot */
		prev = lev->next->ord;
		break;
	case OSIDE_SELL:
		lev = ask_side_level(q, io->o->p);
		prev = lev->next->ord;
		break;
	case OSIDE_UNK:
	case NOSIDES:
	default:
		return -1;
	}

	assert(lev != NULL);
	/* fiddle with prev, insert o after prev */
	io->next = prev->next, prev->next = io;
	lev->l->q += io->o->q;
	io->lev = lev;
	/* update the level pointer */
	lev->next->ord = io;
	return 0;
}

static int
add_match(umoq_t q, umoq_m_t im)
{
	/* insert into our list */
	im->next = q->ms->next, q->ms->next = im;
	im->next->prev = im;
	im->prev = q->ms;
	return 0;
}

static umoq_m_t
add_match_immediate_pfill(umoq_t q, umoq_o_t qo, umo_t new_o)
{
/* assume the counterparty has the same quantity and no order id yet */
	umoq_m_t im = pop_m(q);

	/* sort of inverts the meaning */
	switch (um_order_side(qo->o)) {
	case OSIDE_BUY:
		im->m->ob = qo->oid;
		im->m->os = ++q->oid;
		/* agent tracking */
		im->m->ab = qo->o->agent_id;
		im->m->as = new_o->agent_id;
		break;
	case OSIDE_SELL:
		im->m->ob = ++q->oid;
		im->m->os = qo->oid;
		/* agent tracking */
		im->m->ab = new_o->agent_id;
		im->m->as = qo->o->agent_id;
		break;
	case OSIDE_UNK:
	case NOSIDES:
	default:
		abort();
	}
	/* make sure we keep track of the instruments traded */
	im->m->ib = q->secid;
	im->m->is = q->funid;
	/* stipulate the price and quantity here */
	im->m->p = qo->o->p;
	im->m->q = new_o->q;
	/* insert into our list */
	add_match(q, im);
	return im;
}

static umoq_m_t
add_match_immediate(umoq_t q, umoq_o_t o, umo_t new_o)
{
/* assume the counterparty has the same quantity and no order id yet */
	return add_match_immediate_pfill(q, o, new_o);
}

static inline int
check_limit(umo_t o, umo_t lim_o)
{
/* return 0 if order is within the limit */
	switch (um_order_type(lim_o)) {
	case OTYPE_MKT:
		return 0;
	case OTYPE_MTL:
	case OTYPE_LIM:
		switch (um_order_side(lim_o)) {
		case OSIDE_BUY:
			return ffff_m30_cmp(o->p, lim_o->p) > 0;
		case OSIDE_SELL:
			return ffff_m30_cmp(o->p, lim_o->p) < 0;
		case OSIDE_UNK:
		case NOSIDES:
		default:
			abort();
		}
		return -1;
	case OTYPE_UNK:
	case NOTYPES:
	default:
		abort();
	}
	return -1;
}

static umoq_o_t
match_order(umoq_t q, umo_t o)
{
	umoq_o_t sta;

	/* find the match side and better_than parameter */
	switch (um_order_side(o)) {
	case OSIDE_BUY:
		/* point to all asks */
		sta = q->oa;
		break;
	case OSIDE_SELL:
		/* point to all bids */
		sta = q->ob;
		break;
	case OSIDE_UNK:
	case NOSIDES:
	default:
		abort();
	}

	/* if it's a market-to-limit order, get the current market price */
	if (um_order_type(o) == OTYPE_MTL) {
		/* set the top level ask as limit */
		o->p = sta->next->o->p;
	}
	/* check if top bid level is filled */
	while (sta->next && check_limit(sta->next->o, o) == 0 &&
	       o->q >= sta->next->o->q) {
		umoq_o_t fro = sta->next;

		/* retune the quantity cell */
		o->q -= fro->o->q;

		/* adapt o's level as well */
		rem_order_from_level(q, fro);

		/* record the matches */
		add_match_immediate(q, fro, o);

		/* we can make a match, but we'd have to fiddle
		 * with the queue */
		sta->next = sta->next->next;

		/* what happens to all the freed cells here? :O */
		push_o(q, fro);
	}
	if (UNLIKELY(sta->next == NULL && um_order_type(o) == OTYPE_MKT)) {
		/* we've got a market order with more size than the
		 * market has depth, what are we gonna do now? */
		return NULL;

	} else if (sta->next && check_limit(sta->next->o, o) == 0) {
		assert(sta->next->o->q > o->q);

		/* bingo, we can make a match on the fly */
		sta->next->o->q -= o->q;

		/* record the matches */
		add_match_immediate_pfill(q, sta->next, o);

		assert(sta->lev->next->l->q > o->q);
		/* adapt the levels as well */
		sta->lev->next->l->q -= o->q;
		/* total fill */
		o->q = 0;
		return NULL;
	}

	/* put the rest on the queue as limit */
	sta = pop_o(q);
	/* copy the order guts */
	*sta->o = *o;
	return sta;
}

static umoq_o_t
try_match(umoq_t q, umo_t o)
{
	umoq_o_t res = match_order(q, o);
	/* if there's a callback for matches, serve it */
	if (q->match_cb != NULL) {
		for (umoq_m_t m = (q->ms + 1)->prev; m != q->ms; m = m->prev) {
			q->match_cb(m->m, q->match_clo);
			/* unlink the match, DO ME PROPERLY! */
			m->prev->next = q->ms + 1;
		}
	}
	return res;
}


/* ctor/dtor */
umoq_t
make_oq(insid_t secu_id, insid_t fund_id)
{
	umoq_t res = xnew(*res);

	/* wipe the order space */
	memset(res->ob, 0, sizeof(*res->ob));
	memset(res->oa, 0, sizeof(*res->oa));
	/* wipe the level space */
	memset(res->lb, 0, 2 * sizeof(*res->lb));
	memset(res->la, 0, 2 * sizeof(*res->la));
	/* now a more complex task */
	res->ols = make_mmls(sizeof(struct umoq_o_s), INITIAL_NUMOQ);
	res->lls = make_mmls(sizeof(struct umoq_o_s), INITIAL_NUMOQ);

	/* set initial level pointers */
	res->oa->lev = res->la;
	res->ob->lev = res->lb;
	/* and the other way around */
	res->la->ord = res->oa;
	res->lb->ord = res->ob;
	res->la->next = res->la + 1;
	res->lb->next = res->lb + 1;
	res->la->next->ord = res->oa;
	res->lb->next->ord = res->ob;

	/* start with a fresh oid count */
	res->oid = 0;

	/* initialise the matching cache */
	res->mls = make_mmls(sizeof(struct umoq_m_s), INITIAL_NUMOQ);
	memset(res->ms, 0, 2 * sizeof(*res->ms));
	res->ms[0].next = res->ms + 1;
	res->ms[1].prev = res->ms + 0;

	/* finally keep track of what we are */
	res->secid = secu_id;
	res->funid = fund_id;

	/* set match_cb and match_clo */
	res->match_cb = NULL;
	res->match_clo = NULL;
	return res;
}

void
free_oq(umoq_t q)
{
	free_mmls(q->ols);
	free_mmls(q->lls);
	free_mmls(q->mls);
	xfree(q);
	return;
}


/* order queue operations */
oid_t
oq_add_order(umoq_t q, umo_t o)
{
	/* check if the order O would cause a match
	 * if so, make the maximum match, then process the rest
	 * if not, or for the rest of the matched order, queue it */
	umoq_o_t io = try_match(q, o);

	/* insert the order, only if it's not completely matched */
	if (UNLIKELY(io == NULL || add_order(q, io) < 0)) {
		return 0;
	}
	/* flag as new */
	io->st = OSTATUS_NEW;
	return io->oid = ++q->oid;
}

struct umo_s
oq_get_order(umoq_t q, oid_t oid)
{
/* not sure about the signature here,
 * we don't want to hand out pointers to our internal structs, on the other
 * hand just malloc()ing something seems a bit of an overkill */
	umoq_o_t io;
	if (UNLIKELY((io = find_by_oid(q, oid)) == NULL)) {
		return (struct umo_s){0};
	}
	return *io->o;
}

int
oq_cancel_order(umoq_t q, oid_t oid)
{
	umoq_o_t io;

	if (UNLIKELY((io = rem_by_oid(q, oid)) == NULL)) {
		return -1;
	}
	/* flag as cancelled */
	io->st = OSTATUS_CANC;

	/* return the cell to our free list */
	push_o(q, io);

	return 0;
}

umost_t
oq_get_status(umoq_t q, oid_t oid)
{
	umoq_o_t io;
	if (UNLIKELY((io = find_by_oid(q, oid)) == NULL)) {
		return OSTATUS_UNK;
	}
	return io->st;
}

int
oq_suspend_order(umoq_t q, oid_t oid)
{
	umoq_o_t io;
	if (UNLIKELY((io = rem_by_oid(q, oid)) == NULL)) {
		return -1;
	}
	/* mark order as suspended */
	io->st = OSTATUS_SUSP;

	/* prepend the order to the suspension list */
	io->next = q->os->next, q->os->next = io;
	return 0;
}

int
oq_resume_order(umoq_t q, oid_t oid)
{
	umoq_o_t io, mio;

	if ((io = find_by_side_by_oid(susp_head(q), oid)) == NULL) {
		return -1;
	} else if ((mio = try_match(q, io->o)) == NULL) {
		/* nothing left, order must have matched */
		push_o(q, io);
		return -1;
	}
	/* otherwise put the order back on track */
	add_order(q, mio);
	/* free the old bugger */
	push_o(q, io);
	return 0;
}


/* traversal thingamabobs */
int
oq_trav_bids(umoq_t q, void(*cb)(uml_t, void*), void *closure)
{
	int res = 0;
	for (umoq_l_t l = q->lb->next; l != q->lb + 1; l = l->next, res++) {
		cb(l->l, closure);
	}
	return res;
}

int
oq_trav_asks(umoq_t q, void(*cb)(uml_t, void*), void *closure)
{
	int res = 0;
	for (umoq_l_t l = q->la->next; l != q->la + 1; l = l->next, res++) {
		cb(l->l, closure);
	}
	return res;
}

int
oq_trav_matches(umoq_t q, void(*cb)(umm_t, void*), void *closure)
{
	int res = 0;
	for (umoq_m_t m = (q->ms + 1)->prev; m != q->ms; m = m->prev, res++) {
		cb(m->m, closure);
	}
	return res;
}

/* like oq_trav_matches() but the most recent ones first. */
int
oq_trav_matches_rev(umoq_t q, void(*cb)(umm_t, void*), void *closure)
{
	int res = 0;
	for (umoq_m_t m = q->ms->next; m != q->ms + 1; m = m->next, res++) {
		cb(m->m, closure);
	}
	return res;
}

int
oq_clear_matches(umoq_t q)
{
/* return the number of matches cleared */
	int res = 0;
	for (umoq_m_t m = q->ms->next; m != q->ms + 1; res++) {
		q->ms->next = m->next;
		push_m(q, m);
		m = q->ms->next;
	}
	/* make sure the back ptrs are in place */
	(q->ms + 1)->prev = q->ms;
	return res;
}

void
oq_register_match_cb(umoq_t q, void(*cb)(umm_t, void*), void *clo)
{
	q->match_cb = cb;
	q->match_clo = clo;
	return;
}


#if defined STANDALONE
/* for debugging output */
#include <stdio.h>

static void
prnt_orders(umoq_t q)
{
	printf("\nbids o\n");
	for (umoq_o_t o = q->ob->next; o; o = o->next) {
		printf("%2.4f %u\n", ffff_m30_d(o->o->p), o->o->q);
	}
	printf("\nasks o\n");
	for (umoq_o_t o = q->oa->next; o; o = o->next) {
		printf("%2.4f %u\n", ffff_m30_d(o->o->p), o->o->q);
	}
	return;
}

static void
prnt_levels(umoq_t q)
{
	printf("\nbids l\n");
	for (umoq_l_t l = q->lb->next; l != q->lb + 1; l = l->next) {
		printf("%2.4f %u\n", ffff_m30_d(l->l->p), l->l->q);
	}
	printf("\nasks l\n");
	for (umoq_l_t l = q->la->next; l != q->la + 1; l = l->next) {
		printf("%2.4f %u\n", ffff_m30_d(l->l->p), l->l->q);
	}
	return;
}

static void
prnt_matches(umoq_t q)
{
	printf("\nmatches\n");
	for (umoq_m_t m = q->ms->next; m; m = m->next) {
		printf("%u %u: %u @ %2.4f\n",
		       m->m->ob, m->m->os,
		       m->m->q, ffff_m30_d(m->m->p));
	}
	return;
}

static oid_t
bnlot(umoq_t q, double p, size_t qty)
{
	struct umo_s o[1];

	/* add a test order */
	o->agent_id = 1;
	o->instr_id = 1;
	o->p = ffff_m30_get_d(p);
	o->q = qty;
	o->side = OSIDE_BUY;
	o->type = OTYPE_LIM;
	o->tymod = OTYMOD_GTC;
	return oq_add_order(q, o);
}

static oid_t
snlot(umoq_t q, double p, size_t qty)
{
	struct umo_s o[1];

	/* add a test order */
	o->agent_id = 1;
	o->instr_id = 1;
	o->p = ffff_m30_get_d(p);
	o->q = qty;
	o->side = OSIDE_SELL;
	o->type = OTYPE_LIM;
	o->tymod = OTYMOD_GTC;
	return oq_add_order(q, o);
}

static oid_t
b1lot(umoq_t q, double p)
{
	return bnlot(q, p, 1000);
}

static oid_t
s1lot(umoq_t q, double p)
{
	return snlot(q, p, 1000);
}

static oid_t
s1mkt(umoq_t q)
{
	struct umo_s o[1];

	/* add a test order */
	o->agent_id = 1;
	o->instr_id = 1;
	o->q = 2002;
	o->side = OSIDE_SELL;
	o->type = OTYPE_MKT;
	o->tymod = OTYMOD_GTC;
	return oq_add_order(q, o);
}

static oid_t __attribute__((unused))
smtl(umoq_t q, size_t qty)
{
	struct umo_s o[1];

	/* add a test order */
	o->agent_id = 1;
	o->instr_id = 1;
	o->q = qty;
	o->side = OSIDE_SELL;
	o->type = OTYPE_MTL;
	o->tymod = OTYMOD_GTC;

	fprintf(stdout, "smtl\n");
	return oq_add_order(q, o);
}

static oid_t
bmtl(umoq_t q, size_t qty)
{
	struct umo_s o[1];

	/* add a test order */
	o->agent_id = 1;
	o->instr_id = 1;
	o->q = qty;
	o->side = OSIDE_BUY;
	o->type = OTYPE_MTL;
	o->tymod = OTYMOD_GTC;

	fprintf(stdout, "bmtl\n");
	return oq_add_order(q, o);
}

int
main(int argc, char *argv[])
{
	umoq_t s1;
	oid_t o1, o2;

	/* initialise an order queue for security 1 */
	s1 = make_oq(2, 1);

	b1lot(s1, 12.10);
	b1lot(s1, 12.04);
	b1lot(s1, 12.09);
	o1 = b1lot(s1, 12.04);
	s1lot(s1, 12.40);
	s1lot(s1, 12.38);
	b1lot(s1, 12.13);
	s1lot(s1, 12.48);
	s1lot(s1, 12.44);
	s1lot(s1, 12.44);
	o2 = s1lot(s1, 12.22);
	prnt_orders(s1);
	prnt_levels(s1);

	s1mkt(s1);
	oq_cancel_order(s1, o1);
	oq_suspend_order(s1, o2);
	prnt_orders(s1);
	prnt_levels(s1);

	bnlot(s1, 12.24, 1002);
	prnt_orders(s1);
	prnt_levels(s1);

	bmtl(s1, 1024);
	prnt_orders(s1);
	prnt_levels(s1);

	oq_resume_order(s1, o1);
	oq_resume_order(s1, o2);
	prnt_orders(s1);
	prnt_levels(s1);

	prnt_matches(s1);

	/* and out again */
	free_oq(s1);
	return 0;
}
#endif	/* STANDALONE */

/* oq.c ends here */
