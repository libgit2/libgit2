#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

/** Force 64 bit off_t size on POSIX. */
#define _FILE_OFFSET_BITS 64

#if defined(_WIN32) && !defined(__CYGWIN__)
#define GIT_WIN32 1
#endif

#include "git2/thread-utils.h"
#include "cc-compat.h"

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
# include "msvc-compat.h"
# include "mingw-compat.h"
# ifdef GIT_THREADS
#  include "win32/pthread.h"
#endif

# define snprintf _snprintf

typedef SSIZE_T ssize_t;

#else
# include <unistd.h>
# include <arpa/inet.h>

# ifdef GIT_THREADS
#  include <pthread.h>
# endif
#endif

#include "git2/common.h"
#include "git2/types.h"
#include "util.h"
#include "thread-utils.h"
#include "bswap.h"

#define GIT_PATH_MAX 4096

#endif /* INCLUDE_common_h__ */
