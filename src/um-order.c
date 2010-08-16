/* example order client */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dso-oq-order.h"

#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

#define UM_PORT		(12768)

static void
send_order(int fd)
{
	struct umo_s o[1];
	m30_t p = ffff_m30_get_d(12.90);
	m30_t q = ffff_m30_get_d(200);
	make_order(o, 1, 1, OSIDE_SELL, p, q);
	write(fd, o, sizeof(*o));
	return;
}

int
main(int argc, char *argv[])
{
	int s = socket(PF_INET6, SOCK_STREAM, 0);
	struct sockaddr_in6 sa[1] = {{0}};
	struct hostent *he;

	he = gethostbyname2(argv[1], AF_INET6);
	
	sa->sin6_family = he->h_addrtype;
	sa->sin6_addr = *(typeof(sa->sin6_addr)*)(he->h_addr);
	sa->sin6_port = htons(UM_PORT);

	if (connect(s, (struct sockaddr*)sa, sizeof(*sa)) < 0) {
		fprintf(stderr, "Connection failed\n");
		return 1;
	}

	/* send an order to s */
	send_order(s);

	/* and off we go */
	close(s);
	return 0;
}

/* um-order.c ends here */
