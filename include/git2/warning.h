/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_warning_h__
#define INCLUDE_git_warning_h__

#include "common.h"
#include "types.h"

GIT_BEGIN_DECL

/**
 * The kind of warning that was generated.
 */
typedef enum {
	/**
	 * Sentinel value. Should never be used.
	 */
	GIT_GENERIC_NONE = 0,

	/**
	 * Generic warning. There is no extended information
	 * available.
	 */
	GIT_WARNING_GENERIC,

	/**
	 * Warning related to line ending conversion.
	 */
	GIT_WARNING_CRLF,
} git_warning_t;

/**
 * Base struct for warnings.

 * These fields are always available for warnings.
 */
typedef struct {
	/**
	 * The kind of warning.
	 */
	git_warning_t type;

	/**
	 * The text for this warning
	 */
	const char *str;
} git_warning;

typedef struct {
	/** The base struct */
	git_warning parent;

	/** The file this warning refers to */
	const char *path;
} git_warning_crlf;

/**
 * User-specified callback for warnings
 *
 * The warning object is owned by the library and may be deallocated
 * on function return. Make a copy if you want to store the data for
 * later processing. Do not attempt to free it.
 *
 * Note that this may be called concurrently from multiple threads.
 *
 * @param warning the current warning
 * @param payload user-provided payload when registering
 * @return 0 to continue, a negative number to stop processing
 */
typedef int (*git_warning_cb)(git_warning *warning, void *payload);

/**
 * Set the warning callback
 *
 * This sets the global warning callback which be called in places
 * where issues were found which might be of interest to a user but
 * would not cause an error to be returned.
 *
 * This function does not perform locking. Do not call it
 * concurrently.
 *
 * @param callback the function to call; pass `NULL` to unregister
 * @param payload user-specified data to be passed to the callback
 */
GIT_EXTERN(int) git_warning_set_callback(git_warning_cb callback, void *payload);

GIT_END_DECL

#endif
