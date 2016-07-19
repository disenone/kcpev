#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <ev.h>
#include "dbg.h"
#include "ikcp.h"
#include "test.h"

// echo_client based on libev and udp
//
#define PORT 12322	// 连接端口开始数值，递增查找可用端口
#define ECHO_LEN 1025
#define SERVER_PORT "12321"

int make_sock(const char* addr)
{
	struct addrinfo hints, *client_addr, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_DGRAM;	
	hints.ai_flags = AI_PASSIVE;

	char port_str[32];

	int client_sock;
	struct addrinfo *p;
	int port;
	for(port = PORT; ; ++port)
	{

		sprintf(port_str, "%d", port);
		int ret = getaddrinfo(NULL, port_str, &hints, &client_addr);
		check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

		for(p = client_addr; p != NULL; p = p->ai_next)
		{
			client_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (client_sock == -1)
			{
				perror("socket ERROR");
				continue;
			}

			// client用bind，允许server的udp可以connect过来，并且保证端口唯一自己使用
			ret = bind(client_sock, p->ai_addr, p->ai_addrlen);
			if (ret == -1)
			{
				close(client_sock);
				perror("bind ERROR");
				continue;
			}

			break;
		}

		freeaddrinfo(client_addr);
		if (p != NULL)
			break;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_DGRAM;	
	int ret = getaddrinfo(addr, SERVER_PORT, &hints, &server_addr);
	check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

	for(p = server_addr; p != NULL; p = p->ai_next)
	{
		ret = connect(client_sock, p->ai_addr, p->ai_addrlen);
		if (ret != 0)
		{
			perror("connect ERROR");
			continue;
		}
		break;
	}

	check(p != NULL, "failed to connect socket!");

	freeaddrinfo(server_addr);

	ret = setnonblocking(client_sock);
	check(ret == 0, "setnonblocking");

	return client_sock;

error:
	exit(EXIT_FAILURE);
}

void try_recv(ikcpcb *kcp)
{
	char buf[ECHO_LEN];
	int ret = ikcp_recv(kcp, buf, ECHO_LEN - 1);
	if (ret > 0)
	{
		buf[ret] = '\0';
		printf("\nrecv from server: %s\n", buf);
		printf(">> ");
		fflush(stdout);
	}

error:
	return;

}

void echo_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	int ret = recv(w->fd, buf, ECHO_LEN-1, 0);
	check(ret > 0, "recv");

	ikcpcb *kcp = w->data;
	ikcp_input(kcp, buf, ret);

	try_recv(kcp);
error:
	return;
}

void stdin_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	char *buf_in;
	buf_in = fgets(buf, ECHO_LEN-1, stdin);
	check(buf_in != NULL, "get stdin");
	
	ikcpcb *kcp = w->data;
	ikcp_send(kcp, buf, strlen(buf));
error:
	return;
}

void on_timer(struct ev_loop *loop, ev_timer *w, int revents)
{
	uint64_t now64 = ev_now(EV_A) * 1000;
	uint32_t now = now64 & 0xfffffffful;

	ikcpcb *kcp = w->data;

	ikcp_update(kcp, now);

	try_recv(kcp);
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	send((intptr_t)user, buf, len, 0);
	return 0;
}

ikcpcb* create_kcp(int sock)
{
	ikcpcb *kcp = ikcp_create(0x11223344, (void*)sock);
	ikcp_wndsize(kcp, 128, 128);
	ikcp_nodelay(kcp, 0, 10, 0, 0);
	kcp->output = udp_output;
	return kcp;
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("usage: libev_udp_echo_client server_ip\n");
		return 0;
	}

	struct ev_loop *loop = EV_DEFAULT;

	int client_sock = make_sock(argv[1]);
	ikcpcb *kcp = create_kcp(client_sock);

	ev_io ev_stdin;
	ev_stdin.data = kcp;
	ev_io_init(&ev_stdin, stdin_read, STDIN_FILENO, EV_READ);
	ev_io_start(loop, &ev_stdin);

	ev_io ev_client;
	ev_client.data = kcp;
	ev_io_init(&ev_client, echo_read, client_sock, EV_READ);
	ev_io_start(loop, &ev_client);

	ev_timer ev_timer;
	ev_timer.data = kcp;
	ev_timer_init(&ev_timer, on_timer, 0.001, 0.001);
	ev_timer_start(loop, &ev_timer);

	printf(">> ");
	fflush(stdout);
	return ev_run(EV_A_ 0);
}

