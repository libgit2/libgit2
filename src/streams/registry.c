/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "common.h"
#include "global.h"
#include "streams/tls.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "streams/stransport.h"

struct stream_registry {
	git_rwlock lock;
	git_stream_registration callbacks;
	git_stream_registration tls_callbacks;
};

static struct stream_registry stream_registry;

static void shutdown_stream_registry(void)
{
	git_rwlock_free(&stream_registry.lock);
}

int git_stream_registry_global_init(void)
{
	if (git_rwlock_init(&stream_registry.lock) < 0)
		return -1;

	git__on_shutdown(shutdown_stream_registry);
	return 0;
}

int git_stream_registry_lookup(git_stream_registration *out, int tls)
{
	git_stream_registration *target = tls ?
		&stream_registry.callbacks :
		&stream_registry.tls_callbacks;
	int error = GIT_ENOTFOUND;

	assert(out);

	if (git_rwlock_rdlock(&stream_registry.lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock stream registry");
		return -1;
	}

	if (target->init) {
		memcpy(out, target, sizeof(git_stream_registration));
		error = 0;
	}

	git_rwlock_rdunlock(&stream_registry.lock);
	return error;
}

int git_stream_register(int tls, git_stream_registration *registration)
{
	git_stream_registration *target = tls ?
		&stream_registry.callbacks :
		&stream_registry.tls_callbacks;

	assert(!registration || registration->init);

	GITERR_CHECK_VERSION(registration, GIT_STREAM_VERSION, "stream_registration");

	if (git_rwlock_wrlock(&stream_registry.lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock stream registry");
		return -1;
	}

	if (registration)
		memcpy(target, registration, sizeof(git_stream_registration));
	else
		memset(target, 0, sizeof(git_stream_registration));

	git_rwlock_wrunlock(&stream_registry.lock);
	return 0;
}

int git_stream_register_tls(git_stream_cb ctor)
{
	git_stream_registration registration = {0};

	if (ctor) {
		registration.version = GIT_STREAM_VERSION;
		registration.init = ctor;
		registration.wrap = NULL;

		return git_stream_register(1, &registration);
	} else {
		return git_stream_register(1, NULL);
	}
}
