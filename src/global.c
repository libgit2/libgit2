/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "global.h"
#include "git2/threads.h" 
#include "thread-utils.h"

/**
 * Handle the global state with TLS
 *
 * If libgit2 is built with GIT_THREADS enabled,
 * the `git_threads_init()` function must be called
 * before calling any other function of the library.
 *
 * This function allocates a TLS index (using pthreads
 * or the native Win32 API) to store the global state
 * on a per-thread basis.
 *
 * Any internal method that requires global state will
 * then call `git__global_state()` which returns a pointer
 * to the global state structure; this pointer is lazily
 * allocated on each thread.
 *
 * Before shutting down the library, the
 * `git_threads_shutdown` method must be called to free
 * the previously reserved TLS index.
 *
 * If libgit2 is built without threading support, the
 * `git__global_statestate()` call returns a pointer to a single,
 * statically allocated global state. The `git_thread_`
 * functions are not available in that case.
 */

#if defined(GIT_THREADS) && defined(GIT_WIN32)

static DWORD _tls_index;
static int _tls_init = 0;

void git_threads_init(void)
{
	if (_tls_init)
		return;

	_tls_index = TlsAlloc();
	_tls_init = 1;
}

void git_threads_shutdown(void)
{
	TlsFree(_tls_index);
	_tls_init = 0;
}

git_global_st *git__global_state(void)
{
	void *ptr;

	if ((ptr = TlsGetValue(_tls_index)) != NULL)
		return ptr;

	ptr = malloc(sizeof(git_global_st));
	if (!ptr)
		return NULL;

	memset(ptr, 0x0, sizeof(git_global_st));
	TlsSetValue(_tls_index, ptr);
	return ptr;
}

#elif defined(GIT_THREADS) && defined(_POSIX_THREADS)

static pthread_key_t _tls_key;
static int _tls_init = 0;

static void cb__free_status(void *st)
{
	free(st);
}

void git_threads_init(void)
{
	if (_tls_init)
		return;

	pthread_key_create(&_tls_key, &cb__free_status);
	_tls_init = 1;
}

void git_threads_shutdown(void)
{
	pthread_key_delete(_tls_key);
	_tls_init = 0;
}

git_global_st *git__global_state(void)
{
	void *ptr;

	if ((ptr = pthread_getspecific(_tls_key)) != NULL)
		return ptr;

	ptr = malloc(sizeof(git_global_st));
	if (!ptr)
		return NULL;

	memset(ptr, 0x0, sizeof(git_global_st));
	pthread_setspecific(_tls_key, ptr);
	return ptr;
}

#else

static git_global_st __state;

void git_threads_init(void)
{
	/* noop */ 
}

void git_threads_shutdown(void)
{
	/* noop */
}

git_global_st *git__global_state(void)
{
	return &__state;
}

#endif /* GIT_THREADS */
