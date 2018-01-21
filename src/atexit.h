/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_atexit_h__
#define INCLUDE_atexit_h__

#include "common.h"
#include "git2/atexit.h"

typedef struct git__atexit git__atexit;

/**
 * The signature of the function which will be called in order to execute this
 * rollback.
 */
typedef int (*git__atexit_execute)(git__atexit *atexit);

/**
 * Include this header as the first element in your atexit cancellation
 * structure.
 */
struct git__atexit {
	/**
	 * Execute this rollback. This function may be called from any thread.
	 */
	git__atexit_execute execute;
};

/**
 * Register a rollback.
 */
int git__atexit_register(git__atexit* atexit);

/**
 * UnRegister a rollback.
 */
int git__atexit_unregister(git__atexit* atexit);

int git_atexit_global_init(void);

#endif
