#ifndef __KCPEV_H__
#define __KCPEV_H__

#include <stdint.h>
#include <ev.h>
#include <uthash.h>
#include "ikcp.h"
#include "utils.h"

#define INPORT_ANY "0"
#define KCPEV_BUFFER_SIZE 65536
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define UUID_PARSE_SIZE 37

#define COMMAND_DATA        0   // 普通数据
#define COMMAND_SHAKE_HAND  1   // 握手
#define COMMAND_HEARTBEAT   2   // 心跳

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint16_t r4;
    uint8_t r5;
    uint8_t conv;
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

struct _Kcpev;
struct _KcpevServer;

typedef void (*kcpev_recv_cb)(struct _Kcpev *kcpev, const char *buf, int len);
typedef void (*kcpev_server_recv_cb)(struct _KcpevServer *kcpev_server, struct _Kcpev *kcpev, const char *buf, int len);

typedef void (*kcpev_disconnect_cb)(struct _Kcpev *kcpev);
typedef void (*kcpev_server_disconnect_cb)(struct _KcpevServer *server, struct _Kcpev *kcpev);

#define KCPEV_BASE  \
    kcpev_tcp tcp;              \
    kcpev_udp udp;              \
    struct ev_loop *loop;       \

struct _Kcpev
{
    KCPEV_BASE;
    kcpev_key key;
    struct _KcpevServer *server;
    kcpev_recv_cb recv_cb;
    void *data;
    ev_tstamp heartbeat;
    UT_hash_handle hh;
};

typedef struct _Kcpev Kcpev;

struct _KcpevServer
{
    KCPEV_BASE;
    kcpev_server_recv_cb recv_cb;
    char port[10];
    Kcpev *hash;
};

typedef struct _KcpevServer KcpevServer;

typedef void (*ev_io_callback)(EV_P_ ev_io *w, int revents);

// interface

Kcpev *kcpev_create_client(struct ev_loop *loop, const char *port, int family);
KcpevServer *kcpev_create_server(struct ev_loop *loop, const char *port, int family, int backlog);

int kcpev_connect(Kcpev *kcpev, const char *addr, const char *port);

int sock_send_command(int sock, uint8_t command, const char *msg, int len);
int kcpev_send_command(Kcpev *kcpev, uint8_t command, const char *msg, int len);
int kcpev_send(Kcpev *kcpev, const char *msg, int len);
int kcpev_send_tcp(Kcpev *kcpev, const char *msg, int len);

void kcpev_set_cb(Kcpev *kcpev, kcpev_recv_cb recv_cb, kcpev_disconnect_cb disconnect_cb);

void kcpev_server_set_cb(KcpevServer *kcpev, kcpev_server_recv_cb recv_cb, kcpev_server_disconnect_cb disconnect_cb);

#ifdef __cplusplus
}
#endif

#endif

