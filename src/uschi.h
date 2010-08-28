
#if !defined INCLUDED_uschi_h_
#define INCLUDED_uschi_h_

#include "match.h"

/* main uschi structure */
typedef struct uschi_s *uschi_t;


/* ctor/dtor */
extern uschi_t make_uschi(void);
extern void free_uschi(uschi_t);

extern insid_t uschi_add_instr(uschi_t, char *name);
extern agtid_t uschi_add_agent(uschi_t, char *nick);

extern mid_t uschi_add_match(uschi_t, umm_t match);

#endif	/* !INCLUDED_uschi_h_ */
