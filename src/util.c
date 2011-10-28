/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include <git2.h>
#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include "posix.h"

#ifdef _MSC_VER
# include <Shlwapi.h>
#endif

void git_libgit2_version(int *major, int *minor, int *rev)
{
	*major = LIBGIT2_VER_MAJOR;
	*minor = LIBGIT2_VER_MINOR;
	*rev = LIBGIT2_VER_REVISION;
}

void git_strarray_free(git_strarray *array)
{
	size_t i;
	for (i = 0; i < array->count; ++i)
		git__free(array->strings[i]);

	git__free(array->strings);
}

int git__fnmatch(const char *pattern, const char *name, int flags)
{
	int ret;

	ret = p_fnmatch(pattern, name, flags);
	switch (ret) {
	case 0:
		return GIT_SUCCESS;
	case FNM_NOMATCH:
		return GIT_ENOMATCH;
	default:
		return git__throw(GIT_EOSERR, "Error trying to match path");
	}
}

int git__strtol64(int64_t *result, const char *nptr, const char **endptr, int base)
{
	const char *p;
	int64_t n, nn;
	int c, ovfl, v, neg, ndig;

	p = nptr;
	neg = 0;
	n = 0;
	ndig = 0;
	ovfl = 0;

	/*
	 * White space
	 */
	while (isspace(*p))
		p++;

	/*
	 * Sign
	 */
	if (*p == '-' || *p == '+')
		if (*p++ == '-')
			neg = 1;

	/*
	 * Base
	 */
	if (base == 0) {
		if (*p != '0')
			base = 10;
		else {
			base = 8;
			if (p[1] == 'x' || p[1] == 'X') {
				p += 2;
				base = 16;
			}
		}
	} else if (base == 16 && *p == '0') {
		if (p[1] == 'x' || p[1] == 'X')
			p += 2;
	} else if (base < 0 || 36 < base)
		goto Return;

	/*
	 * Non-empty sequence of digits
	 */
	for (;; p++,ndig++) {
		c = *p;
		v = base;
		if ('0'<=c && c<='9')
			v = c - '0';
		else if ('a'<=c && c<='z')
			v = c - 'a' + 10;
		else if ('A'<=c && c<='Z')
			v = c - 'A' + 10;
		if (v >= base)
			break;
		nn = n*base + v;
		if (nn < n)
			ovfl = 1;
		n = nn;
	}

Return:
	if (ndig == 0)
		return git__throw(GIT_ENOTNUM, "Failed to convert string to long. Not a number");

	if (endptr)
		*endptr = p;

	if (ovfl)
		return git__throw(GIT_EOVERFLOW, "Failed to convert string to long. Overflow error");

	*result = neg ? -n : n;
	return GIT_SUCCESS;
}

int git__strtol32(int32_t *result, const char *nptr, const char **endptr, int base)
{
	int error = GIT_SUCCESS;
	int32_t tmp_int;
	int64_t tmp_long;

	if ((error = git__strtol64(&tmp_long, nptr, endptr, base)) < GIT_SUCCESS)
		return error;

	tmp_int = tmp_long & 0xFFFFFFFF;
	if (tmp_int != tmp_long)
		return git__throw(GIT_EOVERFLOW, "Failed to convert. '%s' is too large", nptr);

	*result = tmp_int;
	
	return error;
}

void git__strntolower(char *str, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i) {
		str[i] = (char) tolower(str[i]);
	}
}

void git__strtolower(char *str)
{
	git__strntolower(str, strlen(str));
}

int git__prefixcmp(const char *str, const char *prefix)
{
	for (;;) {
		char p = *(prefix++), s;
		if (!p)
			return 0;
		if ((s = *(str++)) != p)
			return s - p;
	}
}

int git__suffixcmp(const char *str, const char *suffix)
{
	size_t a = strlen(str);
	size_t b = strlen(suffix);
	if (a < b)
		return -1;
	return strcmp(str + (a - b), suffix);
}

char *git__strtok(char **end, const char *sep)
{
	char *ptr = *end;

	while (*ptr && strchr(sep, *ptr))
		++ptr;

	if (*ptr) {
		char *start = ptr;
		*end = start + 1;

		while (**end && !strchr(sep, **end))
			++*end;

		if (**end) {
			**end = '\0';
			++*end;
		}

		return start;
	}

	return NULL;
}

void git__hexdump(const char *buffer, size_t len)
{
	static const size_t LINE_WIDTH = 16;

	size_t line_count, last_line, i, j;
	const char *line;

	line_count = (len / LINE_WIDTH);
	last_line = (len % LINE_WIDTH);

	for (i = 0; i < line_count; ++i) {
		line = buffer + (i * LINE_WIDTH);
		for (j = 0; j < LINE_WIDTH; ++j, ++line)
			printf("%02X ", (unsigned char)*line & 0xFF);

		printf("| ");

		line = buffer + (i * LINE_WIDTH);
		for (j = 0; j < LINE_WIDTH; ++j, ++line)
			printf("%c", (*line >= 32 && *line <= 126) ? *line : '.');

		printf("\n");
	}

	if (last_line > 0) {

		line = buffer + (line_count * LINE_WIDTH);
		for (j = 0; j < last_line; ++j, ++line)
			printf("%02X ", (unsigned char)*line & 0xFF);

		for (j = 0; j < (LINE_WIDTH - last_line); ++j)
			printf("	");

		printf("| ");

		line = buffer + (line_count * LINE_WIDTH);
		for (j = 0; j < last_line; ++j, ++line)
			printf("%c", (*line >= 32 && *line <= 126) ? *line : '.');

		printf("\n");
	}

	printf("\n");
}

#ifdef GIT_LEGACY_HASH
uint32_t git__hash(const void *key, int len, unsigned int seed)
{
	const uint32_t m = 0x5bd1e995;
	const int r = 24;
	uint32_t h = seed ^ len;

	const unsigned char *data = (const unsigned char *)key;

	while(len >= 4) {
		uint32_t k = *(uint32_t *)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	switch(len) {
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
			h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}
#else
/*
	Cross-platform version of Murmurhash3
	http://code.google.com/p/smhasher/wiki/MurmurHash3
	by Austin Appleby (aappleby@gmail.com)

	This code is on the public domain.
*/
uint32_t git__hash(const void *key, int len, uint32_t seed)
{

#define MURMUR_BLOCK() {\
	k1 *= c1; \
	k1 = git__rotl(k1,11);\
	k1 *= c2;\
	h1 ^= k1;\
	h1 = h1*3 + 0x52dce729;\
	c1 = c1*5 + 0x7b7d159c;\
	c2 = c2*5 + 0x6bce6396;\
}

	const uint8_t *data = (const uint8_t*)key;
	const int nblocks = len / 4;

	const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);
	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

	uint32_t h1 = 0x971e137b ^ seed;
	uint32_t k1;

	uint32_t c1 = 0x95543787;
	uint32_t c2 = 0x2ad7eb25;

	int i;

	for (i = -nblocks; i; i++) {
		k1 = blocks[i];
		MURMUR_BLOCK();
	}

	k1 = 0;

	switch(len & 3) {
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
			MURMUR_BLOCK();
	}

	h1 ^= len;
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;

	return h1;
}
#endif

/**
 * A modified `bsearch` from the BSD glibc.
 *
 * Copyright (c) 1990 Regents of the University of California.
 * All rights reserved.
 */
void **git__bsearch(const void *key, void **base, size_t nmemb, int (*compar)(const void *, const void *))
{
		int lim, cmp;
		void **p;

		for (lim = nmemb; lim != 0; lim >>= 1) {
				p = base + (lim >> 1);
				cmp = (*compar)(key, *p);
				if (cmp > 0) { /* key > p: move right */
						base = p + 1;
						lim--;
				} else if (cmp == 0) {
						return (void **)p;
				} /* else move left */
		}
		return NULL;
}

/**
 * A strcmp wrapper
 * 
 * We don't want direct pointers to the CRT on Windows, we may
 * get stdcall conflicts.
 */
int git__strcmp_cb(const void *a, const void *b)
{
	const char *stra = (const char *)a;
	const char *strb = (const char *)b;

	return strcmp(stra, strb);
}
