#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#   include <stdint.h>
#else
#   include <netdb.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <ev.h>
#include <kcpev.h>
#include "dbg.h"

// echo_client based on libev and udp
//
#define PORT 12322	// 连接端口开始数值，递增查找可用端口
#define ECHO_LEN 1025
#define SERVER_PORT "12321"

int make_sock(const char* addr)
{
	struct addrinfo hints, *client_addr, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		// ipv4 or ipv6
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
	hints.ai_family = AF_INET;		// ipv4 or ipv6
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

	return client_sock;

error:
	exit(EXIT_FAILURE);
}

void echo_read(EV_P_ struct ev_io *w, int revents)
{
	char buf[ECHO_LEN];
	int ret = recv(KCPEV_FD_TO_HANDLE(w->fd), buf, ECHO_LEN-1, 0);
	check(ret > 0, "recv");

	buf[ret] = '\0';

	printf("\nrecv from server: %s\n", buf);

	printf(">> ");
	fflush(stdout);
error:
	return;
}

ev_io ev_client;

void on_stdin_read(EV_P_ struct ev_watcher *w, int revents, const char *buf, size_t len)
{
    int ret = send(w->data, buf, len, 0);
	check(ret > 0, "send");
error:
    return;
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("usage: libev_udp_echo_client server_ip\n");
		return 0;
	}

#ifdef _WIN32
    WSADATA wsa_data;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

	struct ev_loop *loop = EV_DEFAULT;

	int client_sock = make_sock(argv[1]);

	printf(">> ");
	fflush(stdout);

    setup_stdin(loop, client_sock, on_stdin_read);

	ev_io_init(&ev_client, echo_read, KCPEV_HANDLE_TO_FD(client_sock), EV_READ);
	ev_io_start(loop, &ev_client);
	ev_run(loop, 0);
    return 0;
}

