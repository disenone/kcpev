/***********************************************************
Copyright (c) 2005-2014, Troy D. Hanson    http://troydhanson.github.com/uthash/
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************/

#ifndef _RINGBUF_H_
#define _RINGBUF_H_
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* simple ring buffer */

typedef struct _ringbuf {
    size_t n; /* allocd size */
    size_t u; /* used space */
    size_t i; /* input pos */
    size_t o; /* output pos */
    char *d; /* C99 flexible array member */
} ringbuf;

ringbuf *ringbuf_new(size_t sz);
int ringbuf_put(ringbuf *r, const void *data, size_t len);
size_t ringbuf_get_pending_size(ringbuf *r);
size_t ringbuf_get_next_chunk(ringbuf *r, char **data);
void ringbuf_mark_consumed(ringbuf *r, size_t len);
void ringbuf_free(ringbuf *r);
void ringbuf_clear(ringbuf *r);
int ringbuf_copy_data(ringbuf *r, void *data, size_t len);
#ifdef __cplusplus
}
#endif
#endif /* _RINGBUF_H_ */
