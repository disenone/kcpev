#include <kcpev.h>
#include <stdio.h>
#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#   include <stdint.h>
#   include <windows.h>
#else
#   include <netdb.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#endif
#include "dbg.h"
#include "test.h"

#define BACKLOG 128
#define PORT "0"

// 基于kcpev的客户端
//

void on_stdin_read(EV_P_ struct ev_watcher *w, int revents, const char *buf, size_t len)
{
	int ret = kcpev_send(w->data, buf, len);
	check(ret >= 0, "");
error:
    return;
}

void recv_cb(void *kcpev, const char *buf, int len)
{
    debug("recv_cb");
}


int main()
{
    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;

    kcpev = kcpev_create_client(loop, PORT, AF_INET);
    check(kcpev, "init client");

    int ret = kcpev_connect(kcpev, "127.0.0.1", "33333");
    check(ret >= 0, "connect");

    kcpev_set_cb(kcpev, recv_cb, NULL);

    setup_stdin(loop, kcpev, on_stdin_read);

	printf(">> ");
	fflush(stdout);

    ev_run(loop, 0);
	return 0;
error:
    return -1;
}


