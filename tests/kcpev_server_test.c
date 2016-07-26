#include <kcpev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "dbg.h"

#define BACKLOG 128
#define PORT "33333"

// 基于kcpev的服务端
//

int main()
{
    KcpevServer *kcpev;
	struct ev_loop *loop = EV_DEFAULT;

    int ret = kcpev_init_server(&kcpev, loop, PORT, AF_INET6, BACKLOG);
    check(ret >= 0, "init server");

    printf("wait for clients...\n");

    return ev_run(loop, 0);
error:
    return -1;

}
