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

#include "kcpev_ringbuf.h"

ringbuf *ringbuf_new(size_t sz) {
  ringbuf *r = malloc(sizeof(*r) + sz);
  if (r == NULL) {
    fprintf(stderr,"out of memory\n");
    goto done;
  }

  r->u = r->i = r->o = 0;
  r->n = sz;

 done:
  return r;
}

void ringbuf_free(ringbuf* r) {
  free(r);
}

/* copy data in. fails if ringbuf has insuff space. */
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
int ringbuf_put(ringbuf *r, const void *_data, size_t len) {
  char *data = (char*)_data;
  size_t a,b,c;
  if (r->i < r->o) {  // available space is a contiguous buffer
    a = r->o - r->i; 
    assert(a == r->n - r->u);
    if (len > a) return -1;
    memcpy(&r->d[r->i], data, len);
  } else {            // available space wraps; it's two buffers
    b = r->n - r->i;  // in-head to eob (receives leading input)
    c = r->o;         // out-head to in-head (receives trailing input)
    a = b + c;        // available space
    // the only ambiguous case is i==o, that's why u is needed
    if (r->i == r->o) a = r->n - r->u; 
    assert(a == r->n - r->u);
    if (len > a) return -1;
    memcpy(&r->d[r->i], data, MIN(b, len));
    if (len > b) memcpy(r->d, &data[b], len-b);
  }
  r->i = (r->i + len) % r->n;
  r->u += len;
  return 0;
}

size_t ringbuf_get_pending_size(ringbuf *r) {
  return r->u;
}

size_t ringbuf_get_next_chunk(ringbuf *r, char **data) {
  // in this case the next chunk is the whole pending buffer
  if (r->o < r->i) {
    assert(r->u == r->i - r->o);
    *data = &r->d[r->o];
    return r->u;
  }
  // in this case (i==o) either the buffer is empty of full.
  // r->u tells distinguishes these cases.
  if ((r->o == r->i) && (r->u == 0)) { *data=NULL; return 0; }
  // if we're here, that means r->o > r->i. the pending
  // output is wrapped around the buffer. this function 
  // returns the chunk prior to eob. the next call gets
  // the next chunk that's wrapped around the buffer.
  size_t b,c;
  b = r->n - r->o; // length of the part we're returning
  c = r->i;        // wrapped part length- a sanity check
  assert(r->u == b + c);
  *data = &r->d[r->o];
  return b;
}

void ringbuf_mark_consumed(ringbuf *r, size_t len) {
  assert(len <= r->u);
  r->o = (r->o + len ) % r->n;
  r->u -= len;
}

void ringbuf_clear(ringbuf *r) {
  r->u = r->i = r->o = 0;
}

// not consuming the data, just copy len data
int ringbuf_copy_data(ringbuf *r, void *data, size_t len)
{
    if(r->u < len)
        return -1;

    // continuous buf
    size_t b = r->n - r->o;
    if(r->o < r->i || b >= len) {
        memcpy(data, &r->d[r->o], len);
    }
    else {
        memcpy(data, &r->d[r->o], b);
        memcpy(data + b, r->d, len - b); 
    }

    return 0;
}

