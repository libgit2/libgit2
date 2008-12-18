/*
 * fileops.h - OS agnostic disk io operations
 *
 * This header describes the strictly internal part of the api
 */
#ifndef INCLUDE_fileops_h__
#define INCLUDE_fileops_h__

/** Force 64 bit off_t size on POSIX. */
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "errors.h"
#include "git/fileops.h"

#define GITFO_BUF_INIT {NULL, 0}

typedef int git_file;
typedef struct stat gitfo_statbuf;
typedef struct gitfo_cache gitfo_cache;

typedef struct {  /* file io buffer  */
	void *data;  /* data bytes   */
	size_t len;  /* data length  */
} gitfo_buf;


#define gitfo_open(path, flags) open(path, flags)
#define gitfo_close(fd) close(fd)

extern int gitfo_read(git_file fd, void *buf, size_t cnt);
extern int gitfo_write(git_file fd, void *buf, size_t cnt);

extern off_t gitfo_size(git_file fd);
#define gitfo_lstat(path, buf) lstat(path, buf)
#define gitfo_fstat(fd, buf) fstat(fd, buf)
#define gitfo_stat(path, buf) stat(path, buf)
#define gitfo_fsync(fd) fsync(fd)

extern int gitfo_read_file(gitfo_buf *obj, const char *path);
extern void gitfo_free_buf(gitfo_buf *obj);

extern gitfo_cache *gitfo_enable_caching(git_file fd, size_t cache_size);
extern int gitfo_write_cached(gitfo_cache *ioc, void *buf, size_t len);
extern int gitfo_flush_cached(gitfo_cache *ioc);
extern int gitfo_close_cached(gitfo_cache *ioc);

#endif /* INCLUDE_fileops_h__ */
