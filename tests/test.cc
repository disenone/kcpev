#include "test.h"
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#   include <windows.h>
#   include <conio.h>
#else
#   include <sys/time.h>
#endif
#include <ev.h>
#include <kcpev.h>

void itimeofday(long *sec, long *usec)
{
	#if defined(__unix) || defined(__APPLE__)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static int64_t freq = 1;
	int64_t qpc;
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

int64_t iclock64(void)
{
	long s, u;
	int64_t value;
	itimeofday(&s, &u);
	value = ((int64_t)s) * 1000 + (u / 1000);
	return value;
}

uint32_t iclock()
{
	return (uint32_t)(iclock64() & 0xfffffffful);
}

void isleep(unsigned long millisecond)
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
#ifdef _WIN32
    return 0;
#else
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
#endif
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


static stdin_callback stdin_cb = NULL;

#ifdef _WIN32
static void stdin_read(EV_P_ struct ev_watcher *w, int revents)
{
	static HANDLE stdinHandle;
	static int index = 0;
	static char buf[KCPEV_BUFFER_SIZE];
	// Get the IO handles
	stdinHandle = GetStdHandle(STD_INPUT_HANDLE);

	switch (WaitForSingleObject(stdinHandle, 10))
	{
	case(WAIT_TIMEOUT) :
		break; // return from this function to allow thread to terminate
	case(WAIT_OBJECT_0) :
		if (_kbhit()) // _kbhit() always returns immediately
		{
			int i = _getch();
			printf("%c", (char)i);
			buf[index++] = (char)i;
			if (i == 13)
			{
                buf[index] = '\0';
                if(stdin_cb)
                    stdin_cb(EV_A_ w, revents, buf, index);
				index = 0;
                printf("\n>> ");
                fflush(stdout);
			}
		}
		else // some sort of other events , we need to clear it from the queue
		{
			// clear events
			INPUT_RECORD r[512];
			DWORD read;
			ReadConsoleInput(stdinHandle, r, 512, &read);
		}
						break;
	case(WAIT_FAILED) :
		break;
	case(WAIT_ABANDONED) :
		break;
	default:
		break;
	}

}

#else
static void stdin_read(EV_P_ struct ev_watcher *w, int revents)
{

	char buf[KCPEV_BUFFER_SIZE];
	char *buf_in;
	buf_in = fgets(buf, ECHO_LEN-1, stdin);
	check(buf_in != NULL, "get stdin");
	
    if(stdin_cb)
        stdin_cb(EV_A_ w, revents, buf, strlen(buf));

	printf("\n>> ");
	fflush(stdout);
error:
	return;
}

#endif

ev_watcher* setup_stdin(EV_P_ void *data, stdin_callback cb)
{
    stdin_cb = cb;
#ifdef _WIN32
	ev_timer *evt = (ev_timer *)calloc(1, sizeof(ev_timer));
	evt->data = data;
	ev_timer_init(evt, (ev_timer_callback)stdin_read, 1, 0.05);
	ev_timer_start(EV_A_ evt);
    return (ev_watcher *)evt;
#else
	ev_io *ev_stdin = (ev_io *)cealloc(1, sizeof(ev_io));
	ev_io_init(ev_stdin, (ev_io_callback)stdin_read, STDIN_FILENO, EV_READ);
	ev_io_start(ev_A_ ev_stdin);
    return (ev_watcher *)ev_stdin;
#endif // _WIN32
}

