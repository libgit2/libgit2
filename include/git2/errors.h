/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

#include "common.h"

/**
 * @file git2/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef enum {
	GIT_SUCCESS = 0,
	GIT_ERROR = -1,

	/** Input was not a properly formatted Git object id. */
	GIT_ENOTOID = -2,

	/** Input does not exist in the scope searched. */
	GIT_ENOTFOUND = -3,

	/** Not enough space available. */
	GIT_ENOMEM = -4,

	/** Consult the OS error information. */
	GIT_EOSERR = -5,

	/** The specified object is of invalid type */
	GIT_EOBJTYPE = -6,

	/** The specified repository is invalid */
	GIT_ENOTAREPO = -7,

	/** The object type is invalid or doesn't match */
	GIT_EINVALIDTYPE = -8,

	/** The object cannot be written because it's missing internal data */
	GIT_EMISSINGOBJDATA = -9,

	/** The packfile for the ODB is corrupted */
	GIT_EPACKCORRUPTED = -10,

	/** Failed to acquire or release a file lock */
	GIT_EFLOCKFAIL = -11,

	/** The Z library failed to inflate/deflate an object's data */
	GIT_EZLIB = -12,

	/** The queried object is currently busy */
	GIT_EBUSY = -13,

	/** The index file is not backed up by an existing repository */
	GIT_EBAREINDEX = -14,

	/** The name of the reference is not valid */
	GIT_EINVALIDREFNAME = -15,

	/** The specified reference has its data corrupted */
	GIT_EREFCORRUPTED = -16,

	/** The specified symbolic reference is too deeply nested */
	GIT_ETOONESTEDSYMREF = -17,

	/** The pack-refs file is either corrupted or its format is not currently supported */
	GIT_EPACKEDREFSCORRUPTED = -18,

	/** The path is invalid */
	GIT_EINVALIDPATH = -19,

	/** The revision walker is empty; there are no more commits left to iterate */
	GIT_EREVWALKOVER = -20,

	/** The state of the reference is not valid */
	GIT_EINVALIDREFSTATE = -21,

	/** This feature has not been implemented yet */
	GIT_ENOTIMPLEMENTED = -22,

	/** A reference with this name already exists */
	GIT_EEXISTS = -23,

	/** The given integer literal is too large to be parsed */
	GIT_EOVERFLOW = -24,

	/** The given literal is not a valid number */
	GIT_ENOTNUM = -25,

	/** Streaming error */
	GIT_ESTREAM = -26,

	/** invalid arguments to function */
	GIT_EINVALIDARGS = -27,

	/** The specified object has its data corrupted */
	GIT_EOBJCORRUPTED = -28,

	/** The given short oid is ambiguous */
	GIT_EAMBIGUOUSOIDPREFIX = -29,

	/** Skip and passthrough the given ODB backend */
	GIT_EPASSTHROUGH = -30,

	/** The path pattern and string did not match */
	GIT_ENOMATCH = -31,

	/** The buffer is too short to satisfy the request */
	GIT_ESHORTBUFFER = -32,
} git_error;

/**
 * Return a detailed error string with the latest error
 * that occurred in the library.
 * @return a string explaining the error
 */
GIT_EXTERN(const char *) git_lasterror(void);

/**
 * strerror() for the Git library
 *
 * Get a string description for a given error code.
 * NOTE: This method will be eventually deprecated in favor
 * of the new `git_lasterror`.
 *
 * @param num The error code to explain
 * @return a string explaining the error code
 */
GIT_EXTERN(const char *) git_strerror(int num);

/**
 * Clear the latest library error
 */
GIT_EXTERN(void) git_clearerror(void);

/** @} */
GIT_END_DECL
#endif
