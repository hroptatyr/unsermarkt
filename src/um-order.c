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

#if defined __INTEL_COMPILER
#pragma warning (disable:2259)
#endif	/* __INTEL_COMPILER */

#define UM_PORT		(12768)

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
		return 1;
	}

	/* and off we go */
	close(s);
	return 0;
}

/* um-order.c ends here */
