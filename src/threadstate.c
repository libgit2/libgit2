/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "threadstate.h"
#include "global.h"

static void threadstate_dispose(git_threadstate *threadstate);

/**
 * Handle the thread-local state
 *
 * `git_threadstate_global_init` will be called as part
 * of `git_libgit2_init` (which itself must be called
 * before calling any other function in the library).
 *
 * This function allocates a TLS index (using pthreads
 * or fiber-local storage in Win32) to store the per-
 * thread state.
 *
 * Any internal method that requires thread-local state
 * will then call `git_threadstate_get()` which returns a
 * pointer to the thread-local state structure; this
 * structure is lazily allocated on each thread.
 *
 * This mechanism will register a shutdown handler
 * (`git_threadstate_global_shutdown`) which will free the
 * TLS index.  This shutdown handler will be called by
 * `git_libgit2_shutdown`.
 *
 * If libgit2 is built without threading support, the
 * `git_threadstate_get()` call returns a pointer to a single,
 * statically allocated global state. The `git_thread_`
 * functions are not available in that case.
 */

#if defined(GIT_THREADS) && defined(GIT_WIN32)

static DWORD fls_index;

static void git_threadstate_global_shutdown(void)
{
	FlsFree(fls_index);
}

static void WINAPI fls_free(void *threadstate)
{
	threadstate_dispose(threadstate);
	git__free(threadstate);
}

int git_threadstate_global_init(void)
{
	if ((fls_index = FlsAlloc(fls_free)) == FLS_OUT_OF_INDEXES)
		return -1;

	git__on_shutdown(git_threadstate_global_shutdown);

	return 0;
}

git_threadstate *git_threadstate_get(void)
{
	git_threadstate *threadstate;

	if ((threadstate = FlsGetValue(fls_index)) != NULL)
		return threadstate;

	if ((threadstate = git__calloc(1, sizeof(git_threadstate))) == NULL ||
	    git_buf_init(&threadstate->error_buf, 0) < 0)
		return NULL;

	FlsSetValue(fls_index, threadstate);
	return threadstate;
}

#elif defined(GIT_THREADS) && defined(_POSIX_THREADS)

static pthread_key_t tls_key;

static void git_threadstate_global_shutdown(void)
{
	git_threadstate *threadstate;

	threadstate = pthread_getspecific(tls_key);
	pthread_setspecific(tls_key, NULL);

	threadstate_dispose(threadstate);
	git__free(threadstate);

	pthread_key_delete(tls_key);
}

static void tls_free(void *threadstate)
{
	threadstate_dispose(threadstate);
	git__free(threadstate);
}

int git_threadstate_global_init(void)
{
	if (pthread_key_create(&tls_key, &tls_free) != 0)
		return -1;

	git__on_shutdown(git_threadstate_global_shutdown);

	return 0;
}

git_threadstate *git_threadstate_get(void)
{
	git_threadstate *threadstate;

	if ((threadstate = pthread_getspecific(tls_key)) != NULL)
		return threadstate;

	if ((threadstate = git__calloc(1, sizeof(git_threadstate))) == NULL ||
	    git_buf_init(&threadstate->error_buf, 0) < 0)
		return NULL;

	pthread_setspecific(tls_key, threadstate);
	return threadstate;
}

#elif defined(GIT_THREADS)
# error unknown threading model
#else

static git_threadstate threadstate;

static void git_threadstate_global_shutdown(void)
{
	threadstate_dispose(&threadstate);
	memset(&threadstate, 0, sizeof(git_threadstate);
}

int git_threadstate_global_init(void)
{
	git__on_shutdown(git_threadstate_global_shutdown);

	return 0;
}

git_threadstate *git_threadstate_get(void)
{
	return &threadstate;
}

#endif

static void threadstate_dispose(git_threadstate *threadstate)
{
	if (!threadstate)
		return;

	git__free(threadstate->error_t.message);
	threadstate->error_t.message = NULL;
}
