#ifndef __KCPEV_H__
#define __KCPEV_H__

#include <stdint.h>
#include <ev.h>
#include <uthash.h>
#include "kcpev_ringbuf.h"
#include "ikcp.h"
#include "utils.h"

#define INPORT_ANY "0"
#define KCPEV_BUFFER_SIZE 65536
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define UUID_PARSE_SIZE 37

#define HEADER_SIZE_TYPE    uint32_t
#define HEADER_COMMAND_TYPE uint8_t

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
} KcpevSkey;

typedef union
{
    KcpevSkey split_key;
    myuuid_t uuid;
} KcpevKey;

// 存ev_io接口和socket
#define KCPEV_SOCK  \
    int sock;       \
    ev_io *evio

typedef struct
{
    KCPEV_SOCK;
} KcpevSock;

typedef struct
{
    KCPEV_SOCK;
    ringbuf *rb;
} KcpevTcp;

typedef struct
{
    KCPEV_SOCK;
    ev_timer *evt;
    ikcpcb *kcp;
} KcpevUdp;

struct _Kcpev;
struct _KcpevServer;

typedef void (*kcpev_recv_cb)(struct _Kcpev *kcpev, const char *buf, size_t len);
typedef void (*kcpev_server_recv_cb)(struct _KcpevServer *kcpev_server, struct _Kcpev *kcpev, const char *buf, size_t len);

typedef void (*kcpev_disconnect_cb)(struct _Kcpev *kcpev);
typedef void (*kcpev_server_disconnect_cb)(struct _KcpevServer *server, struct _Kcpev *kcpev);

#define KCPEV_BASE  \
    KcpevTcp tcp;              \
    KcpevUdp udp;              \
    struct ev_loop *loop        \

typedef struct _Kcpev
{
    KCPEV_BASE;
    KcpevKey key;
    struct _KcpevServer *server;
    kcpev_recv_cb recv_cb;
    void *data;
    ev_tstamp heartbeat;
    UT_hash_handle hh;
}Kcpev;

typedef struct _KcpevServer
{
    KCPEV_BASE;
    kcpev_server_recv_cb recv_cb;
    char port[10];
    Kcpev *hash;
} KcpevServer;

typedef struct
{
    HEADER_SIZE_TYPE size;
    HEADER_COMMAND_TYPE command;
} KcpevHeader;

typedef void (*ev_io_callback)(EV_P_ ev_io *w, int revents);

// interface

Kcpev *kcpev_create_client(struct ev_loop *loop, const char *port, int family);
KcpevServer *kcpev_create_server(struct ev_loop *loop, const char *port, int family, int backlog);

int kcpev_connect(Kcpev *kcpev, const char *addr, const char *port);

int sock_send_command(int sock, uint8_t command, const char *msg, size_t len);
int kcpev_send_command(Kcpev *kcpev, uint8_t command, const char *msg, size_t len);
int kcpev_send(Kcpev *kcpev, const char *msg, size_t len);
int kcpev_send_tcp(Kcpev *kcpev, const char *msg, size_t len);

void kcpev_set_cb(Kcpev *kcpev, kcpev_recv_cb recv_cb, kcpev_disconnect_cb disconnect_cb);

void kcpev_server_set_cb(KcpevServer *kcpev, kcpev_server_recv_cb recv_cb, kcpev_server_disconnect_cb disconnect_cb);

int header_to_net(KcpevHeader *header, char *buf, size_t len);
int header_from_net(KcpevHeader *header, const char *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif

