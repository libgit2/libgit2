/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "global.h"

#include "alloc.h"
#include "tlsdata.h"
#include "hash.h"
#include "sysdir.h"
#include "filter.h"
#include "settings.h"
#include "mwindow.h"
#include "merge_driver.h"
#include "streams/registry.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "thread-utils.h"
#include "git2/global.h"
#include "transports/ssh.h"

#if defined(GIT_MSVC_CRTDBG)
#include "win32/w32_stack.h"
#include "win32/w32_crtdbg_stacktrace.h"
#endif

typedef int (*git_global_init_fn)(void);

static git_global_init_fn git__init_callbacks[] = {
#if defined(GIT_MSVC_CRTDBG)
	git_win32__crtdbg_stacktrace_init,
	git_win32__stack_init,
#endif
	git_allocator_global_init,
	git_tlsdata_global_init,
	git_threads_global_init,
	git_hash_global_init,
	git_sysdir_global_init,
	git_filter_global_init,
	git_merge_driver_global_init,
	git_transport_ssh_global_init,
	git_stream_registry_global_init,
	git_openssl_stream_global_init,
	git_mbedtls_stream_global_init,
	git_mwindow_global_init,
	git_settings_global_init
};

static git_global_shutdown_fn git__shutdown_callbacks[ARRAY_SIZE(git__init_callbacks)];

static git_atomic git__n_shutdown_callbacks;
static git_atomic git__n_inits;

void git__on_shutdown(git_global_shutdown_fn callback)
{
	int count = git_atomic_inc(&git__n_shutdown_callbacks);
	assert(count <= (int) ARRAY_SIZE(git__shutdown_callbacks) && count > 0);
	git__shutdown_callbacks[count - 1] = callback;
}

static int init_common(void)
{
	size_t i;
	int ret;

	/* Initialize subsystems that have global state */
	for (i = 0; i < ARRAY_SIZE(git__init_callbacks); i++)
		if ((ret = git__init_callbacks[i]()) != 0)
			break;

	GIT_MEMORY_BARRIER;

	return ret;
}

static void shutdown_common(void)
{
	int pos;

	/* Shutdown subsystems that have registered */
	for (pos = git_atomic_get(&git__n_shutdown_callbacks);
		pos > 0;
		pos = git_atomic_dec(&git__n_shutdown_callbacks)) {

		git_global_shutdown_fn cb = git__swap(
			git__shutdown_callbacks[pos - 1], NULL);

		if (cb != NULL)
			cb();
	}
}

/*
 * `git_libgit2_init()` allows subsystems to perform global setup,
 * which may take place in the global scope.  An explicit memory
 * fence exists at the exit of `git_libgit2_init()`.  Without this,
 * CPU cores are free to reorder cache invalidation of `_tls_init`
 * before cache invalidation of the subsystems' newly written global
 * state.
 */
#if defined(GIT_THREADS) && defined(GIT_WIN32)

static volatile LONG init_mutex = 0;

GIT_INLINE(int) mutex_lock(void)
{
	while (InterlockedCompareExchange(&init_mutex, 1, 0)) { Sleep(0); }
	return 0;
}

GIT_INLINE(int) mutex_unlock(void)
{
	InterlockedExchange(&init_mutex, 0);
	return 0;
}

#elif defined(GIT_THREADS) && defined(_POSIX_THREADS)

static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

GIT_INLINE(int) mutex_lock(void)
{
	return pthread_mutex_lock(&init_mutex) == 0 ? 0 : -1;
}

GIT_INLINE(int) mutex_unlock(void)
{
	return pthread_mutex_unlock(&init_mutex) == 0 ? 0 : -1;
}

#elif defined(GIT_THREADS)
# error unknown threading model
#else

# define mutex_lock() 0
# define mutex_unlock() 0

#endif

int git_libgit2_init(void)
{
	int ret;

	if (mutex_lock() < 0)
		return -1;

	/* Only do work on a 0 -> 1 transition of the refcount */
	if ((ret = git_atomic_inc(&git__n_inits)) == 1) {
		if (init_common() < 0)
			ret = -1;
	}

	if (mutex_unlock() < 0)
		return -1;

	return ret;
}

int git_libgit2_shutdown(void)
{
	int ret;

	/* Enter the lock */
	if (mutex_lock() < 0)
		return -1;

	/* Only do work on a 1 -> 0 transition of the refcount */
	if ((ret = git_atomic_dec(&git__n_inits)) == 0)
		shutdown_common();

	/* Exit the lock */
	if (mutex_unlock() < 0)
		return -1;

	return ret;
}
