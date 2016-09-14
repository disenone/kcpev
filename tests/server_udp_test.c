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
#include "dbg.h"

// echo_server based on libev and udp
//
#define PORT "33333"	// Á¬½Ó¶Ë¿Ú
#define ECHO_LEN	1025
#define NI_MAXHOST  1025
#define NI_MAXSERV	32

int make_sock()
{
	struct addrinfo hints, *server_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		// ipv4 or ipv6
	hints.ai_socktype = SOCK_DGRAM;	
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo("127.0.0.1", PORT, &hints, &server_addr);
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

	return server_sock;

error:
	exit(EXIT_FAILURE);
}

int main()
{
	int ret;
#ifdef _WIN32
    WSADATA wsa_data;
    ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

	int server_sock = make_sock();

	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	char buf[ECHO_LEN];
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	for(;;)
	{
error:
		ret = recvfrom(server_sock, buf, ECHO_LEN - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
		check(ret > 0, "recvfrom error");
		buf[ret] = '\0';
		int len = ret;

		ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
			sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
		check(ret == 0, "getnameinfo");

		printf("recvfrom client [%s:%s] : %s\n", hbuf, sbuf, buf);

		ret = sendto(server_sock, buf, len, 0, (struct sockaddr *)&client_addr, addr_size);
	}
}
