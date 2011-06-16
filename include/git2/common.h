/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef INCLUDE_git_common_h__
#define INCLUDE_git_common_h__

#include "thread-utils.h"
#include <time.h>
#include <stdlib.h>

#ifdef __cplusplus
# define GIT_BEGIN_DECL  extern "C" {
# define GIT_END_DECL    }
#else
  /** Start declarations in C mode */
# define GIT_BEGIN_DECL  /* empty */
  /** End declarations in C mode */
# define GIT_END_DECL    /* empty */
#endif

/** Declare a public function exported for application use. */
#ifdef __GNUC__
# define GIT_EXTERN(type) extern \
			  __attribute__((visibility("default"))) \
			  type
#elif defined(_MSC_VER)
# define GIT_EXTERN(type) __declspec(dllexport) type
#else
# define GIT_EXTERN(type) extern type
#endif

/** Declare a public TLS symbol exported for application use. */
#ifdef __GNUC__
# define GIT_EXTERN_TLS(type) extern \
			      __attribute__((visibility("default"))) \
			      GIT_TLS \
			      type
#elif defined(_MSC_VER)
# define GIT_EXTERN_TLS(type) __declspec(dllexport) GIT_TLS type
#else
# define GIT_EXTERN_TLS(type) extern GIT_TLS type
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

#if defined(_WIN32) && !defined(__CYGWIN__)
#define GIT_WIN32 1
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
 * The maximum length of a git valid git path.
 */
#define GIT_PATH_MAX 4096

typedef struct {
	char **strings;
	size_t count;
} git_strarray;

GIT_EXTERN(void) git_strarray_free(git_strarray *array);

/**
 * Return the version of the libgit2 library
 * being currently used.
 *
 * @param major Store the major version number
 * @param minor Store the minor version number
 * @param rev Store the revision (patch) number
 */
GIT_EXTERN(void) git_libgit2_version(int *major, int *minor, int *rev);

/** @} */
GIT_END_DECL
#endif
