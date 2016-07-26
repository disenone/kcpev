#ifndef __KCPEV_H__
#define __KCPEV_H__

#include <stdint.h>
#include <ev.h>
#include <uthash.h>
#include "ikcp.h"

#define INPORT_ANY "0"

typedef struct
{
    int32_t conv;
    int32_t rkey;
} kcpev_key;

// ´æev_io½Ó¿ÚºÍsocket
typedef struct
{
    int sock;
    ev_io *evi;
} kcpev_tcp;

typedef kcpev_tcp kcpev_sock;

typedef struct
{
    int sock;
    ev_io *evi;
    ikcpcb *kcp;
} kcpev_udp;

#define KCPEV_BASE  \
    kcpev_tcp tcp;              \
    kcpev_udp udp;              \
    struct ev_loop *loop;       \

typedef struct
{
    KCPEV_BASE;
    union
    {
        kcpev_key split_key;
        uint64_t merge_key;
    } key;

    UT_hash_handle hh;
} Kcpev;

typedef struct
{
    KCPEV_BASE;
    Kcpev *hash;
} KcpevServer;

typedef void (*ev_io_callback)(EV_P_ ev_io *w, int revents);

#ifdef __cplusplus
extern "C" {
#endif

// interface

int kcpev_init_client(Kcpev **kcpev, struct ev_loop *loop, const char *port);
int kcpev_init_server(KcpevServer **kcpev, struct ev_loop *loop, const char *port, int family, int backlog);

int kcpev_connect(Kcpev *kcpev, const char *addr, const char *port);

#ifdef __cplusplus
}
#endif

#endif

