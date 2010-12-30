#define GIT__NO_HIDE_MALLOC
#include "common.h"
#include <stdarg.h>
#include <stdio.h>

int git__fmt(char *buf, size_t buf_sz, const char *fmt, ...)
{
	va_list va;
	int r;

	va_start(va, fmt);
	r = vsnprintf(buf, buf_sz, fmt, va);
	va_end(va);
	if (r < 0 || ((size_t) r) >= buf_sz)
		return GIT_ERROR;
	return r;
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

int git__dirname(char *dir, size_t n, char *path)
{
	char *s;
	size_t len;

	assert(dir && n > 1);

	if (!path || !*path || (s = strrchr(path, '/')) == NULL) {
		strcpy(dir, ".");
		return 1;
	}

	if (s == path) { /* "/[aaa]" */
		strcpy(dir, "/");
		return 1;
	}

	if ((len = s - path) >= n)
		return GIT_ERROR;

	memcpy(dir, path, len);
	dir[len] = '\0';

	return len;
}

int git__basename(char *base, size_t n, char *path)
{
	char *s;
	size_t len;

	assert(base && n > 1);

	if (!path || !*path) {
		strcpy(base, ".");
		return 1;
	}
	len = strlen(path);

	if ((s = strrchr(path, '/')) == NULL) {
		if (len >= n)
			return GIT_ERROR;
		strcpy(base, path);
		return len;
	}

	if (s == path && len == 1) { /* "/" */
		strcpy(base, "/");
		return 1;
	}

	len -= s - path;
	if (len >= n)
		return GIT_ERROR;

	memcpy(base, s+1, len);
	base[len] = '\0';

	return len;
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
			printf("   ");

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
    k1  = git__rotl(k1,11);\
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
