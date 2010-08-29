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
# include "mmls.c"

#define USE_SQLITE	(1)
#define INITIAL_NAGT	(256)
#define INITIAL_NINS	(256)

#if defined USE_SQLITE
# include <sqlite3.h>
# include <stdio.h>
#endif	/* USE_SQLITE */

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
#if defined USE_SQLITE
	sqlite3 *db;
	/* prepared statements */
	sqlite3_stmt *agetter;
	sqlite3_stmt *igetter;
	/* match inserter */
	sqlite3_stmt *minster;
	/* inventory loader and inserter */
	sqlite3_stmt *linvent;
	sqlite3_stmt *iinvent;
#else  /* !USE_SQLITE */
	mmls_t als;
	mmls_t ils;
	struct uschi_a_s a[1];
	struct uschi_i_s i[1];

	/* instr and agent id counters, should be global */
	uint32_t aid;
	uint32_t iid;
	uint32_t mid;
#endif	/* USE_SQLITE */
};


#if !defined USE_SQLITE
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

static uschi_i_t
find_instr_by_id(uschi_t h, insid_t id)
{
	for (uschi_i_t i = h->i->next; i; i = i->next) {
		if (i->id == id) {
			return i;
		}
	}
	return NULL;
}

static ins_t
uschi_get_instr(uschi_t h, insid_t id)
{
	uschi_i_t ii;
	if (UNLIKELY((ii = find_instr_by_id(h, id)) == NULL)) {
		return NULL;
	}
	return ii->i;
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

static agt_inv_t
make_inv_for_agent(agt_t a, insid_t id)
{
	agt_inv_t new = mmls_pop_cell(a->ils);
	memset(new->i, 0, sizeof(*new->i));
	new->id = id;
	/* append to the list */
	new->next = a->i->next, a->i->next = new;
	return new;
}

static inv_t __attribute__((noinline))
uschi_agent_get_inv(uschi_t h, agt_t a, insid_t id)
{
	agt_inv_t ii;
	if (UNLIKELY((ii = find_inv_by_id(a, id)) == NULL)) {
		/* create an a/c */
		uschi_i_t ui = find_instr_by_id(h, id);
		ii = make_inv_for_agent(a, id);
		ii->ins = ui;
	}
	return ii->i;
}
#endif	/* !USE_SQLITE */


/* ctor/dtor */
uschi_t
make_uschi(void)
{
	uschi_t res = xnew(*res);

#if defined USE_SQLITE
	static const char dbpath[] =
		"/home/freundt/devel/unsermarkt/=build/uschi.sqlt3";
	static const char aget[] =
		"SELECT agent_id FROM agent WHERE nick = ?;";
	static const char iget[] =
		"SELECT instr_id FROM instr WHERE name = ?;";
	static const char mins[] =
		"INSERT INTO match ("
		"b_agent_id, s_agent_id, b_instr_id, s_instr_id, "
		"price, quantity) VALUES "
		"(?, ?, ?, ?, ?, ?);";
	static const char linv[] =
		"SELECT lpos, spos FROM agtinv "
		"WHERE agent_id = ? AND instr_id = ?;";
	static const char iinv[] =
		"INSERT OR REPLACE INTO agtinv "
		"(agent_id, instr_id, lpos, spos) "
		"VALUES (?, ?, ?, ?);";

	sqlite3_open(dbpath, &res->db);

	/* prepare some statements */
	sqlite3_prepare_v2(res->db, aget, sizeof(aget), &res->agetter, NULL);
	sqlite3_prepare_v2(res->db, iget, sizeof(iget), &res->igetter, NULL);
	sqlite3_prepare_v2(res->db, mins, sizeof(mins), &res->minster, NULL);
	sqlite3_prepare_v2(res->db, linv, sizeof(linv), &res->linvent, NULL);
	sqlite3_prepare_v2(res->db, iinv, sizeof(iinv), &res->iinvent, NULL);

#else  /* !USE_SQLITE */
	memset(res->a, 0, sizeof(*res->a));
	memset(res->i, 0, sizeof(*res->i));
	/* create an initial list of agents and instruments */
	res->als = make_mmls(sizeof(struct uschi_a_s), INITIAL_NAGT);
	res->ils = make_mmls(sizeof(struct uschi_i_s), INITIAL_NINS);
#endif	/* USE_SQLITE */
	return res;
}

void
free_uschi(uschi_t h)
{
#if defined USE_SQLITE
	sqlite3_finalize(h->agetter);
	sqlite3_finalize(h->igetter);
	sqlite3_finalize(h->minster);
	sqlite3_finalize(h->linvent);
	sqlite3_finalize(h->iinvent);

	sqlite3_close(h->db);
#else  /* !USE_SQLITE */
	free_mmls(h->als);
	free_mmls(h->ils);
#endif	/* USE_SQLITE */
	xfree(h);
	return;
}


/* agent opers */
agtid_t
uschi_add_agent(uschi_t h, char *nick)
{
/* new agent gets 1 million UMDs */
#if defined USE_SQLITE
	char qry[512];

	snprintf(qry, sizeof(qry),
		 "INSERT INTO 'agent' (nick) VALUES ('%s');", nick);
	if (sqlite3_exec(h->db, qry, NULL, 0, NULL) != SQLITE_OK) {
		fputs("qry failed\n", stderr);
	}
	return sqlite3_last_insert_rowid(h->db);
#else  /* !USE_SQLITE */
	uschi_a_t a = pop_a(h);

	/* create the investment list */
	memset(a, 0, sizeof(*a));
	a->a->ils = make_mmls(sizeof(struct uschi_i_s), INITIAL_NINS);
	/* store the nick name */
	a->a->nick = strdup(nick);
	/* append to our agent list */
	a->next = h->a->next, h->a->next = a;
	return a->id = ++h->aid;
#endif	/* USE_SQLITE */
}

agtid_t
uschi_get_agent(uschi_t h, char *nick)
{
	agtid_t res = 0;
#if defined USE_SQLITE
	size_t len = strlen(nick);
	sqlite3_bind_text(h->agetter, 1, nick, len, SQLITE_STATIC);
	if (sqlite3_step(h->agetter) == SQLITE_ROW) {
		res = sqlite3_column_int(h->agetter, 0);
	}
	sqlite3_reset(h->agetter);
#else
# error implement me, uschi_get_agent()
#endif	/* USE_SQLITE */
	return res;
}

insid_t
uschi_get_instr(uschi_t h, char *name)
{
	insid_t res = 0;
#if defined USE_SQLITE
	size_t len = strlen(name);
	sqlite3_bind_text(h->igetter, 1, name, len, SQLITE_STATIC);
	if (sqlite3_step(h->igetter) == SQLITE_ROW) {
		res = sqlite3_column_int(h->igetter, 0);
	}
	sqlite3_reset(h->igetter);
#else
# error implement me, uschi_get_agent()
#endif	/* USE_SQLITE */
	return res;
}


/* instr opers */
insid_t
uschi_add_instr(uschi_t h, char *name)
{
/* new agent gets 1 million UMDs */
#if defined USE_SQLITE
	char qry[512];

	snprintf(qry, sizeof(qry),
		 "INSERT INTO 'instr' (name) VALUES ('%s');", name);
	if (sqlite3_exec(h->db, qry, NULL, 0, NULL) != SQLITE_OK) {
		fputs("qry failed\n", stderr);
	}
	return sqlite3_last_insert_rowid(h->db);
#else  /* !USE_SQLITE */
	uschi_i_t i = pop_i(h);

	/* memorise the name, it's unused though */
	i->i->name = strdup(name);
	/* append to our instr list */
	i->next = h->i->next, h->i->next = i;
	return i->id = ++h->iid;
#endif	/* USE_SQLITE */
}


/* matching queue operations */
static struct inv_s
load_inventory(uschi_t h, agtid_t a, insid_t i)
{
	struct inv_s res[1] = {0};

	sqlite3_bind_int(h->linvent, 1, a);
	sqlite3_bind_int(h->linvent, 2, i);
	switch (sqlite3_step(h->linvent)) {
	case SQLITE_ROW:
		res->lpos.mant = sqlite3_column_int64(h->linvent, 0);
		res->lpos.expo = 1;
		res->spos.mant = sqlite3_column_int64(h->linvent, 1);
		res->spos.expo = 1;
		break;
	case SQLITE_MISUSE:
		abort();
	default:
		break;
	}
	sqlite3_reset(h->linvent);
	return *res;
}

static void
push_inventory(uschi_t h, agtid_t a, insid_t i, inv_t pf)
{
	sqlite3_bind_int(h->iinvent, 1, a);
	sqlite3_bind_int(h->iinvent, 2, i);
	sqlite3_bind_int64(h->iinvent, 3, pf->lpos.mant);
	sqlite3_bind_int64(h->iinvent, 4, pf->spos.mant);
	sqlite3_step(h->iinvent);
	sqlite3_reset(h->iinvent);
	return;
}

mid_t
uschi_add_match(uschi_t h, umm_t m)
{
/* we process buyer and seller separately */
#if defined USE_SQLITE
	mid_t res = 0;
	struct inv_s bs[1], bf[1], ss[1], sf[1];

	sqlite3_bind_int(h->minster, 1, m->ab);
	sqlite3_bind_int(h->minster, 2, m->as);
	sqlite3_bind_int(h->minster, 3, m->ib);
	sqlite3_bind_int(h->minster, 4, m->is);
	sqlite3_bind_int(h->minster, 5, m->p.mant);
	sqlite3_bind_int(h->minster, 6, m->q * 10000);
	if (sqlite3_step(h->minster) == SQLITE_DONE) {
		res = sqlite3_last_insert_rowid(h->db);
	}
	sqlite3_reset(h->minster);

	/* update the agents' portfolios */
	*bs = load_inventory(h, m->ab, m->ib);
	*bf = load_inventory(h, m->ab, m->is);
	*ss = load_inventory(h, m->as, m->ib);
	*sf = load_inventory(h, m->as, m->is);

	/* the buyer inv is the security, the seller inv the funding side */
	bs->lpos = ffff_m62_add_ui64(bs->lpos, m->q);
	ss->spos = ffff_m62_add_ui64(ss->spos, m->q);
	bf->spos = ffff_m62_add_mul_m30_ui64(bf->spos, m->p, m->q);
	sf->lpos = ffff_m62_add_mul_m30_ui64(sf->lpos, m->p, m->q);

	/* and update the goodies */
	push_inventory(h, m->ab, m->ib, bs);
	push_inventory(h, m->ab, m->is, bf);
	push_inventory(h, m->as, m->ib, ss);
	push_inventory(h, m->as, m->is, sf);
	return res;
#else  /* !USE_SQLITE */
	agt_t ab = uschi_get_agent(h, m->ab);
	inv_t ibs = uschi_agent_get_inv(h, ab, m->ib);
	inv_t ibf = uschi_agent_get_inv(h, ab, m->is);
	/* find the securities in question */
	agt_t as = uschi_get_agent(h, m->as);
	inv_t iss = uschi_agent_get_inv(h, as, m->ib);
	inv_t isf = uschi_agent_get_inv(h, as, m->is);

	/* the buyer inv  is the security, the seller inv the funding side */
	ibs->lpos = ffff_m62_add_ui64(ibs->lpos, m->q);
	iss->spos = ffff_m62_add_ui64(iss->spos, m->q);
	ibf->spos = ffff_m62_add_mul_m30_ui64(ibf->spos, m->p, m->q);
	isf->lpos = ffff_m62_add_mul_m30_ui64(isf->lpos, m->p, m->q);
	return ++h->mid;
#endif	/* USE_SQLITE */
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
	struct umm_s m[1];
	mid_t mid;

	h = make_uschi();

	/* add dummy instruments */
	s1 = uschi_get_instr(h, "UMD");
	s2 = uschi_get_instr(h, "Example Security");
	/* add two dummy agents */
	a1 = uschi_get_agent(h, "S2 MktMkr");
	a2 = uschi_get_agent(h, "Dumb Idiot");

	m->ab = a1, m->as = a2;
	m->ib = s1, m->is = s2;
	m->p = ffff_m30_get_d(12.50);
	m->q = 1200;
	mid = uschi_add_match(h, m);

	fprintf(stderr, "%u\n", mid);

	free_uschi(h);
	return 0;
}
#endif	/* STANDALONE */

/* uschi.c ends here */
