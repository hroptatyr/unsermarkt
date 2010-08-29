/* expected to be included in dso-oq.c */

#include "md5.h"
#include "md5.c"

/* maximum number of http clients */
#define MAX_CLIENTS	(8)

static int htpush[MAX_CLIENTS];
static int status_updated = 0;

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
static void __attribute__((unused))
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
prxmlhdr(void)
{
	static const char hdr[] = "<?xml version=\"1.0\"?>\n";
	append(hdr, sizeof(hdr) - 1);
	return;
}

static void __attribute__((unused))
prep_http_status(void)
{
	/* http mode */
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
prep_htws_status(void)
{
	/* htws mode */
	reset();
	*mptr++ = 0x00;
	prxmlhdr();
	pr_otag();
	/* go through all bids, then all asks */
	oq_trav_bids(q, prstbcb, NULL);
	oq_trav_asks(q, prstacb, NULL);
	pr_ctag();
	*mptr++ = 0xff;
	return;
}

static void
prstatus(int fd)
{
/* prints the current order queue to FD */
	/* check if status needs updating */
	if (!status_updated) {
		prep_htws_status();
		status_updated = 1;
	}

#if 0
/* only in http mode */
	{
		char len[16];
		size_t lenlen;
		size_t chlen = mptr - mbuf;

		/* compute the chunk length */
		lenlen = snprintf(len, sizeof(len), "%zx\r\n", chlen);
		write(fd, len, lenlen);
		/* put the final \r\n */
		*mptr++ = '\r';
		*mptr++ = '\n';
	}
#else
/* htws mode */
	;
#endif
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
wsget_key(char *msg, size_t msglen, int k)
{
	static char cookie[] = "Sec-WebSocket-Keyx:";
	char *key;
	char *fin;
	uint32_t res = 0;
	int nspc = 0;
	char num[16] = {0};

	/* turn the cookie into Key1 or Key2 */
	cookie[sizeof(cookie) - 3] = k + '0';
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
	md5_byte_t hashme[16] __attribute__((aligned(8)));
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
	((uint32_t*)hashme)[0] = htonl(k1);
	((uint32_t*)hashme)[1] = htonl(k2);
	((uint64_t*)hashme)[1] = *(uint64_t*)challenge;

	/* grind through the md5 mill */
	md5_init(st);
	md5_append(st, hashme, sizeof(hashme));
	md5_finish(st, hashme);

	/* ... and finally the response */
	append((char*)hashme, sizeof(hashme));
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
static int
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

static int
htws_get_p(const char *msg, size_t UNUSED(msglen))
{
	static const char get_cookie[] = "GET /";
	return strncmp(msg, get_cookie, sizeof(get_cookie) - 1) == 0;
}

static int
htws_handle_get(int fd, char *msg, size_t msglen)
{
	/* first of all render the status bit void
	 * coz we're using the same buffer */
	status_updated = 0;
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
	/* flag status as outdated */
	status_updated = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (htpush[i] > 0) {
			prstatus(htpush[i]);
		}
	}
	return;
}


/* dealing with the closing handshake */
static const char htws_clo_seq[2] = {0xff, 0x00};

static int
htws_clo_p(const char *msg, size_t UNUSED(msglen))
{
	return memcmp(msg, htws_clo_seq, sizeof(htws_clo_seq)) == 0;
}

static int
htws_handle_clo(int fd, char *UNUSED(msg), size_t UNUSED(msglen))
{
	write(fd, htws_clo_seq, sizeof(htws_clo_seq));
	return -1;
}

/* htws.c ends here */
