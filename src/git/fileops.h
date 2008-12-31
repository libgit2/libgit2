#ifndef INCLUDE_git_fileops_h__
#define INCLUDE_git_fileops_h__

#include "common.h"

/**
 * @file git/fileops.h
 * @brief Git platform agnostic filesystem operations
 * @defgroup git_fileops Git filesystem operations
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * For each directory entry (except "." and ".."), run the function
 * "fn", passing it "arg" as its first argument and the path to
 * the entry as the second argument.
 * @param dir The directory to walk
 * @param fn The callback function to run for each entry in *dir.
 *        "fn" may return >0 to signal "I'm done. Stop parsing and
 *        return successfully" or <0 to signal an error.  All non-zero
 *        return codes cause directory traversal to stop.
 * @param arg The first argument that will be passed to 'fn'
 * @return GIT_SUCCESS if all entries were successfully traversed,
 *         otherwise the result of fn.
 */
GIT_EXTERN(int) git_foreach_dirent(const char *dir,
			int (*fn)(void *, const char *), void *arg);

/** @} */
GIT_END_DECL
#endif /* INCLUDE_git_fileops_h__ */
