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

#include "order.h"

#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

#define UM_PORT		(12768)

static void
send_order(int fd, oside_t s, const char *p, const char *q)
{
	struct umo_s o = {
		.agent_id = 1,
		.secu_id = 2,
		.p = ffff_m30_get_s(&p),
		.q = strtol(q, NULL, 10),
		.side = s,
		.type = OTYPE_LIM,
		.tymod = OTYMOD_GTC,
	};
	write(fd, &o, sizeof(o));
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
	sa->sin6_addr = *(struct in6_addr*)(he->h_addr);
	sa->sin6_port = htons(UM_PORT);

	if (connect(s, (struct sockaddr*)sa, sizeof(*sa)) < 0) {
		fprintf(stderr, "Connection failed\n");
		return 1;
	}

	/* send an order to s */
	{
		oside_t bs = argv[2][0] == 'b' ? OSIDE_BUY : OSIDE_SELL;
		send_order(s, bs, argv[3], argv[4]);
	}

	/* and off we go */
	close(s);
	return 0;
}

/* um-order.c ends here */
