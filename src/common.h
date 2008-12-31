#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include "cc-compat.h"
#include "util.h"
#include "errors.h"

#ifdef GIT_HAS_PTHREAD
# include <pthread.h>
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "git/common.h"

#define GIT_PATH_MAX 4096

#endif /* INCLUDE_common_h__ */
