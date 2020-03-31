/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#include "basics.h"

void put_be24(uint8_t *out, uint32_t i)
{
	out[0] = (uint8_t)((i >> 16) & 0xff);
	out[1] = (uint8_t)((i >> 8) & 0xff);
	out[2] = (uint8_t)(i & 0xff);
}

uint32_t get_be24(uint8_t *in)
{
	return (uint32_t)(in[0]) << 16 | (uint32_t)(in[1]) << 8 |
	       (uint32_t)(in[2]);
}

void put_be16(uint8_t *out, uint16_t i)
{
	out[0] = (uint8_t)((i >> 8) & 0xff);
	out[1] = (uint8_t)(i & 0xff);
}

int binsearch(size_t sz, int (*f)(size_t k, void *args), void *args)
{
	size_t lo = 0;
	size_t hi = sz;

	/* invariant: (hi == sz) || f(hi) == true
	   (lo == 0 && f(0) == true) || fi(lo) == false
	 */
	while (hi - lo > 1) {
		size_t mid = lo + (hi - lo) / 2;

		int val = f(mid, args);
		if (val) {
			hi = mid;
		} else {
			lo = mid;
		}
	}

	if (lo == 0) {
		if (f(0, args)) {
			return 0;
		} else {
			return 1;
		}
	}

	return hi;
}

void free_names(char **a)
{
	char **p = a;
	if (p == NULL) {
		return;
	}
	while (*p) {
		reftable_free(*p);
		p++;
	}
	reftable_free(a);
}

int names_length(char **names)
{
	int len = 0;
	char **p = names;
	while (*p) {
		p++;
		len++;
	}
	return len;
}

void parse_names(char *buf, int size, char ***namesp)
{
	char **names = NULL;
	size_t names_cap = 0;
	size_t names_len = 0;

	char *p = buf;
	char *end = buf + size;
	while (p < end) {
		char *next = strchr(p, '\n');
		if (next != NULL) {
			*next = 0;
		} else {
			next = end;
		}
		if (p < next) {
			if (names_len == names_cap) {
				names_cap = 2 * names_cap + 1;
				names = reftable_realloc(
					names, names_cap * sizeof(char *));
			}
			names[names_len++] = xstrdup(p);
		}
		p = next + 1;
	}

	if (names_len == names_cap) {
		names_cap = 2 * names_cap + 1;
		names = reftable_realloc(names, names_cap * sizeof(char *));
	}

	names[names_len] = NULL;
	*namesp = names;
}

int names_equal(char **a, char **b)
{
	while (*a && *b) {
		if (strcmp(*a, *b)) {
			return 0;
		}

		a++;
		b++;
	}

	return *a == *b;
}
