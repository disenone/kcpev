#include <kcpev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "dbg.h"

#define BACKLOG 128
#define PORT "33333"

// 基于kcpev的服务端
//

void recv_cb(KcpevServer *server, Kcpev* client, const char* buf, size_t len)
{
    kcpev_send(client, buf, len);
}

int main()
{
    KcpevServer *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;

    kcpev = kcpev_create_server(loop, PORT, AF_INET, BACKLOG);
    check(kcpev, "init server");

    kcpev_server_set_cb(kcpev, recv_cb, NULL);

    debug("wait for clients...");

    ev_run(loop, 0);
	return 0;
error:
    return -1;

}

