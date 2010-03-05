/*
 * fileops.h - OS agnostic disk io operations
 *
 * This header describes the strictly internal part of the api
 */
#ifndef INCLUDE_fileops_h__
#define INCLUDE_fileops_h__

#include "common.h"
#include "map.h"
#include "dir.h"
#include <fcntl.h>
#include <time.h>

#ifdef GIT_WIN32
GIT_INLINE(int) link(const char *GIT_UNUSED(old), const char *GIT_UNUSED(new))
{
	GIT_UNUSED_ARG(old)
	GIT_UNUSED_ARG(new)
	errno = ENOSYS;
	return -1;
}

GIT_INLINE(int) git__mkdir(const char *path, int GIT_UNUSED(mode))
{
	GIT_UNUSED_ARG(mode)
	return mkdir(path);
}

extern int git__unlink(const char *path);
extern int git__mkstemp(char *template);
extern int git__fsync(int fd);

# ifndef GIT__WIN32_NO_HIDE_FILEOPS
#  define unlink(p) git__unlink(p)
#  define mkstemp(t) git__mkstemp(t)
#  define mkdir(p,m) git__mkdir(p, m)
#  define fsync(fd) git__fsync(fd)
# endif
#endif  /* GIT_WIN32 */


#if !defined(O_BINARY)
#define O_BINARY 0
#endif

#define GITFO_BUF_INIT {NULL, 0}

typedef int git_file;
typedef struct gitfo_cache gitfo_cache;

typedef struct {  /* file io buffer  */
	void *data;  /* data bytes   */
	size_t len;  /* data length  */
} gitfo_buf;

extern int gitfo_exists(const char *path);
extern int gitfo_open(const char *path, int flags);
extern int gitfo_creat(const char *path, int mode);
#define gitfo_close(fd) close(fd)

extern int gitfo_read(git_file fd, void *buf, size_t cnt);
extern int gitfo_write(git_file fd, void *buf, size_t cnt);
#define gitfo_lseek(f,n,w) lseek(f, n, w)
extern off_t gitfo_size(git_file fd);

extern int gitfo_read_file(gitfo_buf *obj, const char *path);
extern void gitfo_free_buf(gitfo_buf *obj);
extern int gitfo_move_file(char *from, char *to);

#define gitfo_stat(p,b) stat(p, b)
#define gitfo_fstat(f,b) fstat(f, b)

#define gitfo_unlink(p) unlink(p)
#define gitfo_rmdir(p) rmdir(p)
#define gitfo_chdir(p) chdir(p)
#define gitfo_mkdir(p,m) mkdir(p, m)

#define gitfo_mkstemp(t) mkstemp(t)
#define gitfo_fsync(fd) fsync(fd)
#define gitfo_chmod(p,m) chmod(p, m)

/**
 * Read-only map all or part of a file into memory.
 * When possible this function should favor a virtual memory
 * style mapping over some form of malloc()+read(), as the
 * data access will be random and is not likely to touch the
 * majority of the region requested.
 *
 * @param out buffer to populate with the mapping information.
 * @param fd open descriptor to configure the mapping from.
 * @param begin first byte to map, this should be page aligned.
 * @param end number of bytes to map.
 * @return
 * - GIT_SUCCESS on success;
 * - GIT_EOSERR on an unspecified OS related error.
 */
extern int gitfo_map_ro(
	git_map *out,
	git_file fd,
	off_t begin,
	size_t len);

/**
 * Release the memory associated with a previous memory mapping.
 * @param map the mapping description previously configured.
 */
extern void gitfo_free_map(git_map *map);

/**
 * Walk each directory entry, except '.' and '..', calling fn(state).
 *
 * @param pathbuf buffer the function reads the initial directory
 * 		path from, and updates with each successive entry's name.
 * @param pathmax maximum allocation of pathbuf.
 * @param fn function to invoke with each entry.  The first arg is
 *		the input state and the second arg is pathbuf.  The function
 *		may modify the pathbuf, but only by appending new text.
 * @param state to pass to fn as the first arg.
 */
extern int gitfo_dirent(
	char *pathbuf,
	size_t pathmax,
	int (*fn)(void *, char *),
	void *state);

extern gitfo_cache *gitfo_enable_caching(git_file fd, size_t cache_size);
extern int gitfo_write_cached(gitfo_cache *ioc, void *buf, size_t len);
extern int gitfo_flush_cached(gitfo_cache *ioc);
extern int gitfo_close_cached(gitfo_cache *ioc);

#endif /* INCLUDE_fileops_h__ */
