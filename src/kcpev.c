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

static void* (*kcpev_malloc_hook)(size_t) = NULL;
static void (*kcpev_free_hook)(void *) = NULL;

static inline void *kcpev_malloc(size_t size)
{
    if(kcpev_malloc_hook)
    {
        void *p = kcpev_malloc_hook(size);
        memset(p, 0, size);
        return p;
    }

    return calloc(1, size);
}

static inline void kcpev_free(void *p)
{
    if(kcpev_free_hook)
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

    if(evs->evio)
    {
        ev_io_stop(loop, evs->evio);
        kcpev_free(evs->evio);
        evs->evio = NULL;
    }
}

void kcpev_udp_destroy(struct ev_loop *loop, kcpev_udp *evs)
{
    kcpev_sock_destroy(loop, (kcpev_sock *)evs);
    if(evs->evt)
    {
        ev_timer_stop(loop, evs->evt);
        kcpev_free(evs->evt);
        evs->evt = NULL;
    }
    if(evs->kcp)
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
	if(flag < 0) 
	{
		return -1;
	}
	if(fcntl(fd, F_SETFL, flag | O_NONBLOCK) < 0) 
	{
		return -1;
	}

	return 0;
}

// create ipv4 or ipv6 socket
// sock_type = SOCK_STREAM or SOCK_DGRAM
// ifaddress = INADDR_ANY, will auto pick an usable address
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

        if(reuse)
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
        if(s >= 0)
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

    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    ret = getsockname(kcpev->tcp.sock, (struct sockaddr*)&client_addr, &addr_size);
    check(ret >= 0, "getpeername");
    ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret >= 0, "getnameinfo");
    debug("local tcp port[%s : %s]", hbuf, sbuf);

    // 如果udp创建失败，会退化到用tcp来通信
    ret = kcpev_bind_udp(&kcpev->udp, port, family, reuse);
    getsockname(kcpev->udp.sock, (struct sockaddr*)&client_addr, &addr_size);
    getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    debug("local udp port[%s : %s]", hbuf, sbuf);
    if(ret < 0)
    {
        debug("failed to create udp socket\n");
    }

    return 0;
error:
    return -1;
}

int kcpev_connect_socket(kcpev_sock *sock, int sock_type, const char *address, const char *port)
{
    if(!sock->sock)
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
        if(ret < 0)
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
    if(ret < 0)
    {
        debug("failed to connect udp socket\n");
        if(kcpev->udp.sock)
        {
            close(kcpev->udp.sock);
            kcpev->udp.sock = 0;
        }

    }
    else if(kcpev->udp.kcp)
    {
        kcpev->udp.kcp->user = (void *)&kcpev->udp;

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

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    kcpev_udp *client = user;
	int ret = send(client->sock, buf, len, 0);
	check(ret == len, "send");

error:
	return 0;
}

// 创建kcp, 会使用kcpev里面的conv属性和udp套接口，
// 如果是空的，后面创建socket或者客户端连接服务端时会重新填写这两个属性
// kcp_mode: 0 默认模式, 1 普通模式, 2 极速模式
int kcpev_create_kcp(kcpev_udp *udp, int conv, int kcp_mode)
{
	ikcpcb *kcp = ikcp_create(conv, udp);
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

    kcp->output = udp_output;

    udp->kcp = kcp;
    return 0;

error:
    if(kcp)
        ikcp_release(kcp);
    return -1;
}

// uint8_t command:
// 0: normal data
// 1: set key
void client_tcp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[RECV_LEN];
    uint8_t command;
    int ret = -1;
    
    Kcpev *client = w->data;

    int len = recv(w->fd, buf, sizeof(buf) - 1, 0);
    check(len > 0, "");

    buf[len] = '\0';
    command = (uint8_t)buf[0];
    switch(command)
    {
        case COMMAND_DATA:
            debug("tcp recv from server: %s", buf);
            break;

        case COMMAND_SET_KEY:
            if(len != sizeof(kcpev_key) + 1)
            {
                 log_err("set key data len error");
                 return;
            }
            memcpy(client->key.uuid, buf + 1, sizeof(kcpev_key));

            char uuids[37];
            uuid_unparse(client->key.uuid, uuids);
            debug("set key successfully [%s]!", uuids);

            // udp shake hand
            if(client->udp.sock)
            {
                send(client->udp.sock, buf, len, 0);
            }
            break;

        default:
            log_err("unrecognized command from server");
            break;
    }

    return;

error:
    debug("tcp recv error");
    exit(1);
    return;
}

int try_kcp_recv(Kcpev *kcpev)
{
    ikcpcb *kcp = kcpev->udp.kcp;

	char buf[RECV_LEN];
    int result = -1;
    for(;;)
    {
        int len = ikcp_recv(kcp, buf, sizeof(buf) - 1);
        check_silently(len > 0);
        buf[len] = '\0';

        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        getpeername(kcpev->udp.sock, (struct sockaddr*)&client_addr, &addr_size);
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        int ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
            sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        /*debug("kcp recv client [%s:%s]: [%d]\n", hbuf, sbuf, len);*/
        result = 0;

        if(kcpev->server)
            kcpev->server->recv_cb(kcpev->server, kcpev, buf, len);
        else
            kcpev->recv_cb(kcpev, buf, len);
    }

error:
    return result;
}

void kcpev_timer_repeat(Kcpev *kcpev)
{
    if(!kcpev)
        return;

    struct ev_loop *loop = kcpev->loop;
    ev_timer *evt = kcpev->udp.evt;
	ikcpcb *kcp = kcpev->udp.kcp;

	uint64_t now64 = ev_now(EV_A) * 1000;
	uint32_t now = now64 & 0xfffffffful;

	// use ikcp_check to decide if really need ikcp_update
	uint32_t next = ikcp_check(kcp, now);

	if (next <= now)
	{
		ikcp_update(kcp, now);
		next = ikcp_check(kcp, now);
	}

	if (next <= now)
		evt->repeat = 0.01;
	else
		evt->repeat = (next - now) / 1000.0;
	/*debug("%u, %u, %lf", now, next, w->repeat);*/
	ev_timer_again(EV_A_ evt);

}

void kcpev_on_timer(EV_P_ ev_timer *w, int revents)
{
    kcpev_timer_repeat((Kcpev *)w->data);
	try_kcp_recv(w->data);
}

// create kcp timer here
int check_create_kcp_timer(Kcpev *kcpev)
{
    if(!kcpev->udp.kcp->conv)
        return -1;

    if(kcpev->udp.evt)
        return -1;

    ev_timer *evt = kcpev_malloc(sizeof(ev_timer));
    check_mem(evt);

    kcpev->udp.evt = evt;
    evt->data = kcpev;
    ev_timer_init(evt, kcpev_on_timer, 0.01, 0.01);
    ev_timer_start(kcpev->loop, evt);

    return 0;

error:
    return -1;
}

// 如果 kcpev.conv 没有设置，说明还没有握手成功，来到这里收到服务端的udp包，直接设置kcp.conv
// 否则说明握手成功，用 kcp 接收数据
void client_udp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[RECV_LEN];
    int len = recv(w->fd, buf, sizeof(buf) - 1, 0);
    check(len > 0, "");
    buf[len] = '\0';

    Kcpev *client = w->data;

    if(!client->udp.kcp->conv)
    {
        client->udp.kcp->conv = client->key.split_key.conv;
        int ret = check_create_kcp_timer(client);
        check(ret >= 0, "check_create_kcp_timer");

        debug("shake hand successfully.");
        return;
    }

    ikcp_input(client->udp.kcp, buf, len);
 
    kcpev_timer_repeat(client);

    int ret = try_kcp_recv(client);
error:
    return;
}

void close_client(Kcpev *client)
{
    if(!client)
        return;

    KcpevServer *server = client->server;
    HASH_DEL(server->hash, client);
    kcpev_destroy(client);
}

void server_tcp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[RECV_LEN];
    Kcpev *client = w->data;

    int len = recv(w->fd, buf, sizeof(buf) - 1, 0);
    check(len > 0, "client tcp closed");
    buf[len] = '\0';
 
    char uuids[37];
    uuid_unparse(client->key.uuid, uuids);

    debug("tcp recv from client[%s]: %s", uuids, buf);
    return;

error:
    close_client(client);
    return;
}

void server_udp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[RECV_LEN];
    Kcpev *client = w->data;

    int len = recv(w->fd, buf, sizeof(buf) - 1, 0);
    check(len > 0, "");
  
    buf[len] = '\0';

    char uuids[37];
    uuid_unparse(client->key.uuid, uuids);

    ikcp_input(client->udp.kcp, buf, len);
 
    kcpev_timer_repeat(client);

    int ret = try_kcp_recv(client);
error:
    return;
}

// try create udp connection
int connect_client_udp(Kcpev *client, char *port, struct sockaddr *addr, socklen_t addr_size, \
    struct ev_loop *loop, void *data)
{
    int ret = kcpev_bind_udp(&client->udp, port, addr->sa_family, 1);
    check(ret >= 0, "server udp bind");

    ret = connect(client->udp.sock, addr, addr_size);
    check(ret >= 0, "connect client udp");

    ret = kcpev_create_kcp(&client->udp, client->key.split_key.conv, 2);
    check(ret >= 0, "client udp create kcp");

    ret = kcpev_init_ev(client, loop, data, server_tcp_recv, server_udp_recv);
    check(ret >= 0, "client init ev");

    ret = check_create_kcp_timer(client);
    check(ret >= 0, "check_create_kcp_timer");

    char *msg = "hi client";
    send(client->udp.sock, msg, strlen(msg), 0);
    return 0;

error:
    kcpev_udp_destroy(loop, &client->udp);
    return -1;
}

// 接受新的客户端，并发送key
void tcp_accept(EV_P_ struct ev_io *w, int revents)
{
    Kcpev *client = NULL;

	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	int client_sock = accept(w->fd, (struct sockaddr *)&client_addr, &addr_size);
    check(client_sock > 0, "accept");

    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret == 0, "getnameinfo");

    KcpevServer *server = w->data;

    // create new client structure and add to server
    client = kcpev_create();
    check_mem(client);

    client->tcp.sock = client_sock;
    uuid_generate(client->key.uuid);
    // create hashtable for client kcpev
    HASH_ADD(hh, server->hash, key, sizeof(kcpev_key), client);

    char uuids[37];
    uuid_unparse(client->key.uuid, uuids);
	debug("accept client [%s : %s : %s]\n", hbuf, sbuf, uuids);

    // send client key
    char buf[RECV_LEN];
    buf[0] = COMMAND_SET_KEY;
    memcpy(buf + 1, &client->key, sizeof(kcpev_key));
    send(client->tcp.sock, buf, sizeof(kcpev_key) + 1, 0);

    return;

error:
    if(client)
        kcpev_destroy(client);
    else if(client_sock)
        close(client_sock);
    return;
}

// 接收客户端的udp信息
void server_udp_recv_all(EV_P_ struct ev_io *w, int revents)
{
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
    char buf[RECV_LEN];

	int len = recvfrom(w->fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
    check(len > 0, "server recvfrom");

    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret >= 0, "getnameinfo");

    uint8_t command = (uint8_t)buf[0];

    KcpevServer *server = w->data;

    // command: 
    // 1: udp握手, data = key
    switch(command)
    {
        case COMMAND_SHAKE_HAND:
            check(len == 1 + sizeof(kcpev_key), "udp shake len error [%s : %s]", hbuf, sbuf);

            kcpev_key *key = (kcpev_key *)(buf + 1);
            Kcpev *client = NULL;
            
            HASH_FIND(hh, server->hash, key, sizeof(kcpev_key), client);
            check(client, "udp shake client key not find [%s : %s]", hbuf, sbuf);

            // udp may failed, if so, use tcp then
            ret = connect_client_udp(client, server->port, (struct sockaddr *)&client_addr, \
                addr_size, server->loop, client);
            check_goto(ret >= 0, "connect_client_udp", shake_error);

            client->server = (KcpevServer *)w->data;

            if(ret >= 0)
                debug("recv client shake hand msg");

            break;
    shake_error:
            break;
        default:
            sentinel("server_udp_recv_all command error");
    }

    buf[len] = '\0';

    debug("udp_all recv client [%s : %s]: [%d]\n", hbuf, sbuf, len);

error:

    return;
}

int kcpev_set_ev(struct ev_loop *loop, void *data, kcpev_sock *evs, ev_io_callback cb)
{
    if(!evs->sock)
        return 0;
    
    ev_io * ev_one = kcpev_malloc(sizeof(ev_io));
    check(ev_one != NULL, "kcpev_malloc");
    
    evs->evio = ev_one;

    ev_one->data = data;
    ev_io_init(ev_one, cb, evs->sock, EV_READ);
    ev_io_start(loop, ev_one);

    return 0;

error:
    return -1;
}

int kcpev_init_ev(Kcpev *kcpev, struct ev_loop *loop, void *data, ev_io_callback tcp_cb, ev_io_callback udp_cb)
{
	check(kcpev, "kcpev is NULL");

    kcpev->loop = loop;

    int ret = kcpev_set_ev(loop, data, (kcpev_sock *)&kcpev->tcp, tcp_cb);
    check(ret >= 0, "set tcp ev");

    if(udp_cb)
    {
        ret = kcpev_set_ev(loop, data, (kcpev_sock *)&kcpev->udp, udp_cb);
        check_log(ret >= 0, "set udp ev");
    }

    return 0;
error:
    return -1;
}

Kcpev *kcpev_create_client(struct ev_loop *loop, const char *port, int family)
{
    Kcpev *kcpev = kcpev_create();
    check(kcpev, "kcpev_create");
    
    int ret = kcpev_bind(kcpev, port, family, 0);
    check(ret >= 0, "kcpev_bind");

    ret = kcpev_create_kcp(&kcpev->udp, kcpev->key.split_key.conv, 2);
    check(ret >= 0, "create kcp");

    ret = kcpev_init_ev(kcpev, loop, kcpev, client_tcp_recv, client_udp_recv);
    check(ret >= 0, "init ev");

    return kcpev;

error:
    if(kcpev)
    {
        kcpev_destroy(kcpev);
        kcpev = NULL;
    }
    return NULL;
}

KcpevServer *kcpev_create_server(struct ev_loop *loop, const char *port, int family, int backlog)
{
    KcpevServer *kcpev = kcpev_server_create();
    check(kcpev, "kcpev_create");

    strncpy(kcpev->port, port, sizeof(kcpev->port));

    int reuse = 1;
    int ret = kcpev_bind((Kcpev *)kcpev, port, family, reuse);
    check(ret >= 0, "kcpev_server_bind");

    ret = kcpev_init_ev((Kcpev *)kcpev, loop, kcpev, tcp_accept, server_udp_recv_all);
    check(ret >= 0, "init ev");

    ret = kcpev_listen((Kcpev *)kcpev, backlog);
    check(ret >= 0, "listen");

    return kcpev;

error:
    if(kcpev)
    {
        kcpev_server_destroy(kcpev);
        kcpev = NULL;
    }
    return NULL;
}

int is_kcp_valid(Kcpev *kcpev)
{
    if(!kcpev->udp.sock)
        return 0;

    if(!kcpev->udp.kcp)
        return 0;

    if(!kcpev->udp.kcp->conv)
        return 0;

    if(!kcpev->udp.evt)
        return 0;

    return 1;
}

int kcpev_send(Kcpev *kcpev, const char *msg, int len)
{
    // 如果kcp能用，就用kcp来发消息，否则用tcp
    if(is_kcp_valid(kcpev))
    {
        int ret = ikcp_send(kcpev->udp.kcp, msg, len);
        kcpev_timer_repeat(kcpev);
        return ret;
    }
    else
        return send(kcpev->tcp.sock, msg, len, 0);
}

void kcpev_set_cb(Kcpev *kcpev, kcpev_recv_cb recv_cb, kcpev_disconnect_cb disconnect_cb)
{
   kcpev->recv_cb = recv_cb; 
}

void kcpev_server_set_cb(KcpevServer *kcpev, kcpev_server_recv_cb recv_cb, kcpev_server_disconnect_cb disconnect_cb)
{
    kcpev->recv_cb = recv_cb;
}

