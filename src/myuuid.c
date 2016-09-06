#include "myuuid.h"

#ifdef WIN32
#include <objbase.h>
#include <stdint.h>
#include <stdio.h>

struct uuid {
    uint32_t    time_low;
    uint16_t    time_mid;
    uint16_t    time_hi_and_version;
    uint16_t    clock_seq;
    uint8_t node[6];
};

static const char *fmt_lower =
"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";

static const char *fmt_upper =
"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X";

#ifdef UUID_UNPARSE_DEFAULT_UPPER
#define FMT_DEFAULT fmt_upper
#else
#define FMT_DEFAULT fmt_lower
#endif


void uuid_unpack(const myuuid_t in, struct uuid *uu)
{
    const uint8_t   *ptr = in;
    uint32_t        tmp;

    tmp = *ptr++;
    tmp = (tmp << 8) | *ptr++;
    tmp = (tmp << 8) | *ptr++;
    tmp = (tmp << 8) | *ptr++;
    uu->time_low = tmp;

    tmp = *ptr++;
    tmp = (tmp << 8) | *ptr++;
    uu->time_mid = tmp;

    tmp = *ptr++;
    tmp = (tmp << 8) | *ptr++;
    uu->time_hi_and_version = tmp;

    tmp = *ptr++;
    tmp = (tmp << 8) | *ptr++;
    uu->clock_seq = tmp;

    memcpy(uu->node, ptr, 6);
}


static void uuid_unparse_x(const myuuid_t uu, char *out, const char *fmt)
{
    struct uuid uuid;

    uuid_unpack(uu, &uuid);
    sprintf(out, fmt,
        uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
        uuid.clock_seq >> 8, uuid.clock_seq & 0xFF,
        uuid.node[0], uuid.node[1], uuid.node[2],
        uuid.node[3], uuid.node[4], uuid.node[5]);
}

void uuid_unparse_lower(const myuuid_t uu, char *out)
{
    uuid_unparse_x(uu, out, fmt_lower);
}

void uuid_unparse_upper(const myuuid_t uu, char *out)
{
    uuid_unparse_x(uu, out, fmt_upper);
}

void uuid_unparse(const myuuid_t uu, char *out)
{
    uuid_unparse_x(uu, out, FMT_DEFAULT);
}

void uuid_generate(myuuid_t out)
{
    CoCreateGuid((GUID *)out);
}

#endif  // define WIN32
