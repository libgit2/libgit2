/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_errors_h__
#define INCLUDE_errors_h__

#include "common.h"

/*
 * Set the error message for this thread, formatting as needed.
 */
void git_error_set(int error_class, const char *fmt, ...) GIT_FORMAT_PRINTF(2, 3);
void git_error_vset(int error_class, const char *fmt, va_list ap);

/**
 * Gets the system error code for this thread.
 */
int git_error_system_last(void);

/**
 * Sets the system error code for this thread.
 */
void git_error_system_set(int code);

/**
 * Structure to preserve libgit2 error state
 */
typedef struct {
	int error_code;
	unsigned int oom : 1;
	git_error error_msg;
} git_error_state;

/**
 * Capture current error state to restore later, returning error code.
 * If `error_code` is zero, this does not clear the current error state.
 * You must either restore this error state, or free it.
 */
extern int git_error_state_capture(git_error_state *state, int error_code);

/**
 * Restore error state to a previous value, returning saved error code.
 */
extern int git_error_state_restore(git_error_state *state);

/** Free an error state. */
extern void git_error_state_free(git_error_state *state);

#endif
