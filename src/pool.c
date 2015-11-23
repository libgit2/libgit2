#include "pool.h"
#include "posix.h"
#ifndef GIT_WIN32
#include <unistd.h>
#endif

void git_pool_swap(git_pool *a, git_pool *b)
{
	git_pool temp;

	if (a == b)
		return;

	memcpy(&temp, a, sizeof(temp));
	memcpy(a, b, sizeof(temp));
	memcpy(b, &temp, sizeof(temp));
}

char *git_pool_strndup(git_pool *pool, const char *str, size_t n)
{
	char *ptr = NULL;

	assert(pool && str && pool->item_size == sizeof(char));

	if ((uint32_t)(n + 1) < n)
		return NULL;

	if ((ptr = git_pool_malloc(pool, (uint32_t)(n + 1))) != NULL) {
		memcpy(ptr, str, n);
		ptr[n] = '\0';
	}

	return ptr;
}

char *git_pool_strdup(git_pool *pool, const char *str)
{
	assert(pool && str && pool->item_size == sizeof(char));
	return git_pool_strndup(pool, str, strlen(str));
}

char *git_pool_strdup_safe(git_pool *pool, const char *str)
{
	return str ? git_pool_strdup(pool, str) : NULL;
}

char *git_pool_strcat(git_pool *pool, const char *a, const char *b)
{
	void *ptr;
	size_t len_a, len_b;

	assert(pool && pool->item_size == sizeof(char));

	len_a = a ? strlen(a) : 0;
	len_b = b ? strlen(b) : 0;

	if ((ptr = git_pool_malloc(pool, (uint32_t)(len_a + len_b + 1))) != NULL) {
		if (len_a)
			memcpy(ptr, a, len_a);
		if (len_b)
			memcpy(((char *)ptr) + len_a, b, len_b);
		*(((char *)ptr) + len_a + len_b) = '\0';
	}
	return ptr;
}
