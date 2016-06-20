/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_unix_pthread_h__
#define INCLUDE_unix_pthread_h__

typedef struct {
	pthread_t thread;
} git_thread;

#define git_thread_create(git_thread_ptr, start_routine, arg) \
	pthread_create(&(git_thread_ptr)->thread, NULL, start_routine, arg)
#define git_thread_join(git_thread_ptr, status) \
	pthread_join((git_thread_ptr)->thread, status)

/* Git Mutex */
#define git_mutex pthread_mutex_t
#define git_mutex_init(a)	pthread_mutex_init(a, NULL)
#define git_mutex_lock(a)	pthread_mutex_lock(a)
#define git_mutex_unlock(a)     pthread_mutex_unlock(a)
#define git_mutex_free(a)	pthread_mutex_destroy(a)

/* Git condition vars */
#define git_cond pthread_cond_t
#define git_cond_init(c)	pthread_cond_init(c, NULL)
#define git_cond_free(c) 	pthread_cond_destroy(c)
#define git_cond_wait(c, l)	pthread_cond_wait(c, l)
#define git_cond_signal(c)	pthread_cond_signal(c)
#define git_cond_broadcast(c)	pthread_cond_broadcast(c)

#endif /* INCLUDE_unix_pthread_h__ */
