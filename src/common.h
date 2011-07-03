#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "git2/common.h"
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
# include "win32/msvc-compat.h"
# include "win32/mingw-compat.h"
# ifdef GIT_THREADS
#  include "win32/pthread.h"
#endif

# define snprintf _snprintf

typedef SSIZE_T ssize_t;

#else
# include <unistd.h>

# ifdef GIT_THREADS
#  include <pthread.h>
# endif
#endif

#include "git2/types.h"
#include "git2/errors.h"
#include "thread-utils.h"
#include "bswap.h"

extern int git__throw(int error, const char *, ...) GIT_FORMAT_PRINTF(2, 3);
extern int git__rethrow(int error, const char *, ...) GIT_FORMAT_PRINTF(2, 3);

#include "util.h"

#endif /* INCLUDE_common_h__ */
