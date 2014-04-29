/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_trace_h__
#define INCLUDE_git_trace_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/trace.h
 * @brief Git tracing configuration routines
 * @defgroup git_trace Git tracing configuration routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Available tracing messages.  Each tracing level can be enabled
 * independently or pass GIT_TRACE_ALL to enable all levels.
 */
typedef enum {
	/** No tracing will be performed. */
	GIT_TRACE_NONE = 0x0000u,

	/** All tracing messages will be sent. */
	GIT_TRACE_ALL = 0xFFFFu,

	/** Severe errors that may impact the program's execution */
	GIT_TRACE_FATAL = 0x0001u,

	/** Errors that do not impact the program's execution */
	GIT_TRACE_ERROR = 0x0002u,
	GIT_TRACE_ERROR_AND_BELOW = 0x0003u,

	/** Warnings that suggest abnormal data */
	GIT_TRACE_WARN = 0x0004u,
	GIT_TRACE_WARN_AND_BELOW = 0x0007u,

	/** Informational messages about program execution */
	GIT_TRACE_INFO = 0x0008u,
	GIT_TRACE_INFO_AND_BELOW = 0x000Fu,

	/** Detailed data that allows for debugging */
	GIT_TRACE_DEBUG = 0x0010u,

	/** Exceptionally detailed debugging data */
	GIT_TRACE_TRACE = 0x0020u,

	/** Performance tracking related traces */
	GIT_TRACE_PERF = 0x0040u,
} git_trace_level_t;

/**
 * An instance for a tracing function
 */
typedef void (*git_trace_callback)(
	git_trace_level_t level, /* just one bit will be sent */
	void *cb_payload,
	void *msg_payload,
	const char *msg);

/**
 * Sets the system tracing configuration to the specified level with the
 * specified callback.  When system events occur at a level equal to, or
 * lower than, the given level they will be reported to the given callback.
 *
 * @param level Bitmask of all enabled trace levels
 * @param cb Function to call with trace messages
 * @param cb_payload Payload to pass when callback is invoked
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_trace_set(
	git_trace_level_t level,
	git_trace_callback cb,
	void *cb_payload);

/** @} */
GIT_END_DECL
#endif
