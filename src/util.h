/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_util_h__
#define INCLUDE_util_h__

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define bitsizeof(x) (CHAR_BIT * sizeof(x))
#define MSB(x, bits) ((x) & (~0ULL << (bitsizeof(x) - (bits))))
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * Custom memory allocation wrappers
 * that set error code and error message
 * on allocation failure
 */
GIT_INLINE(void *) git__malloc(size_t len)
{
	void *ptr = malloc(len);
	if (!ptr)
		git__throw(GIT_ENOMEM, "Out of memory. Failed to allocate %d bytes.", (int)len);
	return ptr;
}

GIT_INLINE(void *) git__calloc(size_t nelem, size_t elsize)
{
	void *ptr = calloc(nelem, elsize);
	if (!ptr)
		git__throw(GIT_ENOMEM, "Out of memory. Failed to allocate %d bytes.", (int)elsize);
	return ptr;
}

GIT_INLINE(char *) git__strdup(const char *str)
{
	char *ptr = strdup(str);
	if (!ptr)
		git__throw(GIT_ENOMEM, "Out of memory. Failed to duplicate string");
	return ptr;
}

GIT_INLINE(char *) git__strndup(const char *str, size_t n)
{
	size_t length;
	char *ptr;

	length = strlen(str);
	if (n < length)
		length = n;

	ptr = (char*)malloc(length + 1);
	if (!ptr) {
		git__throw(GIT_ENOMEM, "Out of memory. Failed to duplicate string");
		return NULL;
	}

	memcpy(ptr, str, length);
	ptr[length] = '\0';

	return ptr;
}

GIT_INLINE(void *) git__realloc(void *ptr, size_t size)
{
	void *new_ptr = realloc(ptr, size);
	if (!new_ptr)
		git__throw(GIT_ENOMEM, "Out of memory. Failed to allocate %d bytes.", (int)size);
	return new_ptr;
}

#define git__free(ptr) free(ptr)

extern int git__prefixcmp(const char *str, const char *prefix);
extern int git__suffixcmp(const char *str, const char *suffix);

extern int git__strtol32(int32_t *n, const char *buff, const char **end_buf, int base);
extern int git__strtol64(int64_t *n, const char *buff, const char **end_buf, int base);

extern void git__hexdump(const char *buffer, size_t n);
extern uint32_t git__hash(const void *key, int len, uint32_t seed);

/** @return true if p fits into the range of a size_t */
GIT_INLINE(int) git__is_sizet(git_off_t p)
{
	size_t r = (size_t)p;
	return p == (git_off_t)r;
}

/* 32-bit cross-platform rotl */
#ifdef _MSC_VER /* use built-in method in MSVC */
#	define git__rotl(v, s) (uint32_t)_rotl(v, s)
#else /* use bitops in GCC; with o2 this gets optimized to a rotl instruction */
#	define git__rotl(v, s) (uint32_t)(((uint32_t)(v) << (s)) | ((uint32_t)(v) >> (32 - (s))))
#endif

extern char *git__strtok(char **end, const char *sep);

extern void git__strntolower(char *str, size_t len);
extern void git__strtolower(char *str);

extern int git__fnmatch(const char *pattern, const char *name, int flags);

/*
 * Realloc the buffer pointed at by variable 'x' so that it can hold
 * at least 'nr' entries; the number of entries currently allocated
 * is 'alloc', using the standard growing factor alloc_nr() macro.
 *
 * DO NOT USE any expression with side-effect for 'x' or 'alloc'.
 */
#define alloc_nr(x) (((x)+16)*3/2)
#define ALLOC_GROW(x, nr, alloc) \
	do { \
		if ((nr) > alloc) { \
			if (alloc_nr(alloc) < (nr)) \
				alloc = (nr); \
			else \
				alloc = alloc_nr(alloc); \
			x = xrealloc((x), alloc * sizeof(*(x))); \
		} \
	} while (0)

extern void git__tsort(void **dst, size_t size, int (*cmp)(const void *, const void *));
extern void **git__bsearch(const void *key, void **base, size_t nmemb,
	int (*compar)(const void *, const void *));

extern int git__strcmp_cb(const void *a, const void *b);

#endif /* INCLUDE_util_h__ */
