/* expected to be included in dso-oq.c */

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
append(const char *ptr, size_t len)
{
	memcpy(mptr, ptr, len);
	mptr += len - 1;
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
	append(tag, sizeof(tag));
	return;
}

static void
pr_ctag(void)
{
	static const char tag[] = "</quotes>\n";
	append(tag, sizeof(tag));
	return;
}

static void
prbdry(void)
{
	static const char bdry[] = "--umbdry\r\n";
	append(bdry, sizeof(bdry));
	return;
}

static void
prcty(void)
{
	static const char cty[] = "Content-Type: application/xml\n\n\
<?xml version='1.0'?>\n";
	append(cty, sizeof(cty));
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
	/* reset the buffer */
	mptr = mbuf;
	/* write an initial tag and clear the hash table */
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

static int
handle_wsget(int fd, char *msg, size_t msglen)
{
	/* keep the connection open so we can push stuff */
	memorise_htpush(fd);
	/* obviously a browser managed to connect to us,
	 * print an initial status */
	prhttphdr(fd);
	prep_status();
	prstatus(fd);
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
