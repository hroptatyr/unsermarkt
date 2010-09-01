
#if !defined INCLUDED_uschi_h_
#define INCLUDED_uschi_h_

#include "match.h"
#include "instr.h"

/* main uschi structure */
typedef struct uschi_s *uschi_t;


/* ctor/dtor */
extern uschi_t make_uschi(const char *dbpath);
extern void free_uschi(uschi_t);

extern insid_t uschi_add_instr(uschi_t, char *sym, char *descr);
extern agtid_t uschi_add_agent(uschi_t, char *nick);

extern insid_t uschi_get_instr(uschi_t, char *sym);
extern agtid_t uschi_get_agent(uschi_t, char *nick);

extern mid_t uschi_add_match(uschi_t, umm_t match);


/* still uschi's task? */
extern ins_t uschi_get_instr_ins(uschi_t, insid_t id);

/**
 * Traverse all currently registered instruments.
 * Callback is called with the instrument identifier and its guts. */
extern int
uschi_trav_instr(uschi_t, void(*cb)(insid_t, ins_t, void*), void *clo);

#endif	/* !INCLUDED_uschi_h_ */
