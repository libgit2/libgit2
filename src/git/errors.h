#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

#include "common.h"

/**
 * @file git/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** The git errno. */
#if defined(GIT_TLS)
GIT_EXTERN_TLS(int) git_errno;

#elif defined(GIT_HAS_PTHREAD)
# define git_errno (*git__errno_storage())
GIT_EXTERN(int *) git__errno_storage(void);

#endif

/**
 * strerror() for the Git library
 * @param num The error code to explain
 * @return a string explaining the error code
 */
GIT_EXTERN(const char *) git_strerror(int num);

/** @} */
GIT_END_DECL
#endif
