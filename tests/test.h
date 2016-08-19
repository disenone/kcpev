#ifndef __TEST_H__
#define __TEST_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

/* get system time */
static inline void itimeofday(long *sec, long *usec)
{
	#if defined(__unix) || defined(__APPLE__)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
static inline IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

static inline IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

/* sleep in millisecond */
static inline void isleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	/*nanosleep(&ts, NULL);*/
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}

int setnonblocking(int fd) 
{
	int flag = fcntl(fd, F_GETFL, 0);
	if (flag < 0) 
	{
		return -1;
	}
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) 
	{
		return -1;
	}

	return 0;
}

int create_rand_str(char* ptr, int min_len, int max_len)
{
    int len = rand() % (max_len - min_len) + min_len;
    for(int i = 0; i < len; ++i)
    {
        ptr[i] = rand() % 126 + 1;
    }
    ptr[len] = '\0';
    return len;
}
#endif
