/* stub to get current time stamp */

#if !defined INCLUDED_um_stamp_h_
#define INCLUDED_um_stamp_h_

#include <sys/time.h>
#include <time.h>

static struct timespec
hrclock_stamp(void)
{
	struct timespec tsp[1];
	clock_gettime(CLOCK_REALTIME, tsp);
	return *tsp;
}

#endif	/* !INCLUDED_um_stamp_h_ */
