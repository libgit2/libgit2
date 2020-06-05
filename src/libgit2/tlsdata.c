/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "tlsdata.h"
#include "runtime.h"

static void tlsdata_dispose(git_tlsdata *tlsdata);

/**
 * Handle the thread-local state
 *
 * `git_tlsdata_global_init` will be called as part
 * of `git_libgit2_init` (which itself must be called
 * before calling any other function in the library).
 *
 * This function allocates a TLS index (using pthreads
 * or fiber-local storage in Win32) to store the per-
 * thread state.
 *
 * Any internal method that requires thread-local state
 * will then call `git_tlsdata_get()` which returns a
 * pointer to the thread-local state structure; this
 * structure is lazily allocated on each thread.
 *
 * This mechanism will register a shutdown handler
 * (`git_tlsdata_global_shutdown`) which will free the
 * TLS index.  This shutdown handler will be called by
 * `git_libgit2_shutdown`.
 *
 * If libgit2 is built without threading support, the
 * `git_tlsdata_get()` call returns a pointer to a single,
 * statically allocated global state. The `git_thread_`
 * functions are not available in that case.
 */

#if defined(GIT_THREADS) && defined(GIT_WIN32)

static DWORD fls_index;

static void git_tlsdata_global_shutdown(void)
{
	FlsFree(fls_index);
}

static void WINAPI fls_free(void *tlsdata)
{
	tlsdata_dispose(tlsdata);
	git__free(tlsdata);
}

int git_tlsdata_global_init(void)
{
	if ((fls_index = FlsAlloc(fls_free)) == FLS_OUT_OF_INDEXES)
		return -1;

	return git_runtime_shutdown_register(git_tlsdata_global_shutdown);
}

git_tlsdata *git_tlsdata_get(void)
{
	git_tlsdata *tlsdata;

	if ((tlsdata = FlsGetValue(fls_index)) != NULL)
		return tlsdata;

	if ((tlsdata = git__calloc(1, sizeof(git_tlsdata))) == NULL)
		return NULL;

	git_buf_init(&tlsdata->error_buf, 0);

	FlsSetValue(fls_index, tlsdata);
	return tlsdata;
}

#elif defined(GIT_THREADS) && defined(_POSIX_THREADS)

static pthread_key_t tls_key;

static void git_tlsdata_global_shutdown(void)
{
	git_tlsdata *tlsdata;

	tlsdata = pthread_getspecific(tls_key);
	pthread_setspecific(tls_key, NULL);

	tlsdata_dispose(tlsdata);
	git__free(tlsdata);

	pthread_key_delete(tls_key);
}

static void tls_free(void *tlsdata)
{
	tlsdata_dispose(tlsdata);
	git__free(tlsdata);
}

int git_tlsdata_global_init(void)
{
	if (pthread_key_create(&tls_key, &tls_free) != 0)
		return -1;

	return git_runtime_shutdown_register(git_tlsdata_global_shutdown);
}

git_tlsdata *git_tlsdata_get(void)
{
	git_tlsdata *tlsdata;

	if ((tlsdata = pthread_getspecific(tls_key)) != NULL)
		return tlsdata;

	if ((tlsdata = git__calloc(1, sizeof(git_tlsdata))) == NULL)
		return NULL;

	git_buf_init(&tlsdata->error_buf, 0);

	pthread_setspecific(tls_key, tlsdata);
	return tlsdata;
}

#elif defined(GIT_THREADS)
# error unknown threading model
#else

static git_tlsdata tlsdata;

static void git_tlsdata_global_shutdown(void)
{
	tlsdata_dispose(&tlsdata);
	memset(&tlsdata, 0, sizeof(git_tlsdata));
}

int git_tlsdata_global_init(void)
{
	return git_runtime_shutdown_register(git_tlsdata_global_shutdown);
}

git_tlsdata *git_tlsdata_get(void)
{
	return &tlsdata;
}

#endif

static void tlsdata_dispose(git_tlsdata *tlsdata)
{
	if (!tlsdata)
		return;

	git__free(tlsdata->error_t.message);
	tlsdata->error_t.message = NULL;
}
