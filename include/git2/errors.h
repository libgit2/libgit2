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
#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

/**
 * @file git2/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

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

/** The given integer literal is too large to be parsed */
#define GIT_EOVERFLOW (GIT_ERROR - 24)

/** The given literal is not a valid number */
#define GIT_ENOTNUM (GIT_ERROR - 25)

/** Streaming error */
#define GIT_ESTREAM (GIT_ERROR - 26)

/** invalid arguments to function */
#define GIT_EINVALIDARGS (GIT_ERROR - 27)

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

/** @} */
GIT_END_DECL
#endif
