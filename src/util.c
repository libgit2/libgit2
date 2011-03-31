#define GIT__NO_HIDE_MALLOC
#include "common.h"
#include <stdarg.h>
#include <stdio.h>

void git_strarray_free(git_strarray *array)
{
	size_t i;
	for (i = 0; i < array->count; ++i)
		free(array->strings[i]);

	free(array->strings);
}

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

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git__basename_r(char *buffer, size_t bufflen, const char *path)
{
	const char *endp, *startp;
	int len, result;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		startp  = ".";
		len     = 1;
		goto Exit;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	/* All slashes becomes "/" */
	if (endp == path && *endp == '/') {
		startp = "/";
		len    = 1;
		goto Exit;
	}

	/* Find the start of the base */
	startp = endp;
	while (startp > path && *(startp - 1) != '/')
		startp--;

	len = endp - startp +1;

Exit:
	result = len;
	if (buffer == NULL) {
		return result;
	}
	if (len > (int)bufflen-1) {
		len    = (int)bufflen-1;
		result = GIT_ENOMEM;
	}

	if (len >= 0) {
		memmove(buffer, startp, len);
		buffer[len] = 0;
	}
	return result;
}

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git__dirname_r(char *buffer, size_t bufflen, const char *path)
{
    const char *endp;
    int result, len;

    /* Empty or NULL string gets treated as "." */
    if (path == NULL || *path == '\0') {
        path = ".";
        len  = 1;
        goto Exit;
    }

    /* Strip trailing slashes */
    endp = path + strlen(path) - 1;
    while (endp > path && *endp == '/')
        endp--;

    /* Find the start of the dir */
    while (endp > path && *endp != '/')
        endp--;

    /* Either the dir is "/" or there are no slashes */
    if (endp == path) {
        path = (*endp == '/') ? "/" : ".";
        len  = 1;
        goto Exit;
    }

    do {
        endp--;
    } while (endp > path && *endp == '/');

    len = endp - path +1;

Exit:
    result = len;
    if (len+1 > GIT_PATH_MAX) {
        return GIT_ENOMEM;
    }
    if (buffer == NULL)
        return result;

    if (len > (int)bufflen-1) {
        len    = (int)bufflen-1;
        result = GIT_ENOMEM;
    }

    if (len >= 0) {
        memmove(buffer, path, len);
        buffer[len] = 0;
    }
    return result;
}


char *git__dirname(const char *path)
{
    char *dname = NULL;
    int len;

	len = (path ? strlen(path) : 0) + 2;
	dname = (char *)git__malloc(len);
	if (dname == NULL)
		return NULL;

    if (git__dirname_r(dname, len, path) < GIT_SUCCESS) {
		free(dname);
		return NULL;
	}

    return dname;
}

char *git__basename(const char *path)
{
    char *bname = NULL;
    int len;

	len = (path ? strlen(path) : 0) + 2;
	bname = (char *)git__malloc(len);
	if (bname == NULL)
		return NULL;

    if (git__basename_r(bname, len, path) < GIT_SUCCESS) {
		free(bname);
		return NULL;
	}

    return bname;
}


const char *git__topdir(const char *path)
{
	size_t len;
	int i;

	assert(path);
	len = strlen(path);

	if (!len || path[len - 1] != '/')
		return NULL;

	for (i = len - 2; i >= 0; --i)
		if (path[i] == '/')
			break;

	return &path[i + 1];
}

void git__joinpath_n(char *buffer_out, int count, ...)
{
	va_list ap;
	int i;
	char *buffer_start = buffer_out;

	va_start(ap, count);
	for (i = 0; i < count; ++i) {
		const char *path;
		int len;

		path = va_arg(ap, const char *);

		assert((i == 0) || path != buffer_start);

		if (i > 0 && *path == '/' && buffer_out > buffer_start && buffer_out[-1] == '/')
			path++;

		if (!*path)
			continue;

		len = strlen(path);
		memmove(buffer_out, path, len);
		buffer_out = buffer_out + len;

		if (i < count - 1 && buffer_out[-1] != '/')
			*buffer_out++ = '/';
	}
	va_end(ap);

	*buffer_out = '\0';
}

static char *strtok_raw(char *output, char *src, char *delimit, int keep)
{
	while (*src && strchr(delimit, *src) == NULL)
		*output++ = *src++;

	*output = 0;

	if (keep)
		return src;
	else
		return *src ? src+1 : src;
}

char *git__strtok(char *output, char *src, char *delimit)
{
	return strtok_raw(output, src, delimit, 0);
}

char *git__strtok_keep(char *output, char *src, char *delimit)
{
	return strtok_raw(output, src, delimit, 1);
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
