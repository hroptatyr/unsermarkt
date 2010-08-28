/*** uschi.c -- unsermarkt settlement and clearing house */

#include <stdlib.h>
#include <string.h>
/* rudi's favourite */
#include <assert.h>

/* for orders */
#include "order.h"
/* for matches */
#include "match.h"
/* our own stuff */
#include "uschi.h"
/* more precision for the big portfolios */
#include "m62.h"

/* very abstract list provider */
#include "mmls.c"

#define INITIAL_NAGT	(256)
#define INITIAL_NINS	(256)

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

/* agent structure within uschi */
typedef struct agt_s *agt_t;
/* instr structure within uschi */
typedef struct ins_s *ins_t;

/* local helpers */
typedef struct uschi_i_s *uschi_i_t;
typedef struct uschi_a_s *uschi_a_t;
typedef struct inv_s *inv_t;

/* instruments */
struct ins_s {
	char *name;
};

/* like ins_s but with nav pointers */
struct uschi_i_s {
	uschi_i_t next;
	struct ins_s i[1];
	insid_t id;
};

/* agents */
/* investments, resembles pfack investments, innit? */
struct inv_s {
	m62_t lpos;
	m62_t spos;
};

typedef struct agt_inv_s *agt_inv_t;

struct agt_inv_s {
	agt_inv_t next;
	struct inv_s i[1];
	insid_t id;
	uschi_i_t ins;
};

struct agt_s {
	/* the agent's investments */
	mmls_t ils;
	struct agt_inv_s i[1];
	char *nick;
};

/* like agt_s but with agent id and nav pointers */
struct uschi_a_s {
	uschi_a_t next;
	struct agt_s a[1];
	agtid_t id;
};

/**
 * The main uschi structure, it's just a set of agents atm. */
struct uschi_s {
	mmls_t als;
	mmls_t ils;
	struct uschi_a_s a[1];
	struct uschi_i_s i[1];

	/* instr and agent id counters, should be global */
	uint32_t aid;
	uint32_t iid;
};


static uschi_a_t
pop_a(uschi_t h)
{
	uschi_a_t res = mmls_pop_cell(h->als);
	return res;
}

static void
push_a(uschi_t h, uschi_a_t a)
{
	a->next = (void*)0xdeadbeef;
	mmls_push_cell(h->als, a);
	return;
}

static uschi_i_t
pop_i(uschi_t h)
{
	uschi_i_t res = mmls_pop_cell(h->ils);
	return res;
}

static void
push_i(uschi_t h, uschi_i_t i)
{
	i->next = (void*)0xdeadbeef;
	mmls_push_cell(h->ils, i);
	return;
}

static uschi_a_t
find_agent_by_id(uschi_t h, agtid_t id)
{
	for (uschi_a_t a = h->a->next; a; a = a->next) {
		if (a->id == id) {
			return a;
		}
	}
	return NULL;
}

static agt_t
uschi_get_agent(uschi_t h, agtid_t id)
{
	uschi_a_t ia;
	if (UNLIKELY((ia = find_agent_by_id(h, id)) == NULL)) {
		return NULL;
	}
	return ia->a;
}

static agt_inv_t
find_inv_by_id(agt_t a, agtid_t id)
{
	for (agt_inv_t i = a->i->next; i; i = i->next) {
		if (i->id == id) {
			return i;
		}
	}
	return NULL;
}

static inv_t
uschi_agent_get_inv(uschi_t UNUSED(h), agt_t a, insid_t id)
{
	agt_inv_t ii;
	if (UNLIKELY((ii = find_inv_by_id(a, id)) == NULL)) {
		return NULL;
	}
	return ii->i;
}


/* ctor/dtor */
uschi_t
make_uschi(void)
{
	uschi_t res = xnew(*res);

	memset(res->a, 0, sizeof(*res->a));
	memset(res->i, 0, sizeof(*res->i));
	/* create an initial list of agents and instruments */
	res->als = make_mmls(sizeof(struct uschi_a_s), INITIAL_NAGT);
	res->ils = make_mmls(sizeof(struct uschi_i_s), INITIAL_NINS);
	return res;
}

void
free_uschi(uschi_t h)
{
	free_mmls(h->als);
	free_mmls(h->ils);
	xfree(h);
	return;
}


/* agent opers */
agtid_t
uschi_add_agent(uschi_t h, char *nick)
{
/* new agent gets 1 million UMDs */
	uschi_a_t a = pop_a(h);

	memset(a, 0, sizeof(*a));
	a->a->ils = make_mmls(sizeof(struct uschi_i_s), INITIAL_NINS);

	/* store the nick name */
	a->a->nick = strdup(nick);
	return 0;
}


/* instr opers */
insid_t
uschi_add_instr(uschi_t h, char *name)
{
/* new agent gets 1 million UMDs */
	uschi_i_t i = pop_i(h);
	i->i->name = strdup(name);
	return i->id = ++h->iid;
}


/* matching queue operations */
mid_t
uschi_add_match(uschi_t h, umm_t m)
{
/* we process buyer and seller separately */
	agt_t ab = uschi_get_agent(h, m->ab);
	inv_t ib = uschi_agent_get_inv(h, ab, m->ib);
	/* find the securities in question */
	agt_t as = uschi_get_agent(h, m->as);
	inv_t is = uschi_agent_get_inv(h, ab, m->is);
	return 0;
}

#if defined STANDALONE
/* for debugging output */
#include <stdio.h>

int
main(int argc, char *argv[])
{
	uschi_t h;
	agtid_t a1, a2;
	insid_t s1, s2;

	h = make_uschi();

	/* add dummy instruments */
	s1 = uschi_add_instr(h, "UMD");
	s2 = uschi_add_instr(h, "Example Security");
	/* add two dummy agents */
	a1 = uschi_add_agent(h, "S2 MktMkr");
	a2 = uschi_add_agent(h, "Dumb Idiot");

	free_uschi(h);
	return 0;
}
#endif	/* STANDALONE */

/* uschi.c ends here */
