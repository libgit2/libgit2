/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_streams_tls_h__
#define INCLUDE_streams_tls_h__

#include "common.h"

#include "git2/sys/stream.h"

/**
 * A TLS cipher
 *
 * Only used to provide mappings from GIT_OPT_SET_SSL_CIPHERS names to
 * what the TLS backend uses.
 */
#if defined(GIT_MBEDTLS) || defined(GIT_SECURE_TRANSPORT)

typedef struct {
	uint32_t value;
	const char *nist_name;
} git_tls_cipher;

#elif defined(GIT_OPENSSL)

typedef struct {
	const char *openssl_name;
	const char *nist_name;
} git_tls_cipher;

#endif

#include "tls_ciphers.h"

/**
 * Create a TLS stream with the most appropriate backend available for
 * the current platform, whether that's SecureTransport on macOS,
 * OpenSSL or mbedTLS on other Unixes, or something else entirely.
 */
extern int git_tls_stream_new(git_stream **out, const char *host, const char *port);

/**
 * Create a TLS stream on top of an existing insecure stream, using
 * the most appropriate backend available for the current platform.
 *
 * This allows us to create a CONNECT stream on top of a proxy;
 * using SecureTransport on macOS, OpenSSL or mbedTLS on other
 * Unixes, or something else entirely.
 */
extern int git_tls_stream_wrap(git_stream **out, git_stream *in, const char *host);

/**
 * Parse a list of cipher names from cipher_list.
 *
 * The intended usage is to pass a pointer to a list of cipher; either the
 * default list of ciphers (GIT_TLS_DEFAULT_CIPHERS) or the list configured by
 * GIT_OPT_SET_SSL_CIPHERS.
 *
 * As this function must work with read-only data, the name is never NUL-terminated.
 *
 * @param out_name the cipher name
 * @param out_len the cipher name length
 * @param cipher_list the current position in the cipher list
 * @return 0 on success, GIT_ITEROVER if the list is empty
 */
GIT_EXTERN(int) git_tls_ciphers_foreach(const char **out_name, size_t *out_len, const char **cipher_list);

/**
 * Lookup cipher information by name
 *
 * @param out the cipher corresponding to name
 * @param name the cipher name to lookup
 * @param namelen the cipher name length
 * @return 0 on success, GIT_ENOTFOUND otherwise
 */
GIT_EXTERN(int) git_tls_cipher_lookup(git_tls_cipher *out, const char *name, size_t namelen);

#endif
