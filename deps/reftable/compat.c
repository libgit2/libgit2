/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd

*/

/* compat.c - compatibility functions for standalone compilation */

#include "system.h"
#include "basics.h"

#ifdef REFTABLE_STANDALONE

#include <dirent.h>

void put_be32(void *p, uint32_t i)
{
	uint8_t *out = (uint8_t *)p;

	out[0] = (uint8_t)((i >> 24) & 0xff);
	out[1] = (uint8_t)((i >> 16) & 0xff);
	out[2] = (uint8_t)((i >> 8) & 0xff);
	out[3] = (uint8_t)((i)&0xff);
}

uint32_t get_be32(uint8_t *in)
{
	return (uint32_t)(in[0]) << 24 | (uint32_t)(in[1]) << 16 |
	       (uint32_t)(in[2]) << 8 | (uint32_t)(in[3]);
}

void put_be64(void *p, uint64_t v)
{
	uint8_t *out = (uint8_t *)p;
	int i = sizeof(uint64_t);
	while (i--) {
		out[i] = (uint8_t)(v & 0xff);
		v >>= 8;
	}
}

uint64_t get_be64(void *out)
{
	uint8_t *bytes = (uint8_t *)out;
	uint64_t v = 0;
	int i = 0;
	for (i = 0; i < sizeof(uint64_t); i++) {
		v = (v << 8) | (uint8_t)(bytes[i] & 0xff);
	}
	return v;
}

uint16_t get_be16(uint8_t *in)
{
	return (uint32_t)(in[0]) << 8 | (uint32_t)(in[1]);
}

char *xstrdup(const char *s)
{
	int l = strlen(s);
	char *dest = (char *)reftable_malloc(l + 1);
	strncpy(dest, s, l + 1);
	return dest;
}

void sleep_millisec(int millisecs)
{
	usleep(millisecs * 1000);
}

void reftable_clear_dir(const char *dirname)
{
	DIR *dir = opendir(dirname);
	struct dirent *ent = NULL;
	assert(dir);
	while ((ent = readdir(dir)) != NULL) {
		unlinkat(dirfd(dir), ent->d_name, 0);
	}
	closedir(dir);
	rmdir(dirname);
}

#else

#include "../dir.h"

void reftable_clear_dir(const char *dirname)
{
	struct strbuf path = STRBUF_INIT;
	strbuf_addstr(&path, dirname);
	remove_dir_recursively(&path, 0);
	strbuf_release(&path);
}

#endif

int hash_size(uint32_t id)
{
	switch (id) {
	case 0:
	case SHA1_ID:
		return SHA1_SIZE;
	case SHA256_ID:
		return SHA256_SIZE;
	}
	abort();
}
