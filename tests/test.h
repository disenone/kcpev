#ifndef __TEST_H__
#define __TEST_H__

#include <ev.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* get system time */
void itimeofday(long *sec, long *usec);

/* get clock in millisecond 64 */
int64_t iclock64(void);

uint32_t iclock();

/* sleep in millisecond */
void isleep(unsigned long millisecond);

int setnonblocking(int fd); 

int create_rand_str(char* ptr, int min_len, int max_len);

typedef void (*stdin_callback)(EV_P_ struct ev_watcher *w, int revents, const char *buf, size_t len);
typedef void (*ev_io_callback)(EV_P_ struct ev_io *w, int revents);
typedef void (*ev_timer_callback)(EV_P_ struct ev_timer *w, int revents);

ev_watcher* setup_stdin(EV_P_ void *data, stdin_callback cb);

#ifdef __cplusplus
}
#endif
#endif
