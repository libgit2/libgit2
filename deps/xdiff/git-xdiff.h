/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

/*
 * This file provides the necessary indirection between xdiff and
 * libgit2.  libgit2-specific functionality should live here, so
 * that git and libgit2 can share a common xdiff implementation.
 */

#ifndef INCLUDE_git_xdiff_h__
#define INCLUDE_git_xdiff_h__

#include <limits.h>
#include <stdint.h>
#include <stddef.h>

/* Work around C90-conformance issues */
#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
# if defined(_MSC_VER)
#  define inline __inline
# elif defined(__GNUC__)
#  define inline __inline__
# else
#  define inline
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
# define XDL_UNUSED __attribute__((unused))
#else
# define XDL_UNUSED
#endif

#define xdl_malloc(x) malloc(x)
#define xdl_calloc(n, sz) calloc(n, sz)
#define xdl_free(ptr) free(ptr)
#define xdl_realloc(ptr, x) realloc(ptr, x)

#if defined(_DEBUG)
# define XDL_BUG(msg) assert(!msg)
#else
# define XDL_BUG(msg)
#endif

#endif
