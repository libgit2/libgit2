#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

/** Force 64 bit off_t size on POSIX. */
#define _FILE_OFFSET_BITS 64

#if defined(_WIN32) && !defined(__CYGWIN__)
#define GIT_WIN32 1
#endif

#include "git/thread-utils.h"
#include "cc-compat.h"

#ifdef GIT_HAS_PTHREAD
# include <pthread.h>
#endif
#ifdef GIT_HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef GIT_WIN32

# include <io.h>
# include <direct.h>
# include <windows.h>

# define snprintf _snprintf

# if defined(__DMC__)
#  if defined(_M_AMD64)
#   define SSIZE_T long long
#  else
#   define SSIZE_T int
#  endif
# endif

typedef SSIZE_T ssize_t;

# if defined(_MSC_VER)
/* access() mode parameter #defines   */
#  define F_OK 0  /* existence  check */
#  define W_OK 2  /* write mode check */
#  define R_OK 4  /* read  mode check */
# endif

#if defined(__MINGW32__)

# define off_t off64_t
# define lseek _lseeki64
# define stat _stati64
# define fstat _fstati64

#elif defined(_MSC_VER)

typedef __int64 off64_t;
# define off_t off64_t
# define lseek _lseeki64
# define stat _stat64
# define fstat _fstat64

#endif

#else

# include <unistd.h>
# include <arpa/inet.h>

#endif

#include "git/common.h"
#include "util.h"
#include "thread-utils.h"
#include "errors.h"
#include "bswap.h"

#define GIT_PATH_MAX 4096

#ifndef GIT_HAVE_INTTYPES_H
/* add some missing <stdint.h> typedef's */
typedef long int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif

#endif /* INCLUDE_common_h__ */
