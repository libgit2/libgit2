#ifndef INCLUDE_util_h__
#define INCLUDE_util_h__

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

/* 
 * Don't wrap malloc/calloc.
 * Use the default versions in glibc, and make
 * sure that any methods that allocate memory
 * return a GIT_ENOMEM error when allocation
 * fails.
 */
#define git__malloc malloc
#define git__calloc calloc
#define git__strdup strdup

extern int git__fmt(char *, size_t, const char *, ...)
	GIT_FORMAT_PRINTF(3, 4);
extern int git__prefixcmp(const char *str, const char *prefix);
extern int git__suffixcmp(const char *str, const char *suffix);

extern int git__dirname(char *dir, size_t n, char *path);
extern int git__basename(char *base, size_t n, char *path);

extern void git__hexdump(const char *buffer, size_t n);
extern uint32_t git__hash(const void *key, int len, uint32_t seed);


/** @return true if p fits into the range of a size_t */
GIT_INLINE(int) git__is_sizet(off_t p)
{
	size_t r = (size_t)p;
	return p == (off_t)r;
}

/* 32-bit cross-platform rotl */
#ifdef _MSC_VER /* use built-in method in MSVC */
#	define git__rotl(v, s) (uint32_t)_rotl(v, s)
#else /* use bitops in GCC; with o2 this gets optimized to a rotl instruction */
#	define git__rotl(v, s) (uint32_t)(((uint32_t)(v) << (s)) | ((uint32_t)(v) >> (32 - (s))))
#endif

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

#endif /* INCLUDE_util_h__ */
