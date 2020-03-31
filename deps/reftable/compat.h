/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#ifndef COMPAT_H
#define COMPAT_H

#include "system.h"

#ifdef REFTABLE_STANDALONE

/* functions that git-core provides, for standalone compilation */
#include <stdint.h>

uint64_t get_be64(void *in);
void put_be64(void *out, uint64_t i);

void put_be32(void *out, uint32_t i);
uint32_t get_be32(uint8_t *in);

uint16_t get_be16(uint8_t *in);

#define ARRAY_SIZE(a) sizeof((a)) / sizeof((a)[0])
#define FREE_AND_NULL(x)          \
	do {                      \
		reftable_free(x); \
		(x) = NULL;       \
	} while (0)
#define QSORT(arr, n, cmp) qsort(arr, n, sizeof(arr[0]), cmp)
#define SWAP(a, b)                              \
	{                                       \
		char tmp[sizeof(a)];            \
		assert(sizeof(a) == sizeof(b)); \
		memcpy(&tmp[0], &a, sizeof(a)); \
		memcpy(&a, &b, sizeof(a));      \
		memcpy(&b, &tmp[0], sizeof(a)); \
	}

char *xstrdup(const char *s);

void sleep_millisec(int millisecs);

#endif
#endif
