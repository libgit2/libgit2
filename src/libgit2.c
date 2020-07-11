/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "runtime.h"

#include "alloc.h"
#include "threadstate.h"
#include "hash.h"
#include "sysdir.h"
#include "filter.h"
#include "settings.h"
#include "mwindow.h"
#include "merge_driver.h"
#include "pool.h"
#include "streams/registry.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "thread-utils.h"
#include "git2/global.h"
#include "transports/ssh.h"
#include "win32/w32_stack.h"

int git_libgit2_init(void)
{
	static git_runtime_init_fn init_fns[] = {
		git_allocator_global_init,
		git_threadstate_global_init,
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
		git_pool_global_init,
		git_settings_global_init
	};

	return git_runtime_init(init_fns, ARRAY_SIZE(init_fns));
}

int git_libgit2_shutdown(void)
{
	return git_runtime_shutdown();
}
