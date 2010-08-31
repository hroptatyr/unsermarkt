#if !defined INCLUDED_um_conn_h_
#define INCLUDED_um_conn_h_

/* maximum number of http clients */
#if !defined MAX_CLIENTS
# define MAX_CLIENTS	(1024)
#endif	/* MAX_CLIENTS */

typedef struct um_conn_s *um_conn_t;

struct um_conn_s {
	int fd:30;
	int flags:2;
	agtid_t agent;
};

static struct um_conn_s um_conn[MAX_CLIENTS];

/* htpush connexions */
static um_conn_t
um_conn_memorise(int fd, agtid_t a)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (um_conn[slot].fd <= 0) {
			break;
		}
	}
	if (slot == MAX_CLIENTS) {
		/* no more room */
		return NULL;
	}
	um_conn[slot].fd = fd;
	um_conn[slot].agent = a;
	return um_conn + slot;
}

static void
um_conn_forget(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (um_conn[slot].fd == fd) {
			um_conn[slot].fd = 0;
			return;
		}
	}
	return;
}

static inline um_conn_t
um_conn_find_by_fd(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (um_conn[slot].fd == fd) {
			return um_conn + slot;
		}
	}
	return NULL;
}

#define FOR_EACH_CONN(c)						\
	for (um_conn_t c = um_conn; c < um_conn + MAX_CLIENTS; c++)	\
		if (c->fd > 0)

#endif	/* !INCLUDED_um_conn_h_ */
