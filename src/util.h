#ifndef INCLUDE_util_h__
#define INCLUDE_util_h__

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

extern void *git__malloc(size_t);
extern void *git__calloc(size_t, size_t);
extern char *git__strdup(const char *);

#ifndef GIT__NO_HIDE_MALLOC
# define GIT__FORBID_MALLOC do_not_use_malloc_directly

# ifdef malloc
#  undef malloc
# endif
# define malloc(a)          GIT__FORBID_MALLOC

# ifdef calloc
#  undef calloc
# endif
# define calloc(a,b)        GIT__FORBID_MALLOC

# ifdef strdup
#  undef strdup
# endif
# define strdup(a)          GIT__FORBID_MALLOC
#endif

extern int git__fmt(char *, size_t, const char *, ...)
	GIT_FORMAT_PRINTF(3, 4);
extern int git__prefixcmp(const char *str, const char *prefix);
extern int git__suffixcmp(const char *str, const char *suffix);

extern int git__dirname(char *dir, size_t n, char *path);
extern int git__basename(char *base, size_t n, char *path);

/** @return true if p fits into the range of a size_t */
GIT_INLINE(int) git__is_sizet(off_t p)
{
	size_t r = (size_t)p;
	return p == r;
}

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
