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

struct git_tls_stream_registration {
	git_rwlock lock;
	git_stream_registration callbacks;
};

static struct git_tls_stream_registration stream_registration;

static void shutdown_ssl(void)
{
	git_rwlock_free(&stream_registration.lock);
}

int git_tls_stream_global_init(void)
{
	if (git_rwlock_init(&stream_registration.lock) < 0)
		return -1;

	git__on_shutdown(shutdown_ssl);
	return 0;
}

int git_stream_register_tls(git_stream_registration *registration)
{
	assert(!registration || registration->init);

	if (git_rwlock_wrlock(&stream_registration.lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock stream registration");
		return -1;
	}

	if (registration)
		memcpy(&stream_registration.callbacks, registration,
		    sizeof(git_stream_registration));
	else
		memset(&stream_registration.callbacks, 0,
		    sizeof(git_stream_registration));

	git_rwlock_wrunlock(&stream_registration.lock);
	return 0;
}

int git_tls_stream_new(git_stream **out, const char *host, const char *port)
{
	int (*init)(git_stream **, const char *, const char *) = NULL;

	assert(out && host && port);

	if (git_rwlock_rdlock(&stream_registration.lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock stream registration");
		return -1;
	}

	if (stream_registration.callbacks.init) {
		init = stream_registration.callbacks.init;
	} else {
#ifdef GIT_SECURE_TRANSPORT
		init = git_stransport_stream_new;
#elif defined(GIT_OPENSSL)
		init = git_openssl_stream_new;
#elif defined(GIT_MBEDTLS)
		init = git_mbedtls_stream_new;
#endif
	}

	if (git_rwlock_rdunlock(&stream_registration.lock) < 0) {
		giterr_set(GITERR_OS, "failed to unlock stream registration");
		return -1;
	}

	if (!init) {
		giterr_set(GITERR_SSL, "there is no TLS stream available");
		return -1;
	}

	return init(out, host, port);
}

int git_tls_stream_wrap(git_stream **out, git_stream *in, const char *host)
{
	int (*wrap)(git_stream **, git_stream *, const char *) = NULL;

	assert(out && in);

	if (git_rwlock_rdlock(&stream_registration.lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock stream registration");
		return -1;
	}

	if (stream_registration.callbacks.wrap) {
		wrap = stream_registration.callbacks.wrap;
	} else {
#ifdef GIT_SECURE_TRANSPORT
		wrap = git_stransport_stream_wrap;
#elif defined(GIT_OPENSSL)
		wrap = git_openssl_stream_wrap;
#elif defined(GIT_MBEDTLS)
		wrap = git_mbedtls_stream_wrap;
#endif
	}

	if (git_rwlock_rdunlock(&stream_registration.lock) < 0) {
		giterr_set(GITERR_OS, "failed to unlock stream registration");
		return -1;
	}

	if (!wrap) {
		giterr_set(GITERR_SSL, "there is no TLS stream available");
		return -1;
	}

	return wrap(out, in, host);
}
