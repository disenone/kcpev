#include <kcpev.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "dbg.h"

#define BACKLOG 128
#define PORT "0"

// 基于kcpev的客户端
//

int main()
{
    Kcpev *kcpev;
	struct ev_loop *loop = EV_DEFAULT;

    int ret = kcpev_init_client(&kcpev, EV_DEFAULT, PORT);
    check(ret >= 0, "init client");

    ret = kcpev_connect(kcpev, "127.0.0.1", "33333");
    check(ret >= 0, "connect");

    char *msg = "hi there";
    send(kcpev->udp.sock, msg, strlen(msg), 0);

    return ev_run(loop, 0);
error:
    return -1;
}
