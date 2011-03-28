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

/**
 * @file git2/common.h
 * @brief Git common platform definitions
 * @defgroup git_common Git common platform definitions
 * @ingroup Git
 * @{
 */

/** Operation completed successfully. */
#define GIT_SUCCESS 0

/**
 * Operation failed, with unspecified reason.
 * This value also serves as the base error code; all other
 * error codes are subtracted from it such that all errors
 * are < 0, in typical POSIX C tradition.
 */
#define GIT_ERROR -1

/** Input was not a properly formatted Git object id. */
#define GIT_ENOTOID (GIT_ERROR - 1)

/** Input does not exist in the scope searched. */
#define GIT_ENOTFOUND (GIT_ERROR - 2)

/** Not enough space available. */
#define GIT_ENOMEM (GIT_ERROR - 3)

/** Consult the OS error information. */
#define GIT_EOSERR (GIT_ERROR - 4)

/** The specified object is of invalid type */
#define GIT_EOBJTYPE (GIT_ERROR - 5)

/** The specified object has its data corrupted */
#define GIT_EOBJCORRUPTED (GIT_ERROR - 6)

/** The specified repository is invalid */
#define GIT_ENOTAREPO (GIT_ERROR - 7)

/** The object type is invalid or doesn't match */
#define GIT_EINVALIDTYPE (GIT_ERROR - 8)

/** The object cannot be written because it's missing internal data */
#define GIT_EMISSINGOBJDATA (GIT_ERROR - 9)

/** The packfile for the ODB is corrupted */
#define GIT_EPACKCORRUPTED (GIT_ERROR - 10)

/** Failed to acquire or release a file lock */
#define GIT_EFLOCKFAIL (GIT_ERROR - 11)

/** The Z library failed to inflate/deflate an object's data */
#define GIT_EZLIB (GIT_ERROR - 12)

/** The queried object is currently busy */
#define GIT_EBUSY (GIT_ERROR - 13)

/** The index file is not backed up by an existing repository */
#define GIT_EBAREINDEX (GIT_ERROR - 14)

/** The name of the reference is not valid */
#define GIT_EINVALIDREFNAME (GIT_ERROR - 15)

/** The specified reference has its data corrupted */
#define GIT_EREFCORRUPTED  (GIT_ERROR - 16)

/** The specified symbolic reference is too deeply nested */
#define GIT_ETOONESTEDSYMREF (GIT_ERROR - 17)

/** The pack-refs file is either corrupted or its format is not currently supported */
#define GIT_EPACKEDREFSCORRUPTED (GIT_ERROR - 18)

/** The path is invalid */
#define GIT_EINVALIDPATH (GIT_ERROR - 19)

/** The revision walker is empty; there are no more commits left to iterate */
#define GIT_EREVWALKOVER (GIT_ERROR - 20)

/** The state of the reference is not valid */
#define GIT_EINVALIDREFSTATE (GIT_ERROR - 21)

/** This feature has not been implemented yet */
#define GIT_ENOTIMPLEMENTED (GIT_ERROR - 22)

/** A reference with this name already exists */
#define GIT_EEXISTS (GIT_ERROR - 23)

GIT_BEGIN_DECL

typedef struct {
	char **strings;
	size_t count;
} git_strarray;

GIT_EXTERN(void) git_strarray_free(git_strarray *array);

/** @} */
GIT_END_DECL
#endif
