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


void KcpevSock_destroy(struct ev_loop *loop, KcpevSock *evs)
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

void KcpevTcp_destroy(struct ev_loop *loop, KcpevTcp *evs)
{

    KcpevSock_destroy(loop, (KcpevSock *)evs);
    if(evs->rb)
    {
        ringbuf_free(evs->rb);
        evs->rb = NULL;
    }
}

void KcpevUdp_destroy(struct ev_loop *loop, KcpevUdp *evs)
{
    KcpevSock_destroy(loop, (KcpevSock *)evs);
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
    KcpevTcp_destroy(kcpev->loop, &kcpev->tcp);
    KcpevUdp_destroy(kcpev->loop, &kcpev->udp);
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
    KcpevTcp_destroy(kcpev->loop, &kcpev->tcp);
    KcpevUdp_destroy(kcpev->loop, &kcpev->udp);
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
int kcpev_bind_socket(KcpevSock *sock, int sock_type, const char *port, int family, int reuse)
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

inline int kcpev_bind_tcp(KcpevTcp *sock, const char *port, int family, int reuse)
{
    return kcpev_bind_socket((KcpevSock *)sock, SOCK_STREAM, port, family, reuse);
}

inline int kcpev_bind_udp(KcpevUdp *sock, const char *port, int family, int reuse)
{
    return kcpev_bind_socket((KcpevSock *)sock, SOCK_DGRAM, port, family, reuse);
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

int kcpev_connect_socket(KcpevSock *sock, int sock_type, const char *address, const char *port)
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

inline int kcpev_connect_tcp(KcpevTcp *sock, const char *address, const char *port)
{
    return kcpev_connect_socket((KcpevSock *)sock, SOCK_STREAM, address, port);
}

inline int kcpev_connect_udp(KcpevUdp *sock, const char *address, const char *port)
{
    return kcpev_connect_socket((KcpevSock *)sock, SOCK_DGRAM, address, port);
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

int udp_output(const char *buf, size_t len, ikcpcb *kcp, void *user)
{
    KcpevUdp *client = user;
    ssize_t ret = send(client->sock, buf, len, 0);
    check(ret == len, "send");

error:
    return 0;
}

// 创建kcp, 会使用kcpev里面的conv属性和udp套接口，
// 如果是空的，后面创建socket或者客户端连接服务端时会重新填写这两个属性
// kcp_mode: 0 默认模式, 1 普通模式, 2 极速模式
int kcpev_create_kcp(KcpevUdp *udp, int conv, int kcp_mode)
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

// buf = uint32_t(len) + uint8_t(command) + data
// #define COMMAND_DATA        0   # 普通数据
// #define COMMAND_SHAKE_HAND  1   # 握手
// #define COMMAND_HEARTBEAT   2   # 心跳
int on_client_recv(Kcpev *client, const char *buf, size_t len)
{
    KcpevHeader header;
    const size_t header_size = sizeof(KcpevHeader);
    int ret = -1;

    check(len >= header_size, "recv data len < %d", header_size);

    ret = header_from_net(&header, buf, len);
    check(ret == 0, "header_from_net");

    check(header.size == len, "recv data len error, len = %d, real_len = %d", len, header.size);

    const char *data = buf + header_size;
    size_t data_len = len - header_size;

    switch(header.command)
    {
        case COMMAND_DATA:
            client->recv_cb(client, data, data_len);
            break;

        case COMMAND_SHAKE_HAND:
            check(data_len == sizeof(KcpevKey), "set key data len error");

            memcpy(client->key.uuid, data, sizeof(KcpevKey));

            char uuids[UUID_PARSE_SIZE];
            uuid_unparse(client->key.uuid, uuids);
            debug("set key successfully [%s]!", uuids);

            // udp shake hand
            if(client->udp.sock)
            {
                sock_send_command(client->udp.sock, COMMAND_SHAKE_HAND, data, data_len);
            }
            break;

        case COMMAND_HEARTBEAT:
            break;

        default:
            sentinel("unrecognized command from server");
    }
   
    return 0;

error:
    return -1;
}

// buf = uint32_t(len) + uint8_t(command) + data
// #define COMMAND_DATA        0   # 普通数据
// #define COMMAND_SHAKE_HAND  1   # 握手
// #define COMMAND_HEARTBEAT   2   # 心跳
int on_server_recv(KcpevServer *server, Kcpev *client, const char *buf, size_t len, \
    const struct sockaddr *client_addr, int addr_size)
{
    KcpevHeader header;
    int ret = -1;

    const size_t header_size = sizeof(KcpevHeader);
    check(len >= header_size, "recv data len < %d", header_size);

    ret = header_from_net(&header, buf, len); 
    check(ret == 0, "header_from_net");
    check(header.size == len, "recv data len error len = %d, real_len = %d", len, header.size);

    const char *data = buf + header_size;
    size_t data_len = len - header_size;

    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    ret = getnameinfo(client_addr, addr_size, hbuf, sizeof(hbuf), \
        sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    check(ret == 0, "");

    switch(header.command)
    {
        case COMMAND_DATA:
            
            // debug("recv client data: [%s : %s], [%d]", hbuf, sbuf, data_len);
            server->recv_cb(server, client, data, data_len);
            break;

        case COMMAND_SHAKE_HAND:
            check(data_len == sizeof(KcpevKey), "udp shake len error [%s : %s]", hbuf, sbuf);

            KcpevKey *key = (KcpevKey *)data;
            client = NULL;
            
            HASH_FIND(hh, server->hash, key, sizeof(KcpevKey), client);
            check(client, "udp shake client key not find [%s : %s]", hbuf, sbuf);

            // udp may failed, if so, use tcp then
            ret = connect_client_udp(client, server->port, client_addr, \
                addr_size, server->loop);
            check_goto(ret >= 0, "connect_client_udp", shake_error);

            char *msg = "hi client";

            sock_send_command(client->udp.sock, COMMAND_DATA, msg, strlen(msg));

            if(ret >= 0)
                debug("recv client shake hand msg");

    shake_error:
            break;

        case COMMAND_HEARTBEAT:
            break;

        default:
            sentinel("unrecognized command from server");
    }
   
    return 0;

error:
    return -1;
}

size_t on_tcp_recv(Kcpev *kcpev, char* data, size_t len)
{
    KcpevTcp *tcp = &kcpev->tcp;
    KcpevHeader header;
    int ret = -1;
    ringbuf *rb = tcp->rb;
    ringbuf *new_rb;

    // no pending data
    if(!rb || ringbuf_get_pending_size(rb) == 0)
    {
        if(len >= sizeof(KcpevHeader))
        {
            ret = header_from_net(&header, data, len);
            check(ret == 0, "header_from_net");

            if(header.size == len)
            {
                return len;
            }
        }
    }

    if(!rb)
    {
        rb = ringbuf_new(KCPEV_BUFFER_SIZE);
        check_mem(rb);
        tcp->rb = rb;
    }

    ret = ringbuf_put(rb, data, len);
    if(ret == -1)
    {
        ringbuf *new_rb = ringbuf_new(rb->n + KCPEV_BUFFER_SIZE);

        check_mem(new_rb);

        char* chunk;
        int size;
        while((size = ringbuf_get_next_chunk(rb, &chunk)))
        {
            ret = ringbuf_put(new_rb, chunk, size);
            if(ret == -1)
            {
                ringbuf_free(new_rb);
                goto error;
            }

            ringbuf_mark_consumed(rb, size);
        }
    }

    return 0;

error:
    return -1;
}

size_t get_tcp_buf_chunk(Kcpev *kcpev, char *ret_data, size_t len)
{
    KcpevHeader header;
    ringbuf *rb = kcpev->tcp.rb;
    char *data;
    int ret = -1;
    size_t size;

    check(len >= sizeof(KcpevHeader), "ret buf len too small");

    check_silently(rb);

    size = ringbuf_get_pending_size(rb);
    check_silently(size >= sizeof(KcpevHeader));

    ret = ringbuf_copy_data(rb, ret_data, sizeof(KcpevHeader));
    check(ret == 0, "ringbuf_copy_data");

    header_from_net(&header, ret_data, len);

    if(header.size <= size)
    {
        ret = ringbuf_copy_data(rb, ret_data, header.size);
        check(ret == 0, "ringbuf_copy_data");
        ringbuf_mark_consumed(rb, header.size);
        return header.size;
    }

error:
    return 0;
}

void client_tcp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[KCPEV_BUFFER_SIZE];
    uint8_t command;
    ssize_t ret = -1;
    
    Kcpev *client = w->data;

    ssize_t len = recv(w->fd, buf, sizeof(buf), 0);
    check(len > 0, "");

    len = on_tcp_recv(client, buf, len);

    if(len > 0)
    {
        ret = on_client_recv(client, buf, len);
        check(ret == 0, "");
    }
    
    while((len = get_tcp_buf_chunk(client, buf, sizeof(buf))))
    {
        ret = on_client_recv(client, buf, len);
        check(ret == 0, "");
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

    char buf[KCPEV_BUFFER_SIZE];
    int result = -1;
    for(;;)
    {
        int len = ikcp_recv(kcp, buf, sizeof(buf));
        check_silently(len > 0);
        buf[len] = '\0';

        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        result = getpeername(kcpev->udp.sock, (struct sockaddr*)&client_addr, &addr_size);
        check(result == 0, "getpeername");

        // debug("kcp recv client [%d]\n", len);
        if(kcpev->server)
        {
            result = on_server_recv(kcpev->server, kcpev, buf, len, (struct sockaddr*)&client_addr, addr_size);
            check(result == 0, "");
        }
        else
        {
            result = on_client_recv(kcpev, buf, len);
            check(result == 0, "");
        }
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
    //debug("%u, %u, %lf", now, next, evt->repeat);
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
    char buf[KCPEV_BUFFER_SIZE];
    ssize_t len = recv(w->fd, buf, sizeof(buf), 0);
    check(len > 0, "");
    Kcpev *client = w->data;

    //debug("client recv udp raw: [%d]", len);

    if(!client->udp.kcp->conv)
    {
        client->udp.kcp->conv = client->key.split_key.conv;
        int ret = check_create_kcp_timer(client);
        check(ret >= 0, "check_create_kcp_timer");

        debug("shake hand successfully conv: %d.", client->udp.kcp->conv);
        return;
    }

    ikcp_input(client->udp.kcp, buf, len);
 
    //kcpev_timer_repeat(client);

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
    struct sockaddr_storage client_addr;

    char buf[KCPEV_BUFFER_SIZE];
    Kcpev *client = w->data;
    int ret = -1;

    ssize_t len = recv(w->fd, buf, sizeof(buf), 0);
    kcpev_timer_repeat(client);
    check(len > 0, "client tcp closed");
 
    char uuids[UUID_PARSE_SIZE];
    uuid_unparse(client->key.uuid, uuids);

    socklen_t addr_size = sizeof(client_addr);
    getpeername(client->tcp.sock, (struct sockaddr*)&client_addr, &addr_size);

    len = on_tcp_recv(client, buf, len);

    //debug("tcp recv from client[%s]: [%d]", uuids, len);

    if(len > 0)
    {
        ret = on_server_recv(client->server, client, buf, len, (struct sockaddr*)&client_addr, addr_size);
        check(ret == 0, "");
    }
    
    while((len = get_tcp_buf_chunk(client, buf, sizeof(buf))))
    {
        ret = on_server_recv(client->server, client, buf, len, (struct sockaddr*)&client_addr, addr_size);
        check(ret == 0, "");
    }
    return;

error:
    close_client(client);
    return;
}

void server_udp_recv(EV_P_ struct ev_io *w, int revents)
{
    char buf[KCPEV_BUFFER_SIZE];
    Kcpev *client = w->data;

    ssize_t len = recv(w->fd, buf, sizeof(buf), 0);
    check(len > 0, "");

    char uuids[UUID_PARSE_SIZE];
    uuid_unparse(client->key.uuid, uuids);

    //debug("udp recv from client[%s : %d]: [%d]", uuids, client->udp.kcp->conv, len);

    ikcp_input(client->udp.kcp, buf, len);
 
    //kcpev_timer_repeat(client);

    int ret = try_kcp_recv(client);
error:
    return;
}

// try create udp connection
int connect_client_udp(Kcpev *client, char *port, struct sockaddr *addr, socklen_t addr_size, \
    struct ev_loop *loop)
{
    int ret = kcpev_bind_udp(&client->udp, port, addr->sa_family, 1);
    check(ret >= 0, "server udp bind");

    ret = connect(client->udp.sock, addr, addr_size);
    check(ret >= 0, "connect client udp");

    ret = kcpev_create_kcp(&client->udp, client->key.split_key.conv, 2);
    check(ret >= 0, "client udp create kcp");

    ret = kcpev_init_ev(client, loop, client, NULL, server_udp_recv);
    check(ret >= 0, "client init ev");

    ret = check_create_kcp_timer(client);
    check(ret >= 0, "check_create_kcp_timer");
    return 0;

error:
    KcpevUdp_destroy(loop, &client->udp);
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

    client->server = server;
    client->tcp.sock = client_sock;
    uuid_generate(client->key.uuid);

    // create hashtable for client kcpev
    HASH_ADD(hh, server->hash, key, sizeof(KcpevKey), client);

    char uuids[UUID_PARSE_SIZE];
    uuid_unparse(client->key.uuid, uuids);
    debug("accept client [%s : %s : %s]\n", hbuf, sbuf, uuids);

	// set up tcp ev
    ret = kcpev_init_ev(client, loop, client, server_tcp_recv, NULL);
    check(ret >= 0, "client init ev");

    // shake hand
    kcpev_send_command(client, COMMAND_SHAKE_HAND, (char *)&client->key, sizeof(KcpevKey));

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
    char buf[KCPEV_BUFFER_SIZE];
    int ret = -1;

    ssize_t len = recvfrom(w->fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client_addr, &addr_size);
    check(len > 0, "server recvfrom");

    KcpevServer *server = w->data;

    ret = on_server_recv(server, NULL, buf, len, (struct sockaddr *)&client_addr, addr_size);
    check(ret == 0, "");

    //debug("udp_all recv client: [%d]\n", len);

error:
    return;
}

int kcpev_set_ev(struct ev_loop *loop, void *data, KcpevSock *evs, ev_io_callback cb)
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
	int ret = -1;

	if(tcp_cb)
	{
		ret = kcpev_set_ev(loop, data, (KcpevSock *)&kcpev->tcp, tcp_cb);
		check(ret >= 0, "set tcp ev");
	}

    if(udp_cb)
    {
        ret = kcpev_set_ev(loop, data, (KcpevSock *)&kcpev->udp, udp_cb);
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

size_t pack_send_buf(char *buf, uint32_t buf_size, uint8_t command, const char *msg, size_t len)
{
    KcpevHeader header;
    int ret = -1;
    const uint32_t header_size = sizeof(KcpevHeader);
    const uint32_t real_size = len + header_size;

    check(buf_size >= real_size && len >= 0, "buf exceed max size, buf size: %d, max size: %d", \
        len, buf_size - header_size);

    header.size = real_size;
    header.command = command;
    ret = header_to_net(&header, buf, buf_size);
    check(ret == 0, "header_to_net");

    memcpy(buf + header_size, msg, len);
    return real_size;

error:
    return 0;
}

int sock_send_command(int sock, uint8_t command, const char *msg, size_t len)
{
    char buf[KCPEV_BUFFER_SIZE];
    int real_size = pack_send_buf(buf, sizeof(buf), command, msg, len);
    check(real_size > 0, "pack_send_buf");

    return send(sock, buf, real_size, 0);

error:
    return -1;
}

int kcpev_send_command(Kcpev *kcpev, uint8_t command, const char *msg, size_t len)
{
    char buf[KCPEV_BUFFER_SIZE];
    size_t real_size = pack_send_buf(buf, sizeof(buf), command, msg, len);
    check(real_size > 0, "pack_send_buf");

    // 如果kcp能用，就用kcp来发消息，否则用tcp
    if(is_kcp_valid(kcpev))
    {
        int ret = ikcp_send(kcpev->udp.kcp, buf, real_size);
        //kcpev_timer_repeat(kcpev);
        return ret;
    }
    else
    {
        return send(kcpev->tcp.sock, buf, real_size, 0);
    }

error:
    return -1;
}

int kcpev_send(Kcpev *kcpev, const char *msg, size_t len)
{
    return kcpev_send_command(kcpev, COMMAND_DATA, msg, len);
}

int kcpev_send_tcp(Kcpev *kcpev, const char *msg, size_t len)
{
    return sock_send_command(kcpev->tcp.sock, COMMAND_DATA, msg, len);
}

void kcpev_set_cb(Kcpev *kcpev, kcpev_recv_cb recv_cb, kcpev_disconnect_cb disconnect_cb)
{
   kcpev->recv_cb = recv_cb; 
}

void kcpev_server_set_cb(KcpevServer *kcpev, kcpev_server_recv_cb recv_cb, kcpev_server_disconnect_cb disconnect_cb)
{
    kcpev->recv_cb = recv_cb;
}

int header_to_net(KcpevHeader *header, char *buf, size_t len)
{
    if(len < sizeof(KcpevHeader))
        return -1;

    *(HEADER_SIZE_TYPE *)(buf) = htonl(header->size);
    *(HEADER_COMMAND_TYPE *)(buf + sizeof(HEADER_SIZE_TYPE)) = header->command;
    return 0;
}

int header_from_net(KcpevHeader *header, const char *buf, size_t len)
{
    if(len < sizeof(KcpevHeader))
        return -1;

    header->size = ntohl(*(const HEADER_SIZE_TYPE *)buf);
    header->command = *(HEADER_COMMAND_TYPE *)(buf + sizeof(HEADER_SIZE_TYPE));
    return 0;
}

