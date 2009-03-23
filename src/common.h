#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
#define GIT_WIN32 1
#endif

#include "git/thread-utils.h"

#ifdef GIT_HAS_PTHREAD
# include <pthread.h>
#endif
#include <inttypes.h>
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

#define snprintf _snprintf

typedef int ssize_t;

#else

# include <unistd.h>
# include <arpa/inet.h>

#endif

#include "cc-compat.h"
#include "git/common.h"
#include "util.h"
#include "thread-utils.h"
#include "errors.h"

#define GIT_PATH_MAX 4096

#endif /* INCLUDE_common_h__ */
