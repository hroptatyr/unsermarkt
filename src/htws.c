/* expected to be included in dso-oq.c */

#include "md5.h"
#include "md5.c"

/* maximum number of http clients */
#define MAX_CLIENTS	(8)

static int htpush[MAX_CLIENTS];

/* htpush connexions */
static void
memorise_htpush(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (htpush[slot] <= 0) {
			break;
		}
	}
	if (slot == MAX_CLIENTS) {
		/* no more room */
		return;
	}
	htpush[slot] = fd;
	return;
}

static void
forget_htpush(int fd)
{
	int slot;
	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (htpush[slot] == fd) {
			htpush[slot] = 0;
			return;
		}
	}
	return;
}


/* order queue */
static void
prhttphdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 200 OK\r\n\
Date: Tue, 24 Aug 2010 21:51:08 GMT\r\n\
Server: unsermarkt/0.1\r\n\
Transfer-Encoding: chunked\r\n\
Connection: Keep-Alive\r\n\
Content-Type: multipart/x-mixed-replace;boundary=\"umbdry\"\r\n\r\n";
	write(fd, httphdr, sizeof(httphdr) - 1);
	return;
}

static char mbuf[4096], *mptr;

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
}

static void
prstcb(char side, uml_t l)
{
	char pri[32];

	ffff_m30_s(pri, l->p);
	mptr += sprintf(mptr, "<%c p=\"%s\" q=\"%u\"/>", side, pri, l->q);
	return;
}

static void
prstbcb(uml_t l, void *UNUSED(clo))
{
	prstcb('b', l);
	return;
}

static void
prstacb(uml_t l, void *UNUSED(clo))
{
	prstcb('a', l);
	return;
}

static void
pr_otag(void)
{
	static const char tag[] = "<quotes>";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
pr_ctag(void)
{
	static const char tag[] = "</quotes>\n";
	append(tag, sizeof(tag) - 1);
	return;
}

static void
prbdry(void)
{
	static const char bdry[] = "--umbdry\r\n";
	append(bdry, sizeof(bdry) - 1);
	return;
}

static void
prcty(void)
{
	static const char cty[] = "Content-Type: application/xml\n\n\
<?xml version='1.0'?>\n";
	append(cty, sizeof(cty) - 1);
	return;
}

static void
prfinale(void)
{
	*mptr++ = '\r';
	*mptr++ = '\n';
	return;
}

static void
prep_status(void)
{
	/* bla */
	reset();
	prbdry();
	prcty();
	pr_otag();
	/* go through all bids, then all asks */
	oq_trav_bids(q, prstbcb, NULL);
	oq_trav_asks(q, prstacb, NULL);
	pr_ctag();
	prbdry();
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	char len[16];
	size_t lenlen;
	size_t chlen = mptr - mbuf;

	/* compute the chunk length */
	lenlen = snprintf(len, sizeof(len), "%zx\r\n", chlen);
	write(fd, len, lenlen);

	/* put the final \r\n */
	prfinale();
	write(fd, mbuf, mptr - mbuf);
	return;
}

#if 0
static void
prhtwshdr(int fd)
{
	static const char httphdr[] = "\
HTTP/1.1 101 WebSocket Protocol Handshake\r\n\
Date: Tue, 24 Aug 2010 21:51:08 GMT\r\n\
Server: unsermarkt/0.1\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Origin: http://www.unserding.org:12768\r\n\
Sec-WebSocket-Location: ws://www.unserding.org:12768\r\n\
Transfer-Encoding: chunked\r\n\
Connection: Keep-Alive\r\n\
Content-Type: multipart/x-mixed-replace;boundary=\"umbdry\"\r\n\r\n";
	write(fd, httphdr, sizeof(httphdr) - 1);
	return;
}
#endif

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
wsget_key(char *msg, size_t UNUSED(msglen), int k)
{
	static char cookie[] = "Sec-WebSocket-Keyx:";
	char *key;
	char *fin;
	uint32_t res = 0;
	int nspc = 0;
	char num[16] = {0};

	/* turn the cookie into Key1 or Key2 */
	cookie[sizeof(cookie) - 3] = k + '0';
	key = strcasestr(msg, cookie);
	fin = rawmemchr(key, '\n');
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

/* see http://www.whatwg.org/specs/web-socket-protocol/ */
static int
wsget_challenge(int fd, char *msg, size_t msglen)
{
	static const char wsget_resp[] = "\
HTTP/1.1 101 WebSocket Protocol Handshake\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
";
	uint32_t k1, k2;


	/* buffer reset and initialising */
	reset();
	msg[msglen] = '\0';
	/* generic answer */
	append(wsget_resp, sizeof(wsget_resp) - 1);
	/* find the Origin header */
	{
		static char loca[] =
			"Sec-WebSocket-Location: ws://www.unserding.org:12768/\r\n";
		static char prefix[] = "Sec-WebSocket-";
		char *o = strcasestr(msg, "Origin:");
		char *f = rawmemchr(o, '\n');
		append(prefix, sizeof(prefix) - 1);
		append(o, f - o + 1);
		append(loca, sizeof(loca) - 1);
	}
	/* find the keys */
	k1 = wsget_key(msg, msglen, 1);
	k2 = wsget_key(msg, msglen, 2);

	/* find the final 8 bytes of the hash info */
	{
		char *o = strstr(msg, "\r\n\r\n");
		char hashme[16];
		struct md5_state_s st[1];
		uint32_t k1be = htonl(k1);
		uint32_t k2be = htonl(k2);

		memcpy(hashme + 0, &k1be, 4);
		memcpy(hashme + 4, &k2be, 4);
		memcpy(hashme + 8, o, 8);

		/* grind through the md5 mill */
		md5_init(st);
		md5_append(st, (md5_byte_t*)hashme, sizeof(hashme));
		md5_finish(st, (md5_byte_t*)hashme);

		*mptr++ = '\r';
		*mptr++ = '\n';
		append(hashme, sizeof(hashme));
	}
	/* write the challenge response */
	write(fd, mbuf, mptr - mbuf);
	return 0;
}

static int
handle_wsget(int fd, char *msg, size_t msglen)
{
	/* respond to what the client wants */
	wsget_challenge(fd, msg, msglen);
	/* keep the connection open so we can push stuff */
	memorise_htpush(fd);
	return 0;
}

static void
upstatus(void)
{
/* for all fds in the htpush queue print the status */
	prep_status();
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (htpush[i] > 0) {
			prstatus(htpush[i]);
		}
	}
	return;
}

/* htws.c ends here */