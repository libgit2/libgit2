/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "pthread.h"
#include "thread.h"
#include "runtime.h"

git_tlsdata_key thread_handle;

static void git_threads_global_shutdown(void) {
  git_tlsdata_dispose(thread_handle);
}

int git_threads_global_init(void) {
  int error = git_tlsdata_init(&thread_handle, NULL);
  if (error != 0) {
    return error;
  }

  return git_runtime_shutdown_register(git_threads_global_shutdown);
}

static void *git_unix__threadproc(void *arg)
{
  void *result;
  int error;
  git_thread *thread = arg;

  error = git_tlsdata_set(thread_handle, thread);
  if (error != 0) {
    return NULL;
  }

  if (thread->tls.set_storage_on_thread) {
    thread->tls.set_storage_on_thread(thread->tls.payload);
  }

  result = thread->proc(thread->param);

  if (thread->tls.teardown_storage_on_thread) {
    thread->tls.teardown_storage_on_thread();
  }

  return result;
}

int git_thread_create(
	git_thread *thread,
	void *(*start_routine)(void*),
	void *arg)
{

  thread->proc = start_routine;
  thread->param = arg;
  if (git_custom_tls__init(&thread->tls) < 0)
    return -1;

  return pthread_create(&thread->thread, NULL, git_unix__threadproc, thread);
}

void git_thread_exit(void *value)
{
  git_thread *thread = git_tlsdata_get(thread_handle);

  if (thread && thread->tls.teardown_storage_on_thread)
    thread->tls.teardown_storage_on_thread();

  return pthread_exit(value);
}
