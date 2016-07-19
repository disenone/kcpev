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

// echo_server based on libev and udp
//
#define PORT "12321"	// 连接端口
#define ECHO_LEN	1025
#define NI_MAXHOST  1025
#define NI_MAXSERV	32

int make_sock()
{
	struct addrinfo hints, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_DGRAM;	
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo(NULL, PORT, &hints, &server_addr);
	check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

	int server_sock;
	struct addrinfo *p;
	for(p = server_addr; p != NULL; p = p->ai_next)
	{
		server_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (server_sock == -1)
		{
			perror("socket ERROR");
			continue;
		}

		int opt = 1;
		ret = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (ret == -1)
		{
			perror("reuseaddr ERROR");
			continue;
		}

		ret = bind(server_sock, p->ai_addr, p->ai_addrlen);
		if (ret == -1)
		{
			close(server_sock);
			perror("bind ERROR");
			continue;
		}
		break;
	}

	check(p != NULL, "failed to make socket!");

	freeaddrinfo(server_addr);

	ret = setnonblocking(server_sock);
	check(ret == 0, "setnonblocking");

	return server_sock;

error:
	exit(EXIT_FAILURE);
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	int ret = send((intptr_t)user, buf, len, 0);
	check(ret > 0, "send");

error:
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

void try_recv(ikcpcb *kcp)
{
	char buf[ECHO_LEN];
	int ret = ikcp_recv(kcp, buf, ECHO_LEN - 1);
	int fd = (intptr_t)kcp->user;
	if (ret > 0)
	{
		int len = ret;
		buf[len] = '\0';

		struct sockaddr_storage client_addr;
		socklen_t addr_size = sizeof(client_addr);
		getpeername(fd, (struct sockaddr*)&client_addr, &addr_size);
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
		ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
			sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
		check(ret == 0, "getnameinfo");

		printf("recv client [%s:%s]: %s\n", hbuf, sbuf, buf);
		ikcp_send(kcp, buf, len);
	}

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

void recv_client(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	int ret;

	ikcpcb *kcp = w->data;

	ret = recv((intptr_t)kcp->user, buf, ECHO_LEN - 1, 0);
	check_silently(ret > 0);

	buf[ret] = '\0';
	int len = ret;

	ikcp_input(kcp, buf, len);

	try_recv(kcp);

error:
	return;
}

void accept_client(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	char buf[ECHO_LEN];

	ikcpcb *kcp = w->data;

	int ret = recvfrom((intptr_t)kcp->user, buf, ECHO_LEN - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
	check_silently(ret > 0);
	int len = ret;

	// 读回来的数据给kcp处理
	ret = connect((intptr_t)kcp->user, (struct sockaddr *)&client_addr, addr_size);
	check(ret == 0, "connect client");
	ev_io_stop(EV_A_ w);
	ev_io_init(w, recv_client, w->fd, EV_READ);
	ev_io_start(EV_A_ w);

	ikcp_input(kcp, buf, len);

	try_recv(kcp);

	// 重新创建一个新的socket来接收别的客户端连接
	int new_sock = make_sock();
	ikcpcb *new_kcp = create_kcp(new_sock);

	ev_io* ev_server = (ev_io*)malloc(sizeof(ev_io));
	ev_server->data = new_kcp;
	ev_io_init(ev_server, accept_client, new_sock, EV_READ);
	ev_io_start(EV_A_ ev_server);

	ev_timer *new_timer = (ev_timer*)malloc(sizeof(ev_timer));
	new_timer->data = new_kcp;
	ev_timer_init(new_timer, on_timer, 0.001, 0.001);
	ev_timer_start(EV_A_ new_timer);
error:
	return;
}

int main()
{
	struct ev_loop *loop = EV_DEFAULT;
	int server_sock = make_sock();
	ikcpcb *kcp = create_kcp(server_sock);

	ev_io ev_server;
	ev_server.data = kcp;
	ev_io_init(&ev_server, accept_client, server_sock, EV_READ);
	ev_io_start(loop, &ev_server);

	ev_timer ev_timer;
	ev_timer.data = kcp;
	ev_timer_init(&ev_timer, on_timer, 0.001, 0.001);
	ev_timer_start(loop, &ev_timer);

	printf("waiting for client...\n");
	return ev_run(EV_A_ 0);
}

