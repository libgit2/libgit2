/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "git2_util.h"
#include "streams/registry.h"
#include "streams/tls.h"
#include "streams/mbedtls.h"
#include "streams/openssl.h"
#include "streams/securetransport.h"
#include "streams/schannel.h"

int git_stream_tls_new(git_stream **out)
{
	int (*init)(git_stream **) = NULL;
	git_stream_registration custom = {0};
	int error;

	GIT_ASSERT_ARG(out);

	if ((error = git_stream_registry_lookup(&custom, GIT_STREAM_TLS)) == 0) {
		init = custom.init;
	} else if (error == GIT_ENOTFOUND) {
#if defined(GIT_HTTPS_SECURETRANSPORT)
		init = git_stream_securetransport_new;
#elif defined(GIT_HTTPS_OPENSSL)
		init = git_stream_openssl_new;
#elif defined(GIT_HTTPS_MBEDTLS)
		init = git_stream_mbedtls_new;
#elif defined(GIT_HTTPS_SCHANNEL)
		init = git_stream_schannel_new;
#endif
	} else {
		return error;
	}

	if (!init) {
		git_error_set(GIT_ERROR_SSL, "there is no TLS stream available");
		return -1;
	}

	return init(out);
}
