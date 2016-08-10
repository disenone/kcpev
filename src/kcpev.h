#ifndef __KCPEV_H__
#define __KCPEV_H__

#include <stdint.h>
#include <ev.h>
#include <uthash.h>
#include "ikcp.h"
#include "utils.h"

#define INPORT_ANY "0"
#define RECV_LEN 65536
#define NI_MAXHOST 1025
#define NI_MAXSERV 32

#define COMMAND_DATA        0
#define COMMAND_SET_KEY     1
#define COMMAND_SHAKE_HAND  1

typedef struct
{
    int32_t r1;
    int32_t r2;
    int32_t r3;
    int32_t conv;
} kcpev_skey;

typedef union
{
    kcpev_skey split_key;
    uuid_t uuid;
} kcpev_key;

// 存ev_io接口和socket
typedef struct
{
    int sock;
    ev_io *evio;
} kcpev_tcp;

typedef kcpev_tcp kcpev_sock;

typedef struct
{
    int sock;
    ev_io *evio;
    ev_timer *evt;
    ikcpcb *kcp;
} kcpev_udp;

#define KCPEV_BASE  \
    kcpev_tcp tcp;              \
    kcpev_udp udp;              \
    struct ev_loop *loop;       \

typedef struct
{
    KCPEV_BASE;
    kcpev_key key;
    UT_hash_handle hh;
} Kcpev;

typedef struct
{
    KCPEV_BASE;
    char port[10];
    Kcpev *hash;
} KcpevServer;

// 保存所属的 server 和 key，用来找回实际的客户端结构
typedef struct
{
    KcpevServer *server;
    kcpev_key *key;
} KcpevReflect;

typedef void (*ev_io_callback)(EV_P_ ev_io *w, int revents);

#ifdef __cplusplus
extern "C" {
#endif

// interface

Kcpev *kcpev_create_client(struct ev_loop *loop, const char *port, int family);
KcpevServer *kcpev_create_server(struct ev_loop *loop, const char *port, int family, int backlog);

int kcpev_connect(Kcpev *kcpev, const char *addr, const char *port);

int kcpev_init_ev(Kcpev *kcpev, struct ev_loop *loop, void *data, ev_io_callback tcp_cb, ev_io_callback udp_cb);

int kcpev_send(Kcpev *kcpev, char *msg, int len);

#ifdef __cplusplus
}
#endif

#endif

