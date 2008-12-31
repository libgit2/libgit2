#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "git/thread-utils.h"

#ifdef GIT_HAS_PTHREAD
# include <pthread.h>
#endif
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef PRIuPTR
# define PRIuPTR "lu"
#endif

#include "cc-compat.h"
#include "util.h"
#include "errors.h"

#define GIT_PATH_MAX 4096

#endif /* INCLUDE_common_h__ */
