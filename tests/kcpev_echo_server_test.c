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
#define PORT "33333"

// »ùÓÚkcpevµÄ echo_server
//

void recv_cb(KcpevServer *server, Kcpev* client, const char* buf, int len)
{
    /*char *data = malloc(len + 1);*/
    /*memcpy(data, buf, len);*/
    /*data[len] = '\0';*/
    /*debug("%s", data);*/
    /*free(data);*/

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

	printf("%d, %d\n", kcpev->tcp.sock, kcpev->udp.sock);

    ev_run(loop, 0);
	return 0;
error:
    return -1;

}

