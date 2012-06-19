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
#include <uterus/m62.h>
/* very abstract list provider */
#include "mmls.c"

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

/* local helpers */
typedef struct uschi_i_s *uschi_i_t;
typedef struct uschi_a_s *uschi_a_t;
typedef struct inv_s *inv_t;

/* investments, resembles pfack investments, innit? */
struct inv_s {
	m62_t lpos;
	m62_t spos;
};

/* instruments, like ins_s but with nav pointers */
struct uschi_i_s {
	uschi_i_t next;
	struct ins_s i[1];
	insid_t id;
};

#if !defined USE_SQLITE
/* agents */
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
#endif	/* !USE_SQLITE */

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

	/* used as caches */
	mmls_t ils;
	struct uschi_i_s i[1];

#else  /* !USE_SQLITE */
	mmls_t als;
	struct uschi_a_s a[1];
	struct uschi_i_s i[1];

	/* instr and agent id counters, should be global */
	uint32_t aid;
	uint32_t iid;
	uint32_t mid;
#endif	/* USE_SQLITE */
};


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

static uschi_i_t
find_instr_by_sym(uschi_t h, char *sym)
{
	for (uschi_i_t i = h->i->next; i; i = i->next) {
		if (strcmp(i->i->sym, sym) == 0) {
			return i;
		}
	}
	return NULL;
}

static uschi_i_t
pop_i(uschi_t h)
{
	uschi_i_t res = mmls_pop_cell(h->ils);
	return res;
}

static void __attribute__((unused))
push_i(uschi_t h, uschi_i_t i)
{
	i->next = (void*)0xdeadbeef;
	mmls_push_cell(h->ils, i);
	return;
}

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
uschi_get_agent_agt(uschi_t h, agtid_t id)
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

#if defined USE_SQLITE
static void
tune_sqlite_backend(uschi_t h)
{
	/* turn off synchronous mode */
	sqlite3_exec(h->db, "PRAGMA synchronous=0;", NULL, 0, NULL);
	/* insist on foreign keys */
	sqlite3_exec(h->db, "PRAGMA foreign_keys=1;", NULL, 0, NULL);
	/* turn off transactions */
	sqlite3_exec(h->db, "PRAGMA journal_mode=MEMORY;", NULL, 0, NULL);
	return;
}
#endif	/* USE_SQLITE */

static uschi_i_t
add_instr(uschi_t h, insid_t id, const char *sym, const char *descr)
{
	uschi_i_t i = pop_i(h);

	i->i->sym = strdup(sym);
	i->i->descr = strdup(descr);
	/* append to our instr list */
	i->next = h->i->next, h->i->next = i;
	i->id = id;
	return i;
}


/* ctor/dtor */
uschi_t
make_uschi(const char *dbpath)
{
	uschi_t res = xnew(*res);

#if defined USE_SQLITE
	static const char aget[] =
		"SELECT agent_id, nick FROM agent WHERE nick = ?;";
	static const char iget[] =
		"SELECT instr_id, sym, descr FROM instr WHERE sym = ?;";
	static const char mins[] =
		"INSERT INTO match ("
		"b_agent_id, s_agent_id, b_instr_id, s_instr_id, "
		"price, quantity, tsec, tusec) VALUES "
		"(?, ?, ?, ?, ?, ?, ?, ?);";
	static const char linv[] =
		"SELECT lpos, spos FROM agtinv "
		"WHERE agent_id = ? AND instr_id = ?;";
	static const char iinv[] =
		"INSERT OR REPLACE INTO agtinv "
		"(agent_id, instr_id, lpos, spos) "
		"VALUES (?, ?, ?, ?);";

	sqlite3_open(dbpath, &res->db);
	/* turn off synchronous mode, etc. */
	tune_sqlite_backend(res);
	/* prepare some statements */
	sqlite3_prepare_v2(res->db, aget, sizeof(aget), &res->agetter, NULL);
	sqlite3_prepare_v2(res->db, iget, sizeof(iget), &res->igetter, NULL);
	sqlite3_prepare_v2(res->db, mins, sizeof(mins), &res->minster, NULL);
	sqlite3_prepare_v2(res->db, linv, sizeof(linv), &res->linvent, NULL);
	sqlite3_prepare_v2(res->db, iinv, sizeof(iinv), &res->iinvent, NULL);

#else  /* !USE_SQLITE */
	memset(res->a, 0, sizeof(*res->a));
	/* create an initial list of agents and instruments */
	res->als = make_mmls(sizeof(struct uschi_a_s), INITIAL_NAGT);
#endif	/* USE_SQLITE */
	/* create a maplist for agents */
	memset(res->i, 0, sizeof(*res->i));
	res->ils = make_mmls(sizeof(struct uschi_i_s), INITIAL_NINS);
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
#endif	/* USE_SQLITE */
	free_mmls(h->ils);
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
uschi_get_instr(uschi_t h, char *sym)
{
	uschi_i_t i;
	insid_t res = 0;

	/* try the cache before bothering the database */
	if (LIKELY((i = find_instr_by_sym(h, sym)) != NULL)) {
		/* bingo */
		return i->id;
	}
#if defined USE_SQLITE
	sqlite3_bind_text(h->igetter, 1, sym, -1, SQLITE_STATIC);
	if (sqlite3_step(h->igetter) == SQLITE_ROW) {
		const char *descr;
		res = sqlite3_column_int(h->igetter, 0);
		descr = (const char*)sqlite3_column_text(h->igetter, 2);
		/* place into cache */
		add_instr(h, res, sym, descr);
	}
	sqlite3_reset(h->igetter);
#else
	/* we're puzzled in this mode */
	;
#endif	/* USE_SQLITE */
	return res;
}

ins_t
uschi_get_instr_ins(uschi_t h, insid_t id)
{
	uschi_i_t ii;
	if (UNLIKELY((ii = find_instr_by_id(h, id)) == NULL)) {
#if defined USE_SQLITE
		/* actually the instr MUST be there, or we're fucked */
		static const char iget[] =
			"SELECT sym, descr FROM instr WHERE instr_id = ?;";
		sqlite3_stmt *stmt;

		sqlite3_prepare_v2(h->db, iget, sizeof(iget) - 1, &stmt, NULL);
		sqlite3_bind_int(stmt, 1, id);
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const char *sym =
				(const char*)sqlite3_column_text(stmt, 0);
			const char *descr =
				(const char*)sqlite3_column_text(stmt, 1);
			ii = pop_i(h);
			ii->id = id;
			ii->i->sym = strdup(sym);
			ii->i->descr = strdup(descr);
			/* append ii */
			ii->next = h->i->next, h->i->next = ii;
		}
		sqlite3_finalize(stmt);
#else  /* !USE_SQLITE */
		return NULL;
#endif	/* USE_SQLITE */
	}
	return ii->i;
}


/* instr opers */
insid_t
uschi_add_instr(uschi_t h, char *sym, char *descr)
{
/* new agent gets 1 million UMDs */
#if defined USE_SQLITE
	char qry[512];

	snprintf(qry, sizeof(qry),
		 "INSERT INTO 'instr' (sym, descr) VALUES ('%s', '%s');",
		 sym, descr);
	if (sqlite3_exec(h->db, qry, NULL, 0, NULL) != SQLITE_OK) {
		fputs("qry failed\n", stderr);
	}
	return sqlite3_last_insert_rowid(h->db);
#else  /* !USE_SQLITE */
	uschi_i_t i = add_instr(++h->iid, sym, descr);
	return i->id;
#endif	/* USE_SQLITE */
}

int
uschi_trav_instr(uschi_t h, void(*cb)(insid_t, ins_t, void*), void *clo)
{
	int res = 0;
#if USE_SQLITE
	static const char trav[] =
		"SELECT instr_id, sym, descr FROM instr;";
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(h->db, trav, sizeof(trav) - 1, &stmt, NULL);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		struct ins_s i[1];
		insid_t id = sqlite3_column_int(stmt, 0);
		i->sym = (const char*)sqlite3_column_text(stmt, 1);
		i->descr = (const char*)sqlite3_column_text(stmt, 2);
		cb(id, i, clo);
		res++;
	}
	sqlite3_finalize(stmt);
#else
# error implement me
#endif	/* !USE_SQLITE */
	return res;
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

#if defined DEBUG_FLAG
static void
pr_match(FILE *fp, umm_t m)
{
	fprintf(fp, "%u.%06u %u v %u %u<->%u\n",
		m->ts_sec, (uint32_t)m->ts_usec, m->ab, m->as, m->ib, m->is);
	return;
}
#else  /* !DEBUG_FLAG */
# define pr_match(args...)
#endif	/* DEBUG_FLAG */

mid_t
uschi_add_match(uschi_t h, umm_t m)
{
/* we process buyer and seller separately */
#if defined USE_SQLITE
	mid_t res = 0;
	struct inv_s bs[1], bf[1], ss[1], sf[1];
	int UNUSED(rc);

	sqlite3_bind_int(h->minster, 1, m->ab);
	sqlite3_bind_int(h->minster, 2, m->as);
	sqlite3_bind_int(h->minster, 3, m->ib);
	sqlite3_bind_int(h->minster, 4, m->is);
	sqlite3_bind_int(h->minster, 5, m->p.mant);
	sqlite3_bind_int(h->minster, 6, m->q * 10000);
	sqlite3_bind_int(h->minster, 7, m->ts_sec);
	sqlite3_bind_int(h->minster, 8, m->ts_usec);
	pr_match(stderr, m);
	if ((rc = sqlite3_step(h->minster)) == SQLITE_DONE) {
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
	static const char dbpath[] =
		"/home/freundt/devel/unsermarkt/=build/uschi.sqlt3";

	h = make_uschi(dbpath);

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
