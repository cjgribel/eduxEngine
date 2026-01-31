// -------------------------------------------------------------------------
// File:    cTimer
// Desc:    a simple timer (start-stop watch)
//
// Author:  Tomas Akenine-Möller
// History: March, 2000 (started)
//          July 2002, rewrote for PCs
// -------------------------------------------------------------------------

#include "timer.h"

cTimer::cTimer()
{
   mTotalTime=0.0;
//   QueryPerformanceFrequency(&mFrequency);
}

void cTimer::start(void)
{
#ifdef _WIN32
	mStartTime = GetTickCount64();
#else
	gettimeofday(&mStartTime,NULL);
#endif
//	QueryPerformanceCounter(&mStartTime);
}


void cTimer::stop(void)
{
#ifdef _WIN32
	unsigned long long endTime = GetTickCount64();
	mTotalTime += (double)(endTime - mStartTime)*1e-3;
#else
	struct timeval end_time;
	gettimeofday(&end_time,NULL);
	mTotalTime+=(((double)end_time.tv_sec*1000000.0+end_time.tv_usec)-((double)mStartTime.tv_sec*1000000.0+mStartTime.tv_usec))/1000000.0;
#endif

//	LARGE_INTEGER end_time;
//	QueryPerformanceCounter(&end_time);
//	mTotalTime+=double(end_time.LowPart-mStartTime.LowPart)/mFrequency.LowPart;	
}


void cTimer::reset(void)
{
   mTotalTime=0.0;
}

double cTimer::getTime(void)  // in seconds
{
   return mTotalTime;
}
