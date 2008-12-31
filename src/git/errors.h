#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

#include "common.h"
#include "thread-utils.h"

/**
 * @file git/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** The git errno. */
GIT_EXTERN(int) GIT_TLS git_errno;

/**
 * strerror() for the Git library
 * @param num The error code to explain
 * @return a string explaining the error code
 */
GIT_EXTERN(const char *) git_strerror(int num);

/** @} */
GIT_END_DECL
#endif
