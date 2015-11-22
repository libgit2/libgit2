#include "pool.h"
#include "posix.h"
#ifndef GIT_WIN32
#include <unistd.h>
#endif

struct git_pool_page {
	git_pool_page *next;
	uint32_t size;
	uint32_t avail;
	char data[GIT_FLEX_ARRAY];
};

static void *pool_alloc_page(git_pool *pool, uint32_t size);

uint32_t git_pool__system_page_size(void)
{
	static uint32_t size = 0;

	if (!size) {
		size_t page_size;
		if (git__page_size(&page_size) < 0)
			page_size = 4096;
		/* allow space for malloc overhead */
		size = page_size - (2 * sizeof(void *)) - sizeof(git_pool_page);
	}

	return size;
}

#if 0
void git_pool_clear(git_pool *pool)
{
	git_pool_page *scan, *next;

	for (scan = pool->pages; scan != NULL; scan = next) {
		next = scan->next;
		git__free(scan);
	}

	pool->pages = NULL;
}
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

uint32_t git_pool__open_pages(git_pool *pool)
{
	uint32_t ct = 0;
	git_pool_page *scan;
	for (scan = pool->pages; scan != NULL; scan = scan->next) ct++;
	return ct;
}

bool git_pool__ptr_in_pool(git_pool *pool, void *ptr)
{
	git_pool_page *scan;
	for (scan = pool->pages; scan != NULL; scan = scan->next)
		if ((void *)scan->data <= ptr &&
			(void *)(((char *)scan->data) + scan->size) > ptr)
			return true;
	return false;
}
