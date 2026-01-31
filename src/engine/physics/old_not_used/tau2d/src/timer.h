// -------------------------------------------------------------------------
// File:    cTimer
// Desc:    a simple timer (start-stop watch)
//
// Author:  Tomas Akenine-Möller
// History: March, 2000 (started)
//          July 2002, rewrote for PCs
// -------------------------------------------------------------------------

#ifndef C_TIMER_H
#define	C_TIMER_H

#ifndef _WIN32
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include <stdlib.h>

class cTimer
{
protected:
//	LARGE_INTEGER mStartTime, mFrequency;
#ifndef _WIN32
	struct timeval mStartTime;
#else
	unsigned long long mStartTime;
#endif
	double mTotalTime;
public:
	cTimer();
	void start(void);					// starts the watch
	void stop(void);					// adds the time from start() to an internal time variable
	void reset(void);					// resets the internal time variable
	double getTime(void);				// in seconds 
};

#endif

