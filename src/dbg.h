#ifndef __dbg_h__
#define __dbg_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define INFO_STR "%s (%d): %s: "

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "[DEBUG] " INFO_STR M "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...) fprintf(stderr, "[ERROR] (" INFO_STR "errno: %s) " M "\n", __FILE__, __LINE__, __FUNCTION__, clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr, "[WARN] (" INFO_STR "errno: %s) " M "\n", __FILE__, __LINE__, __FUNCTION__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (" INFO_STR ") " M "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define check(A, M, ...) do {if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto error; }} while (0)

#define sentinel(M, ...)  do { log_err(M, ##__VA_ARGS__); errno=0; goto error; } while (0)

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...) do {if(!(A)) { debug(M, ##__VA_ARGS__); errno=0; goto error; }} while (0)

#define check_silently(A) do {if(!(A)) { goto error; }} while (0)

#define check_goto(A, M, G, ...) do {if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto G; }} while (0)

#define check_log(A, M, ...) do {if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; }} while (0)

#endif

