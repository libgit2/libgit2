/*
 * posix.h - OS agnostic POSIX calls
 */
#ifndef INCLUDE_posix_h__
#define INCLUDE_posix_h__

#include "common.h"
#include <fcntl.h>
#include <time.h>

#ifdef GIT_WIN32
#	include "win32/posix.h"
#else
#	include "unix/posix.h"
#endif

#define S_IFGITLINK 0160000
#define S_ISGITLINK(m) (((m) & S_IFMT) == S_IFGITLINK)

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

typedef int git_file;


/**
 * Standard POSIX Methods
 *
 * All the methods starting with the `p_` prefix are
 * direct ports of the standard POSIX methods. 
 *
 * Some of the methods are slightly wrapped to provide
 * saner defaults. Some of these methods are emulated
 * in Windows platforns.
 *
 * Use your manpages to check the docs on these.
 * Straightforward 
 */
extern int p_open(const char *path, int flags);
extern int p_creat(const char *path, int mode);
extern int p_read(git_file fd, void *buf, size_t cnt);
extern int p_write(git_file fd, void *buf, size_t cnt);
extern int p_getcwd(char *buffer_out, size_t size);

#define p_lseek(f,n,w) lseek(f, n, w)
#define p_stat(p,b) stat(p, b)
#define p_fstat(f,b) fstat(f, b)
#define p_chdir(p) chdir(p)
#define p_rmdir(p) rmdir(p)
#define p_chmod(p,m) chmod(p, m)
#define p_close(fd) close(fd)

#endif
