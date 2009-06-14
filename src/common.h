#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
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

#ifdef GIT_WIN32

# include <io.h>
# include <direct.h>
# include <windows.h>

# define snprintf _snprintf

# if (defined(_MSC_VER) && defined(_WIN64)) || \
     (defined(__DMC__) && defined(_M_AMD64))
  typedef long long ssize_t;
# else
  typedef int ssize_t;
# endif

# if defined(_MSC_VER)
/* access() mode parameter #defines   */
#  define F_OK 0  /* existence  check */
#  define W_OK 2  /* write mode check */
#  define R_OK 4  /* read  mode check */
# endif

#else

# include <unistd.h>
# include <arpa/inet.h>

#endif

#include "git/common.h"
#include "util.h"
#include "thread-utils.h"
#include "errors.h"

#define GIT_PATH_MAX 4096

#ifndef GIT_HAVE_INTTYPES_H
/* add some missing <stdint.h> typedef's */
typedef long int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif

#endif /* INCLUDE_common_h__ */
