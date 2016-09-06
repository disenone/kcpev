#ifndef __MYUUID_H__
#define __MYUUID_H__

#ifdef WIN32
typedef unsigned char myuuid_t[16];

#ifdef __cplusplus
extern "C" {
#endif
void uuid_generate(myuuid_t out);
void uuid_unparse(const myuuid_t uu, char *out);

#ifdef __cplusplus
}
#endif

#else   // unix and apple

#include <uuid/uuid.h>
typedef uuid_t myuuid_t;

#endif

#endif

