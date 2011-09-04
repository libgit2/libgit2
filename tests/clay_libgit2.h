#ifndef __CLAY_LIBGIT2__
#define __CLAY_LIBGIT2__

#include "clay.h"
#include <git2.h>
#include "common.h"

#define must_be_true(expr) clay_assert(expr, NULL)
#define must_pass(expr) clay_must_pass(expr, NULL)
#define must_fail(expr) clay_must_fail(expr, NULL)

#endif
