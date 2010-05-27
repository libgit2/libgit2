#ifndef INCLUDE_git_common_h__
#define INCLUDE_git_common_h__

#include "thread-utils.h"

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
#else
# define GIT_EXTERN(type) extern type
#endif

/** Declare a public TLS symbol exported for application use. */
#ifdef __GNUC__
# define GIT_EXTERN_TLS(type) extern \
			      __attribute__((visibility("default"))) \
			      GIT_TLS \
			      type
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
 * @file git/common.h
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

GIT_BEGIN_DECL

/** A revision traversal pool. */
typedef struct git_revpool git_revpool;

/** @} */
GIT_END_DECL
#endif
