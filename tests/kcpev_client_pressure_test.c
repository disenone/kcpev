#include <kcpev.h>
#include <stdio.h>
#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#   include <stdint.h>
#else
#   include <netdb.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#endif
#include <fcntl.h>
#include <time.h>
#include "dbg.h"
#include "test.h"

#define BACKLOG 128
#define PORT "0"

// 基于kcpev的客户端
//

void on_timer(EV_P_ struct ev_io *w, int revents)
{
    char buf[KCPEV_BUFFER_SIZE];
    int len = create_rand_str(buf, 1, sizeof(buf));
    kcpev_send(w->data, buf, len);

    debug("send [%d]", len);
}

void on_stop(EV_P_ struct ev_io *w, int revents)
{
    exit(1);
}

int main(int argc, char* argv[])
{
    if(argc != 3)
	{
		printf("usage: kcpev_client_pressure_test server_ip server_port\n");
		return 0;
	}

    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;

    kcpev = kcpev_create_client(loop, PORT, AF_INET);
    check(kcpev, "init client");

    int ret = kcpev_connect(kcpev, argv[1], argv[2]);
    check(ret >= 0, "connect");

    ev_timer evt;
    evt.data = kcpev;
    ev_timer_init(&evt, on_timer, 1, 0.01);
    ev_timer_start(loop, &evt);

    ev_timer evs;
    ev_timer_init(&evs, on_stop, 100, 0);
    ev_timer_start(loop, &evs);

    ev_run(loop, 0);
	return 0;
error:
    return -1;
}

