/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include "alloc.h"
#include "buf.h"
#include "common.h"
#include "filter.h"
#include "hash.h"
#include "merge_driver.h"
#include "pool.h"
#include "mwindow.h"
#include "oid.h"
#include "rand.h"
#include "runtime.h"
#include "settings.h"
#include "sysdir.h"
#include "thread.h"
#include "git2/global.h"
#include "streams/registry.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "streams/socket.h"
#include "transports/ssh_libssh2.h"

#ifdef GIT_WIN32
# include "win32/w32_leakcheck.h"
#endif

int git_libgit2_init(void)
{
	static git_runtime_init_fn init_fns[] = {
#ifdef GIT_WIN32
		git_win32_leakcheck_global_init,
#endif
		git_allocator_global_init,
		git_error_global_init,
		git_threads_global_init,
		git_oid_global_init,
		git_rand_global_init,
		git_hash_global_init,
		git_sysdir_global_init,
		git_filter_global_init,
		git_merge_driver_global_init,
		git_transport_ssh_libssh2_global_init,
		git_stream_registry_global_init,
		git_socket_stream_global_init,
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

int git_libgit2_version(int *major, int *minor, int *rev)
{
	*major = LIBGIT2_VER_MAJOR;
	*minor = LIBGIT2_VER_MINOR;
	*rev = LIBGIT2_VER_REVISION;

	return 0;
}

const char *git_libgit2_prerelease(void)
{
	return LIBGIT2_VER_PRERELEASE;
}

int git_libgit2_features(void)
{
	return 0
#ifdef GIT_THREADS
		| GIT_FEATURE_THREADS
#endif
#ifdef GIT_HTTPS
		| GIT_FEATURE_HTTPS
#endif
#ifdef GIT_SSH
		| GIT_FEATURE_SSH
#endif
#ifdef GIT_USE_NSEC
		| GIT_FEATURE_NSEC
#endif
	;
}
