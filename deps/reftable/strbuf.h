/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#ifndef SLICE_H
#define SLICE_H

#ifdef REFTABLE_STANDALONE

#include "basics.h"

/*
  Provides a bounds-checked, growable byte ranges. To use, initialize as "strbuf
  x = STRBUF_INIT;"
 */
struct strbuf {
	size_t len;
	size_t cap;
	char *buf;

	/* Used to enforce initialization with STRBUF_INIT */
	uint8_t canary;
};

#define STRBUF_CANARY 0x42
#define STRBUF_INIT                       \
	{                                 \
		0, 0, NULL, STRBUF_CANARY \
	}

void strbuf_addstr(struct strbuf *dest, const char *src);

/* Deallocate and clear strbuf */
void strbuf_release(struct strbuf *strbuf);

/* Set strbuf to 0 length, but retain buffer. */
void strbuf_reset(struct strbuf *strbuf);

/* Initializes a strbuf. Accepts a strbuf with random garbage. */
void strbuf_init(struct strbuf *strbuf, size_t alloc);

/* Return `buf`, clearing out `s`. Optionally return len (not cap) in `sz`.  */
char *strbuf_detach(struct strbuf *s, size_t *sz);

/* Set length of the slace to `l`, but don't reallocated. */
void strbuf_setlen(struct strbuf *s, size_t l);

/* Ensure `l` bytes beyond current length are available */
void strbuf_grow(struct strbuf *s, size_t l);

/* Signed comparison */
int strbuf_cmp(const struct strbuf *a, const struct strbuf *b);

/* Append `data` to the `dest` strbuf.  */
int strbuf_add(struct strbuf *dest, const void *data, size_t sz);

/* Append `add` to `dest. */
void strbuf_addbuf(struct strbuf *dest, struct strbuf *add);

#else

#include "../git-compat-util.h"
#include "../strbuf.h"

#endif

extern struct strbuf reftable_empty_strbuf;

/* Like strbuf_add, but suitable for passing to reftable_new_writer
 */
int strbuf_add_void(void *b, const void *data, size_t sz);

/* Find the longest shared prefix size of `a` and `b` */
int common_prefix_size(struct strbuf *a, struct strbuf *b);

#endif
