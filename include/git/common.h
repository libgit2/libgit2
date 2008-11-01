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

#ifdef __cplusplus
# define GIT_BEGIN_DECL  extern "C" {
# define GIT_END_DECL    }
#else
  /** Start declarations in C mode */
# define GIT_BEGIN_DECL  /* empty */
  /** End declarations in C mode */
# define GIT_END_DECL    /* empty */
#endif

/**
 * @file git/common.h
 * @brief Git common platform definitions
 * @defgroup git_common Git common platform definitions
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Declare a public function exported for application use. */
#ifdef __GNUC__
# define GIT_EXTERN(type) __attribute__((visibility("default"))) type
#else
# define GIT_EXTERN(type) type
#endif

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

/** A revision traversal pool. */
typedef struct git_revp git_revp;

/** @} */
GIT_END_DECL
#endif
