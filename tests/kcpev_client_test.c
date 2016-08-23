#include <kcpev.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "dbg.h"

#define BACKLOG 128
#define PORT "0"

// 基于kcpev的客户端
//


void stdin_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[KCPEV_BUFFER_SIZE];
	char *buf_in;
	buf_in = fgets(buf, sizeof(buf) - 1, stdin);
	check(buf_in != NULL, "get stdin");
    int ret = kcpev_send(w->data, buf, strlen(buf));
	check(ret == 0, "");

	printf(">> ");
	fflush(stdout);
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

	ev_io ev_stdin;
	ev_stdin.data = kcpev;
	ev_io_init(&ev_stdin, stdin_read, STDIN_FILENO, EV_READ);
	ev_io_start(loop, &ev_stdin);

	printf(">> ");
	fflush(stdout);

    ev_run(loop, 0);
	return 0;
error:
    return -1;
}


