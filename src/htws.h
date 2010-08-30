#if !defined INCLUDED_htws_h_
#define INCLUDED_htws_h_

static int htws_get_p(const char *msg, size_t msglen);
static int htws_handle_get(int fd, char *msg, size_t msglen);
static int htws_clo_p(const char *msg, size_t msglen);
static int htws_handle_clo(int fd, char *msg, size_t msglen);

#endif	/* !INCLUDED_htws_h_ */
