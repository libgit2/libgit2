/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef GIT_PTHREAD_H
#define GIT_PTHREAD_H

#include "../common.h"

#if defined (_MSC_VER)
#	define GIT_RESTRICT __restrict
#else
#	define GIT_RESTRICT __restrict__
#endif

typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef int pthread_attr_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_t;
typedef HANDLE pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER {(void*)-1};

int pthread_create(pthread_t *GIT_RESTRICT,
					const pthread_attr_t *GIT_RESTRICT,
					void *(*start_routine)(void*), void *__restrict);

int pthread_join(pthread_t, void **);

int pthread_mutex_init(pthread_mutex_t *GIT_RESTRICT, const pthread_mutexattr_t *GIT_RESTRICT);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);

int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int pthread_cond_signal(pthread_cond_t *);
/* pthread_cond_broadcast is not supported on Win32 yet. */

int pthread_num_processors_np(void);

#endif
