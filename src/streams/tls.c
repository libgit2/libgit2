/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "common.h"
#include "global.h"
#include "streams/registry.h"
#include "streams/tls.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "streams/stransport.h"

int git_tls_stream_new(git_stream **out, const char *host, const char *port)
{
	int (*init)(git_stream **, const char *, const char *) = NULL;
	git_stream_registration custom = {0};
	int error;

	assert(out && host && port);

	if ((error = git_stream_registry_lookup(&custom, GIT_STREAM_TLS)) == 0) {
		init = custom.init;
	} else if (error == GIT_ENOTFOUND) {
#ifdef GIT_SECURE_TRANSPORT
		init = git_stransport_stream_new;
#elif defined(GIT_OPENSSL)
		init = git_openssl_stream_new;
#elif defined(GIT_MBEDTLS)
		init = git_mbedtls_stream_new;
#endif
	} else {
		return error;
	}

	if (!init) {
		git_error_set(GIT_ERROR_SSL, "there is no TLS stream available");
		return -1;
	}

	return init(out, host, port);
}

int git_tls_stream_wrap(git_stream **out, git_stream *in, const char *host)
{
	int (*wrap)(git_stream **, git_stream *, const char *) = NULL;
	git_stream_registration custom = {0};

	assert(out && in);

	if (git_stream_registry_lookup(&custom, GIT_STREAM_TLS) == 0) {
		wrap = custom.wrap;
	} else {
#ifdef GIT_SECURE_TRANSPORT
		wrap = git_stransport_stream_wrap;
#elif defined(GIT_OPENSSL)
		wrap = git_openssl_stream_wrap;
#elif defined(GIT_MBEDTLS)
		wrap = git_mbedtls_stream_wrap;
#endif
	}

	if (!wrap) {
		git_error_set(GIT_ERROR_SSL, "there is no TLS stream available");
		return -1;
	}

	return wrap(out, in, host);
}

int git_tls_ciphers_foreach(const char **out_name, size_t *out_len, const char **cipher_list)
{
	char *sep;
	assert(out_name && out_len && cipher_list);

	if (*cipher_list == NULL) {
		*out_name = NULL;
		*out_len = 0;
		return GIT_ITEROVER;
	}

	sep = strchr(*cipher_list, ':');
	if (sep != NULL) {
		*out_name = *cipher_list;
		*out_len = sep - *cipher_list;
		*cipher_list = sep + 1;
	} else {
		*out_name = *cipher_list;
		*out_len = strlen(*cipher_list);
		*cipher_list = NULL;
	}

	return 0;
}

int git_tls_cipher_lookup(git_tls_cipher *out, const char *name, size_t namelen)
{
	size_t idx;

	assert(out);

	for (idx = 0; idx < ARRAY_SIZE(git_tls_ciphers); idx++) {
		const git_tls_cipher cipher = git_tls_ciphers[idx];
		if (cipher.nist_name == NULL)
			continue;

		if (strncasecmp(name, cipher.nist_name, namelen) == 0) {
			*out = cipher;
			return 0;
		}
	}

	return GIT_ENOTFOUND;
}
