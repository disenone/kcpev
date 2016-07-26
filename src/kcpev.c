#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ev.h>
#include "dbg.h"
#include "ikcp.h"
#include "kcpev.h"

// kcpev基本结构

static void* (*kcpev_malloc_hook)(size_t) = NULL;
static void (*kcpev_free_hook)(void *) = NULL;

static inline void *kcpev_malloc(size_t size)
{
    if (kcpev_malloc_hook)
    {
        void *p = kcpev_malloc_hook(size);
        memset(p, 0, size);
        return p;
    }

    return calloc(1, size);
}

static inline void kcpev_free(void *p)
{
    if (kcpev_free_hook)
        kcpev_free_hook(p);
    else
        free(p);
}

Kcpev *kcpev_create()
{
    Kcpev *kcpev = kcpev_malloc(sizeof(Kcpev));
    check_mem(kcpev);

    return kcpev;

error:
    free(kcpev);
    return NULL;
}


void kcpev_sock_destroy(struct ev_loop *loop, kcpev_sock *evs)
{
    if(evs->sock)
    {
        close(evs->sock);
        evs->sock = 0;
    }

    if(evs->evi)
    {
        ev_io_stop(loop, evs->evi);
        kcpev_free(evs->evi);
        evs->evi = NULL;
    }
}

void kcpev_udp_destroy(struct ev_loop *loop, kcpev_udp *evs)
{
    kcpev_sock_destroy(loop, (kcpev_sock *)evs);
    if (evs->kcp)
    {
        ikcp_release(evs->kcp);
        evs->kcp = NULL;
    }
}

void kcpev_destroy(Kcpev *kcpev)
{
    kcpev_sock_destroy(kcpev->loop, &kcpev->tcp);
    kcpev_udp_destroy(kcpev->loop, &kcpev->udp);
    kcpev_free(kcpev);
}

KcpevServer *kcpev_server_create()
{
    KcpevServer *kcpev = kcpev_malloc(sizeof(KcpevServer));
    check_mem(kcpev);

    return kcpev;

error:
    free(kcpev);
    return NULL;

}

void delete_hash(KcpevServer *kcpev)
{
    Kcpev *current, *tmp;

    HASH_ITER(hh, kcpev->hash, current, tmp) {
        HASH_DEL(kcpev->hash, current);
        kcpev_destroy(current);
  }
}

void kcpev_server_destroy(KcpevServer *kcpev)
{
    kcpev_sock_destroy(kcpev->loop, &kcpev->tcp);
    kcpev_udp_destroy(kcpev->loop, &kcpev->udp);
    delete_hash(kcpev);
    kcpev_free(kcpev);
}

static int setnonblocking(int fd) 
{
	int flag = fcntl(fd, F_GETFL, 0);
	if (flag < 0) 
	{
		return -1;
	}
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) 
	{
		return -1;
	}

	return 0;
}

// create ipv4 or ipv6 socket
// sock_type = SOCK_STREAM or SOCK_DGRAM
// if address = INADDR_ANY, will auto pick an usable address
// if port = INPORT_ANY, will auto pick an usable port
// family = AF_UNSPEC or AF_INET or AF_INET6
int kcpev_bind_socket(kcpev_sock *sock, int sock_type, const char *port, int family, int reuse)
{
   	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = sock_type;	
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo(NULL, port, &hints, &addr);
	check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

    sock->sock = 0; 

	struct addrinfo *p;
	for(p = addr; p != NULL; p = p->ai_next)
	{
		int s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        check_goto(s >= 0, "socket", sock_error);

        if (reuse)
        {
            int opt = 1;
		    ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            check(ret >= 0, "set reuse addr");
        }

		ret = bind(s, p->ai_addr, p->ai_addrlen);
        check_goto(ret >= 0, "bind", sock_error);

        /*ret = setnonblocking(s);*/
        /*check_goto(ret >= 0, "setnonblocking", sock_error);*/

        sock->sock = s;
        break;

    sock_error:
        if (s >= 0)
            close(s);
	}

	freeaddrinfo(addr);

	check(sock->sock, "failed to create socket!");

	return 0;

error:
    return -1;   
}

inline int kcpev_bind_tcp(kcpev_tcp *sock, const char *port, int family, int reuse)
{
    return kcpev_bind_socket((kcpev_sock *)sock, SOCK_STREAM, port, family, reuse);
}

inline int kcpev_bind_udp(kcpev_udp *sock, const char *port, int family, int reuse)
{
    return kcpev_bind_socket((kcpev_sock *)sock, SOCK_DGRAM, port, family, reuse);
}

// create socket and bind it
int kcpev_bind(Kcpev *kcpev, const char *port, int family, int reuse)
{
    int ret = 0;

    // tcp连接是必须的
    ret = kcpev_bind_tcp(&kcpev->tcp, port, family, reuse);
    check(ret >= 0, "kcpev_bind_tcp");

    // 如果udp创建失败，会退化到用tcp来通信
    ret = kcpev_bind_udp(&kcpev->udp, port, family, reuse);
    if (ret < 0)
    {
        debug("failed to create udp socket\n");
    }

    return 0;
error:
    return -1;
}

int kcpev_connect_socket(kcpev_sock *sock, int sock_type, const char *address, const char *port)
{
    if (!sock->sock)
        return -1;

   	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = sock_type;	
	hints.ai_flags = AI_PASSIVE;

	int ret = getaddrinfo(address, port, &hints, &addr);
	check(ret == 0, "getaddrinfo ERROR: %s", gai_strerror(ret));

	struct addrinfo *p;
    int success = -1;
	for(p = addr; p != NULL; p = p->ai_next)
	{
		ret = connect(sock->sock, p->ai_addr, p->ai_addrlen);
        if (ret < 0)
        {
            log_err("connect");
            continue;
        }
        success = 0;
        break;
	}
  
    check(success == 0, "connect");

    return success;

error:
    return -1;
}

inline int kcpev_connect_tcp(kcpev_tcp *sock, const char *address, const char *port)
{
    return kcpev_connect_socket((kcpev_sock *)sock, SOCK_STREAM, address, port);
}

inline int kcpev_connect_udp(kcpev_udp *sock, const char *address, const char *port)
{
    return kcpev_connect_socket((kcpev_sock *)sock, SOCK_DGRAM, address, port);
}

int kcpev_connect(Kcpev *kcpev, const char *address, const char *port)
{
     int ret = 0;

    // tcp连接是必须的
    ret = kcpev_connect_tcp(&kcpev->tcp, address, port);
    check(ret >= 0, "kcpev_connect_tcp");

    // 如果udp连接失败，会退化到用tcp来通信
    ret = kcpev_connect_udp(&kcpev->udp, address, port);
    if (ret < 0)
    {
        debug("failed to connect udp socket\n");
        if (kcpev->udp.sock)
        {
            close(kcpev->udp.sock);
            kcpev->udp.sock = 0;
        }

    }
    else if (kcpev->udp.kcp)
    {
        if (kcpev->udp.sock)
        {
            kcpev->udp.kcp->user = (void *)kcpev->udp.sock;
        }

    }

    return 0;

error:
    return -1;
}

int kcpev_listen(Kcpev *kcpev, int backlog)
{
    int ret;
    ret = listen(kcpev->tcp.sock, backlog);
    check(ret >=0, "tcp listen");

    return 0;
error:
    return -1;
}

// 创建kcp, 会使用kcpev里面的conv属性和udp套接口，
// 如果是空的，后面创建socket或者客户端连接服务端时会重新填写这两个属性
// kcp_mode: 0 默认模式, 1 普通模式, 2 极速模式
int kcpev_create_kcp(kcpev_udp *udp, int conv, int kcp_mode)
{
    int user = 0;
    if (udp->sock)
        user = udp->sock;

	ikcpcb *kcp = ikcp_create(conv, (void*)user);
    check_mem(kcp);

	ikcp_wndsize(kcp, 128, 128);

    switch(kcp_mode)
    {
        case 0:
            ikcp_nodelay(kcp, 0, 10, 0, 0);
            break;
        case 1:
            ikcp_nodelay(kcp, 0, 10, 0, 1);
            break;
        case 2:
	        ikcp_nodelay(kcp, 1, 10, 2, 1);
            break;
        default:
            sentinel("unrecognized kcp_mode");       
    }

    udp->kcp = kcp;
    return 0;

error:
    if (kcp)
        ikcp_release(kcp);
    return -1;
}

void tcp_recv(EV_P_ struct ev_io *w, int revents)
{

}

void tcp_accept(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	int client_sock = accept(w->fd, (struct sockaddr *)&client_addr, &addr_size);
    check(client_sock > 0, "accept");

    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret == 0, "getnameinfo");
	debug("accept client [%s:%s]\n", hbuf, sbuf);

error:
    return;
}

#define ECHO_LEN	1025
#define NI_MAXHOST  1025
#define NI_MAXSERV	32
void udp_recv(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	char buf[ECHO_LEN];
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int ret = recvfrom(w->fd, buf, ECHO_LEN - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
    check(ret > 0, "recvfrom error");
    buf[ret] = '\0';
    int len = ret;

    ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret == 0, "getnameinfo");

    printf("recvfrom client [%s:%s] : %s\n", hbuf, sbuf, buf);

error:
    return;
}

int kcpev_set_ev(Kcpev *kcpev, kcpev_sock *evs, ev_io_callback cb)
{
    if (!evs->sock)
        return 0;
    
    ev_io * ev_one = kcpev_malloc(sizeof(ev_io));
    check(ev_one != NULL, "kcpev_malloc");
    
    evs->evi = ev_one;

    ev_one->data = kcpev;
    ev_io_init(ev_one, cb, evs->sock, EV_READ);
    ev_io_start(kcpev->loop, ev_one);

    return 0;

error:
    return -1;
}

int kcpev_init_ev(Kcpev *kcpev, struct ev_loop *loop, ev_io_callback tcp_cb, ev_io_callback udp_cb)
{
	check(kcpev, "kcpev is NULL");

    kcpev->loop = loop;

    int ret = kcpev_set_ev(kcpev, (kcpev_sock *)&kcpev->tcp, tcp_cb);
    check(ret >= 0, "set tcp ev");

    ret = kcpev_set_ev(kcpev, (kcpev_sock *)&kcpev->udp, udp_cb);
    check_log(ret >= 0, "set udp ev");

    return 0;
error:
    return -1;
}

int kcpev_init_client(Kcpev **kcpev, struct ev_loop *loop, const char *port)
{
    *kcpev = kcpev_create();
    check(*kcpev, "kcpev_create");
    
    int ret = kcpev_bind(*kcpev, port, AF_UNSPEC, 0);
    check(ret >= 0, "kcpev_bind");

    ret = kcpev_create_kcp(&(*kcpev)->udp, (*kcpev)->key.split_key.conv, 0);
    check(ret >= 0, "create kcp");

    ret = kcpev_init_ev(*kcpev, loop, tcp_recv, udp_recv);
    check(ret >= 0, "init ev");

    return 0;

error:
    if (*kcpev)
    {
        kcpev_destroy(*kcpev);
        *kcpev = NULL;
    }
    return -1;      
}

int kcpev_init_server(KcpevServer **kcpev, struct ev_loop *loop, const char *port, int family, int backlog)
{
    *kcpev = NULL;

    *kcpev = kcpev_server_create();
    check(*kcpev, "kcpev_create");

    int reuse = 1;
    int ret = kcpev_bind((Kcpev *)*kcpev, port, family, reuse);
    check(ret >= 0, "kcpev_server_bind");

    ret = kcpev_init_ev((Kcpev *)*kcpev, loop, tcp_accept, udp_recv);
    check(ret >= 0, "init ev");

    ret = kcpev_listen((Kcpev *)*kcpev, backlog);
    check(ret >= 0, "listen");

    return 0;

error:
    if (*kcpev)
    {
        kcpev_server_destroy(*kcpev);
        *kcpev = NULL;
    }
    return -1;
}

