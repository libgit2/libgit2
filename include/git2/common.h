/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_common_h__
#define INCLUDE_git_common_h__

#include <time.h>
#include <stdlib.h>

#ifdef _MSC_VER
#	include "inttypes.h"
#else
#	include <inttypes.h>
#endif

#ifdef __cplusplus
# define GIT_BEGIN_DECL extern "C" {
# define GIT_END_DECL	}
#else
 /** Start declarations in C mode */
# define GIT_BEGIN_DECL /* empty */
 /** End declarations in C mode */
# define GIT_END_DECL	/* empty */
#endif

/** Declare a public function exported for application use. */
#if __GNUC__ >= 4
# define GIT_EXTERN(type) extern \
			 __attribute__((visibility("default"))) \
			 type
#elif defined(_MSC_VER)
# define GIT_EXTERN(type) __declspec(dllexport) type
#else
# define GIT_EXTERN(type) extern type
#endif

/** Declare a function as always inlined. */
#if defined(_MSC_VER)
# define GIT_INLINE(type) static __inline type
#else
# define GIT_INLINE(type) static inline type
#endif

/** Declare a function's takes printf style arguments. */
#ifdef __GNUC__
# define GIT_FORMAT_PRINTF(a,b) __attribute__((format (printf, a, b)))
#else
# define GIT_FORMAT_PRINTF(a,b) /* empty */
#endif

#if (defined(_WIN32)) && !defined(__CYGWIN__)
#define GIT_WIN32 1
#endif

#ifdef __amigaos4__
#include <netinet/in.h>
#endif

/**
 * @file git2/common.h
 * @brief Git common platform definitions
 * @defgroup git_common Git common platform definitions
 * @ingroup Git
 * @{
 */

GIT_BEGIN_DECL

/**
 * The separator used in path list strings (ie like in the PATH
 * environment variable). A semi-colon ";" is used on Windows, and
 * a colon ":" for all other systems.
 */
#ifdef GIT_WIN32
#define GIT_PATH_LIST_SEPARATOR ';'
#else
#define GIT_PATH_LIST_SEPARATOR ':'
#endif

/**
 * The maximum length of a valid git path.
 */
#define GIT_PATH_MAX 4096

/**
 * The string representation of the null object ID.
 */
#define GIT_OID_HEX_ZERO "0000000000000000000000000000000000000000"

/**
 * Return the version of the libgit2 library
 * being currently used.
 *
 * @param major Store the major version number
 * @param minor Store the minor version number
 * @param rev Store the revision (patch) number
 */
GIT_EXTERN(void) git_libgit2_version(int *major, int *minor, int *rev);

/**
 * Combinations of these values describe the capabilities of libgit2.
 */
enum {
	GIT_CAP_THREADS			= ( 1 << 0 ),
	GIT_CAP_HTTPS			= ( 1 << 1 )
};

/**
 * Query compile time options for libgit2.
 *
 * @return A combination of GIT_CAP_* values.
 *
 * - GIT_CAP_THREADS
 *   Libgit2 was compiled with thread support. Note that thread support is still to be seen as a
 *   'work in progress'.
 *
 * - GIT_CAP_HTTPS
 *   Libgit2 supports the https:// protocol. This requires the open ssl library to be
 *   found when compiling libgit2.
 */
GIT_EXTERN(int) git_libgit2_capabilities(void);


enum {
	GIT_OPT_GET_MWINDOW_SIZE,
	GIT_OPT_SET_MWINDOW_SIZE,
	GIT_OPT_GET_MWINDOW_MAPPED_LIMIT,
	GIT_OPT_SET_MWINDOW_MAPPED_LIMIT,
	GIT_OPT_GET_SEARCH_PATH,
	GIT_OPT_SET_SEARCH_PATH,
};

/**
 * Set or query a library global option
 *
 * Available options:
 *
 *	opts(GIT_OPT_GET_MWINDOW_SIZE, size_t *):
 *		Get the maximum mmap window size
 *
 *	opts(GIT_OPT_SET_MWINDOW_SIZE, size_t):
 *		Set the maximum mmap window size
 *
 *	opts(GIT_OPT_GET_MWINDOW_MAPPED_LIMIT, size_t *):
 *		Get the maximum memory that will be mapped in total by the library
 *
 *	opts(GIT_OPT_SET_MWINDOW_MAPPED_LIMIT, size_t):
 *		Set the maximum amount of memory that can be mapped at any time
 *		by the library
 *
 *	opts(GIT_OPT_GET_SEARCH_PATH, int level, char *out, size_t len)
 *		Get the search path for a given level of config data.  "level" must
 *		be one of GIT_CONFIG_LEVEL_SYSTEM, GIT_CONFIG_LEVEL_GLOBAL, or
 *		GIT_CONFIG_LEVEL_XDG.  The search path is written to the `out`
 *		buffer up to size `len`.  Returns GIT_EBUFS if buffer is too small.
 *
 *	opts(GIT_OPT_SET_SEARCH_PATH, int level, const char *path)
 *		Set the search path for a level of config data.  The search path
 *		applied to shared attributes and ignore files, too.
 *      - `path` lists directories delimited by GIT_PATH_LIST_SEPARATOR.
 *		  Pass NULL to reset to the default (generally based on environment
 *		  variables).  Use magic path `$PATH` to include the old value
 *		  of the path (if you want to prepend or append, for instance).
 *		- `level` must be GIT_CONFIG_LEVEL_SYSTEM, GIT_CONFIG_LEVEL_GLOBAL,
 *		  or GIT_CONFIG_LEVEL_XDG.
 *
 * @param option Option key
 * @param ... value to set the option
 * @return 0 on success, <0 on failure
 */
GIT_EXTERN(int) git_libgit2_opts(int option, ...);

/** @} */
GIT_END_DECL

#endif
