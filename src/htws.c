/*** htws.c -- websocket handshake
 *
 **/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* for *to* macroes */
#include <netinet/in.h>

#include "md5.h"
#include "nifty.h"
#include "htws.h"

/* should be enough for a websocket handshake */
static char mbuf[4096], *mptr;


/* order queue */
static void
reset(void)
{
	mptr = mbuf;
	return;
}

static void
append(const char *ptr, size_t len)
{
	memcpy(mptr, ptr, len);
	mptr += len;
	return;
}

/* To prove that the handshake was received, the server has to take
 * three pieces of information and combine them to form a response.  The
 * first two pieces of information come from the |Sec-WebSocket-Key1|
 * and |Sec-WebSocket-Key2| fields in the client handshake:
 *
 *      Sec-WebSocket-Key1: 18x 6]8vM;54 *(5:  {   U1]8  z [  8
 *      Sec-WebSocket-Key2: 1_ tx7X d  <  nw  334J702) 7]o}` 0
 * 
 * For each of these fields, the server has to take the digits from the
 * value to obtain a number (in this case 1868545188 and 1733470270
 * respectively), then divide that number by the number of spaces
 * characters in the value (in this case 12 and 10) to obtain a 32-bit
 * number (155712099 and 173347027).  These two resulting numbers are
 * then used in the server handshake, as described below.
 */
static uint32_t
wsget_key(char *msg, size_t msglen, int k)
{
	static char cookie[] = "Sec-WebSocket-Keyx:";
	char *key;
	char *fin;
	uint32_t res = 0;
	int nspc = 0;
	char num[16] = {0};

	/* turn the cookie into Key1 or Key2 */
	cookie[sizeof(cookie) - 3] = (char)(k + '0');
	if (UNLIKELY((key = strcasestr(msg, cookie)) == NULL)) {
		/* google sends no keys */
		return 0;
	}
	fin = memchr(key, '\n', msglen - (key - msg));
	/* look for the value offset, stupid websocket protocol
	 * makes first space optional */
	if (LIKELY((key += sizeof(cookie) - 1)[0] == ' ')) {
		key++;
	}
	/* look out for spaces */
	for (char *p = key, *np = num; p < fin; p++) {
		switch (*p) {
		case ' ':
			nspc++;
			break;
		case '0' ... '9':
			*np++ = *p;
			break;
		default:
			break;
		}
	}
	/* terminate np */
	res = strtoul(num, NULL, 10);
        res /= nspc;
	return res;
}

static int
wsget_append_challenge_response(char *msg, size_t msglen)
{
	/* find the keys */
	uint32_t k1 = wsget_key(msg, msglen, 1);
	uint32_t k2 = wsget_key(msg, msglen, 2);
	/* buffer to store the key and the hash respectively */
	union {
		md5_byte_t b[16];
		uint32_t u32[4];
		uint64_t u64[2];
	} hashme __attribute__((aligned(8)));
	struct md5_state_s st[1];
	/* find the end of the header block */
	char *challenge;

	/* always append these */
	*mptr++ = '\r';
	*mptr++ = '\n';

	/* check if we found the keys, chrome doesn't send keys :( */
	if (UNLIKELY(k1 == 0 || k2 == 0)) {
		return -1;
	}
	/* find the final 8 bytes of the hash info */
	if (UNLIKELY((challenge = strstr(msg, "\r\n\r\n")) == NULL)) {
		return -1;
	}
	/* zap to the content after the \r\n\r\n cookie */
	challenge += 4;

	/* populate the keys and the challenge */
	hashme.u32[0] = htonl(k1);
	hashme.u32[1] = htonl(k2);
	hashme.u64[1] = *(uint64_t*)challenge;

	/* grind through the md5 mill */
	md5_init(st);
	md5_append(st, hashme.b, sizeof(hashme));
	md5_finish(st, hashme.b);

	/* ... and finally the response */
	append((char*)hashme.b, sizeof(hashme));
	return 0;
}

static int
wsget_append_origin(char *msg, size_t msglen)
{
	static char prefix[] = "Sec-WebSocket-Origin: ";
	static char cookie[] = "\r\nOrigin:";
	/* find the origin header */
	char *origin;
	char *eol_o;

	if (UNLIKELY((origin = strcasestr(msg, cookie)) == NULL)) {
		return -1;
	}
	/* find the value, just zap to the first char after : */
	origin += sizeof(cookie) - (origin[sizeof(cookie) - 1] != ' ');
	/* find the end of the line */
	eol_o = memchr(origin, '\n', msglen - (origin - msg));
	/* append prefix and repeat the Origin header */
	append(prefix, sizeof(prefix) - 1);
	append(origin, eol_o - origin + 1/*for \n*/);
	return 0;
}

static int
wsget_append_location(char *msg, size_t msglen)
{
/* location apparently needs a path plus the Host component */
	static char prefix[] = "Sec-WebSocket-Location: ws://";
	static char cookie[] = "\r\nHost:";
	/* find the origin header */
	char *host;
	char *eol_h;

	if (UNLIKELY((host = strcasestr(msg, cookie)) == NULL)) {
		return -1;
	}
	/* find the value, just zap to the first char after : */
	host += sizeof(cookie) - (host[sizeof(cookie) - 1] != ' ');
	/* find the end of the line */
	eol_h = memchr(host, '\n', msglen - (host - msg));
	/* append prefix and repeat the Origin header */
	append(prefix, sizeof(prefix) - 1);
	append(host, eol_h - host - 1/*cut off \r\n*/);
	/* append our location */
	*mptr++ = '/';
	/* and the line terminator */
	*mptr++ = '\r';
	*mptr++ = '\n';
	return 0;
}

/* see http://www.whatwg.org/specs/web-socket-protocol/ */
int
wsget_challenge(int fd, char *msg, size_t msglen)
{
	static const char wsget_resp[] = "\
HTTP/1.1 101 WebSocket Protocol Handshake\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
";

	/* buffer reset and initialising */
	reset();
	msg[msglen] = '\0';
	/* generic answer */
	append(wsget_resp, sizeof(wsget_resp) - 1);
	/* find the Origin header */
	wsget_append_origin(msg, msglen);
	/* find the Host header and turn it into a Location one */
	wsget_append_location(msg, msglen);
	/* determine the challenge and respond */
	wsget_append_challenge_response(msg, msglen);

	/* write the challenge response */
	write(fd, mbuf, mptr - mbuf);
	return 0;
}

/* htws.c ends here */
