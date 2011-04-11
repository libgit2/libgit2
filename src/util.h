#ifndef INCLUDE_util_h__
#define INCLUDE_util_h__

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define bitsizeof(x)  (CHAR_BIT * sizeof(x))
#define MSB(x, bits) ((x) & (~0ULL << (bitsizeof(x) - (bits))))

/* 
 * Don't wrap malloc/calloc.
 * Use the default versions in glibc, and make
 * sure that any methods that allocate memory
 * return a GIT_ENOMEM error when allocation
 * fails.
 */
#define git__malloc malloc
#define git__calloc calloc
#define git__realloc realloc
#define git__strdup strdup

extern int git__fmt(char *, size_t, const char *, ...)
	GIT_FORMAT_PRINTF(3, 4);
extern int git__prefixcmp(const char *str, const char *prefix);
extern int git__suffixcmp(const char *str, const char *suffix);

extern int git__strtol32(long *n, const char *buff, const char **end_buf, int base);

/*
 * The dirname() function shall take a pointer to a character string
 * that contains a pathname, and return a pointer to a string that is a
 * pathname of the parent directory of that file. Trailing '/' characters
 * in the path are not counted as part of the path.
 *
 * If path does not contain a '/', then dirname() shall return a pointer to
 * the string ".". If path is a null pointer or points to an empty string,
 * dirname() shall return a pointer to the string "." .
 *
 * The `git__dirname` implementation is thread safe. The returned 
 * string must be manually free'd.
 *
 * The `git__dirname_r` implementation expects a string allocated
 * by the user with big enough size.
 */
extern char *git__dirname(const char *path);
extern int git__dirname_r(char *buffer, size_t bufflen, const char *path);

/*
 * This function returns the basename of the file, which is the last
 * part of its full name given by fname, with the drive letter and
 * leading directories stripped off. For example, the basename of
 * c:/foo/bar/file.ext is file.ext, and the basename of a:foo is foo.
 *
 * Trailing slashes and backslashes are significant: the basename of
 * c:/foo/bar/ is an empty string after the rightmost slash.
 *
 * The `git__basename` implementation is thread safe. The returned 
 * string must be manually free'd.
 *
 * The `git__basename_r` implementation expects a string allocated
 * by the user with big enough size.
 */
extern char *git__basename(const char *path);
extern int git__basename_r(char *buffer, size_t bufflen, const char *path);

extern const char *git__topdir(const char *path);

/**
 * Join two paths together. Takes care of properly fixing the
 * middle slashes and everything
 *
 * The paths are joined together into buffer_out; this is expected
 * to be an user allocated buffer of `GIT_PATH_MAX` size 
 */
extern void git__joinpath_n(char *buffer_out, int npath, ...);

GIT_INLINE(void) git__joinpath(char *buffer_out, const char *path_a, const char *path_b)
{
	git__joinpath_n(buffer_out, 2, path_a, path_b);
}

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

extern char *git__strtok(char *output, char *src, char *delimit);
extern char *git__strtok_keep(char *output, char *src, char *delimit);

#define STRLEN(str) (sizeof(str) - 1)

#define GIT_OID_LINE_LENGTH(header) (STRLEN(header) + 1 + GIT_OID_HEXSZ + 1)

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
