/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "custom_tls.h"
#include "runtime.h"

#ifdef GIT_THREADS

#ifdef GIT_WIN32
#   include "win32/thread.h"
#else
#   include "unix/pthread.h"
#endif

struct git_custom_tls_callbacks {
	git_retrieve_tls_for_internal_thread_cb retrieve_storage_for_internal_thread;

	git_set_tls_on_internal_thread_cb set_storage_on_thread;

	git_teardown_tls_on_internal_thread_cb teardown_storage_on_thread;

  git_rwlock lock;
};

struct git_custom_tls_callbacks git__custom_tls = { 0, 0, 0 };

static void git_custom_tls_global_shutdown(void)
{
	if (git_rwlock_wrlock(&git__custom_tls.lock) < 0)
		return;

	git__custom_tls.retrieve_storage_for_internal_thread = 0;
	git__custom_tls.set_storage_on_thread = 0;
	git__custom_tls.teardown_storage_on_thread = 0;

	git_rwlock_wrunlock(&git__custom_tls.lock);
	git_rwlock_free(&git__custom_tls.lock);
}

int git_custom_tls__global_init(void)
{
	if (git_rwlock_init(&git__custom_tls.lock) < 0)
		return -1;

	return git_runtime_shutdown_register(git_custom_tls_global_shutdown);
}

int git_custom_tls_set_callbacks(
	git_retrieve_tls_for_internal_thread_cb retrieve_storage_for_internal_thread,
	git_set_tls_on_internal_thread_cb set_storage_on_thread,
	git_teardown_tls_on_internal_thread_cb teardown_storage_on_thread)
{
  /* We want to ensure that all callbacks are set or not set in totality.
   * It does not make sense to have a subset of callbacks set.
   */
	assert((retrieve_storage_for_internal_thread && set_storage_on_thread &&
		teardown_storage_on_thread) || !(retrieve_storage_for_internal_thread &&
		set_storage_on_thread && teardown_storage_on_thread));

	if (git_rwlock_wrlock(&git__custom_tls.lock) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to lock custom thread local storage");
		return -1;
	}

	git__custom_tls.retrieve_storage_for_internal_thread =
		retrieve_storage_for_internal_thread;
	git__custom_tls.set_storage_on_thread =
		set_storage_on_thread;
	git__custom_tls.teardown_storage_on_thread =
		teardown_storage_on_thread;

	git_rwlock_wrunlock(&git__custom_tls.lock);
	return 0;
}

int git_custom_tls__init(git_custom_tls *tls)
{
  if (git_rwlock_rdlock(&git__custom_tls.lock) < 0) {
    git_error_set(GIT_ERROR_OS, "failed to lock custom thread local storage");
    return -1;
  }

  /* We try to ensure that all 3 callbacks must be set or not set.
   * It would not make sense to have a subset of the callbacks set.
   */
  if (!git__custom_tls.retrieve_storage_for_internal_thread) {
    tls->set_storage_on_thread = NULL;
    tls->teardown_storage_on_thread = NULL;
    tls->payload = NULL;
  } else {
    /* We set these on a struct so that if for whatever reason the opts are changed
     * at least the opts will remain consistent for any given thread already in
     * motion.
     */
    tls->set_storage_on_thread = git__custom_tls.set_storage_on_thread;
    tls->teardown_storage_on_thread = git__custom_tls.teardown_storage_on_thread;
    tls->payload = git__custom_tls.retrieve_storage_for_internal_thread();
  }

  git_rwlock_rdunlock(&git__custom_tls.lock);
  return 0;
}

#else

int git_custom_tls__global_init(void)
{
	return 0;
}

int git_custom_tls_set_callbacks(
	git_retrieve_tls_for_internal_thread_cb retrieve_storage_for_internal_thread,
	git_set_tls_on_internal_thread_cb set_storage_on_thread,
	git_teardown_tls_on_internal_thread_cb teardown_storage_on_thread)
{
	return 0;
}

#endif
