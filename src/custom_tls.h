/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_custom_tls_h__
#define INCLUDE_custom_tls_h__

#include "common.h"
#include "git2/sys/custom_tls.h"

int git_custom_tls__global_init(void);

#ifdef GIT_THREADS

typedef struct {
  git_set_tls_on_internal_thread_cb set_storage_on_thread;

  git_teardown_tls_on_internal_thread_cb teardown_storage_on_thread;

  /**
   * payload should be set on the thread that is spawning the child thread.
   * This payload will be passed to set_storage_on_thread
   */
  void *payload;
} git_custom_tls;

int git_custom_tls__init(git_custom_tls *tls);

#endif

#endif
