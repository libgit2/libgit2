#define GIT__NO_HIDE_MALLOC
#include "common.h"
#include <stdarg.h>
#include <stdio.h>

void *git__malloc(size_t n)
{
	void *r = malloc(n);
	if (!r)
		return git_ptr_error(GIT_ENOMEM);
	return r;
}

void *git__calloc(size_t a, size_t b)
{
	void *r = calloc(a, b);
	if (!r)
		return git_ptr_error(GIT_ENOMEM);
	return r;
}

char *git__strdup(const char *s)
{
	char *r = strdup(s);
	if (!s)
		return git_ptr_error(GIT_ENOMEM);
	return r;
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

