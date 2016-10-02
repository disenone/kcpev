#ifndef __KCPEV_H__
#define __KCPEV_H__

#include <stdint.h>
#include <ev.h>
#include <uthash.h>
#include "kcpev_ringbuf.h"
#include "ikcp.h"
#include "utils.h"

#define KCPEV_INPORT_ANY        "0"
#define KCPEV_BUFFER_SIZE       65536
#define KCPEV_NI_MAXHOST        1025
#define KCPEV_NI_MAXSERV        32
#define KCPEV_UUID_PARSE_SIZE   37

#define KCPEV_HEADER_SIZE_TYPE      uint32_t
#define KCPEV_HEADER_COMMAND_TYPE   uint8_t

#define KCPEV_KCP_MODE              2
#define KCPEV_HEARTBEAT_TIMEOUT     30

#ifdef _WIN32
#define KCPEV_HANDLE_TO_FD(handle)  (_open_osfhandle (handle, 0))
#define KCPEV_FD_TO_HANDLE(fd)      (_get_osfhandle(fd))
#else
#define KCPEV_HANDLE_TO_FD(handle)  handle
#define KCPEV_FD_TO_HANDLE(fd)      fd
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef double KcpevTimestamp;

enum Command
{
    COMMAND_DATA = 1,           // 普通数据
    COMMAND_SHAKE_HAND1,        // 握手第一次
    COMMAND_SHAKE_HAND2,        // 握手第二次
    COMMAND_HEARTBEAT1,         // 心跳
    COMMAND_HEARTBEAT2,         // 心跳
    COMMAND_UDP_INVALID,        // 设置udp无效
};

enum UdpStatus
{
    UDP_INVALID = 0,            // udp不可用
    UDP_SHAKING_HAND,           // udp握手中
    UDP_READY,                  // udp可用
    UDP_HEARTBEAT,              // 心跳包
};

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
    ev_timer *evt;      // kcp timer
    ev_timer *evh;      // heartbeat timer
    ikcpcb *kcp;
    uint8_t status;     // UdpStatus
    KcpevTimestamp heart;
} KcpevUdp;

struct _Kcpev;
struct _KcpevServer;

typedef void (*kcpev_recv_cb)(struct _Kcpev *kcpev, const char *buf, size_t len);
typedef void (*kcpev_server_recv_cb)(struct _KcpevServer *kcpev_server, struct _Kcpev *kcpev, const char *buf, size_t len);

typedef void (*kcpev_disconnect_cb)(struct _Kcpev *kcpev);
typedef void (*kcpev_server_disconnect_cb)(struct _KcpevServer *server, struct _Kcpev *kcpev);

typedef void (*timer_cb)(EV_P_ ev_timer *w, int revents);

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
    KCPEV_HEADER_SIZE_TYPE size;
    KCPEV_HEADER_COMMAND_TYPE command;
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

