#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
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

	return client_sock;

error:
	exit(EXIT_FAILURE);
}

int main()
{
	int client_sock = make_sock("127.0.0.1");

	char buf[ECHO_LEN];
	char *buf_in;
	for(;;)
	{
error:
		printf(">> ");
		fflush(stdout);
		buf_in = fgets(buf, ECHO_LEN-1, stdin);
		check(buf_in != NULL, "get stdin");

		int ret = send(client_sock, buf, strlen(buf), 0);
		check(ret > 0, "send");

		ret = recv(client_sock, buf, ECHO_LEN-1, 0);
		check(ret > 0, "recv");
		buf[ret] = '\0';

		printf("\nrecv from server: %s\n", buf);

	}
}
