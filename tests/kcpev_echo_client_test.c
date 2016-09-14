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
#include "dbg.h"

#define BACKLOG 128
#define PORT "0"

// 基于kcpev的客户端
//

void on_stdin_read(EV_P_ struct ev_watcher *w, int revents, const char *buf, size_t len)
{
    static int odd = 0;
    int ret = -1;
	//odd = (odd + 1) % 2;
    Kcpev *kcpev = w->data;
	if(odd)
	    ret = kcpev_send(kcpev, buf, strlen(buf));
	else
		ret = kcpev_send_tcp(kcpev, buf, strlen(buf));

	check(ret >= 0, "");
error:
    return;
}

void recv_cb(void *kcpev, const char *buf, int len)
{
    char *data = malloc(len + 1);
    memcpy(data, buf, len);
    data[len] = '\0';
    debug("%s", data);
	printf(">> ");
	fflush(stdout);
    free(data);
}


int main(int argc, char* argv[])
{
    if(argc != 3)
	{
		printf("usage:kcpev_remote_package_test server_ip server_port\n");
		return 0;
	}

    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;

    kcpev = kcpev_create_client(loop, PORT, AF_INET);
    check(kcpev, "init client");

    int ret = kcpev_connect(kcpev, argv[1], argv[2]);
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


