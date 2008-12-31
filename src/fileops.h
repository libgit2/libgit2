/*
 * fileops.h - OS agnostic disk io operations
 *
 * This header describes the strictly internal part of the api
 */
#ifndef INCLUDE_fileops_h__
#define INCLUDE_fileops_h__

/** Force 64 bit off_t size on POSIX. */
#define _FILE_OFFSET_BITS 64

#include "common.h"
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define GITFO_BUF_INIT {NULL, 0}

typedef int git_file;
typedef struct gitfo_cache gitfo_cache;

typedef struct {  /* file io buffer  */
	void *data;  /* data bytes   */
	size_t len;  /* data length  */
} gitfo_buf;


extern int gitfo_open(const char *path, int flags);
extern int gitfo_creat(const char *path, int mode);
#define gitfo_close(fd) close(fd)

extern int gitfo_read(git_file fd, void *buf, size_t cnt);
extern int gitfo_write(git_file fd, void *buf, size_t cnt);
extern off_t gitfo_size(git_file fd);

extern int gitfo_read_file(gitfo_buf *obj, const char *path);
extern void gitfo_free_buf(gitfo_buf *obj);

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
