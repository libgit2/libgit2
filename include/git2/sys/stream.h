/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_stream_h__
#define INCLUDE_sys_git_stream_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/proxy.h"

GIT_BEGIN_DECL

#define GIT_STREAM_VERSION 1

typedef struct {
	unsigned int version;

	/**
	 * Timeout for read and write operations; can be set to `0` to
	 * block indefinitely.
	 */
	int timeout;

	/**
	 * Timeout to connect to the remote server; can be set to `0`
	 * to use the system defaults. This can be shorter than the
	 * system default - often 75 seconds - but cannot be longer.
	 */
	int connect_timeout;
} git_stream_connect_options;

#define GIT_STREAM_CONNECT_OPTIONS_VERSION 1
#define GIT_STREAM_CONNECT_OPTIONS_INIT \
	{ GIT_STREAM_CONNECT_OPTIONS_VERSION }

#ifdef GIT_WIN32
typedef SOCKET git_socket_t;
# define GIT_SOCKET_INVALID INVALID_SOCKET
#else
typedef int git_socket_t;
# define GIT_SOCKET_INVALID -1
#endif

/**
 * Every stream must have this struct as its first element, so the
 * API can talk to it. You'd define your stream as
 *
 *     struct my_stream {
 *             git_stream parent;
 *             ...
 *     }
 *
 * and fill the functions
 */
typedef struct git_stream {
	int version;

	/**
	 * Nonzero if this is a TLS stream; zero if this is plain socket.
	 */
	int encrypted : 1;

	int GIT_CALLBACK(connect)(
		struct git_stream *,
		const char *host,
		const char *port,
		const git_stream_connect_options *opts);
	int GIT_CALLBACK(wrap)(
		struct git_stream *,
		struct git_stream *in,
		const char *host);
	git_socket_t GIT_CALLBACK(get_socket)(struct git_stream *);
	int GIT_CALLBACK(certificate)(git_cert **, struct git_stream *);
	ssize_t GIT_CALLBACK(read)(struct git_stream *, void *, size_t);
	ssize_t GIT_CALLBACK(write)(struct git_stream *, const char *, size_t, int);
	int GIT_CALLBACK(close)(struct git_stream *);
	void GIT_CALLBACK(free)(struct git_stream *);
} git_stream;

typedef struct {
	/** The `version` field should be set to `GIT_STREAM_VERSION`. */
	int version;

	/**
	 * Called to create a new connection to a given host.
	 *
	 * @param out The created stream
	 * @return 0 or an error code
	 */
	int GIT_CALLBACK(init)(git_stream **out);
} git_stream_registration;

/**
 * The type of stream to register.
 */
typedef enum {
	/** A standard (non-TLS) socket. */
	GIT_STREAM_STANDARD = 1,

	/** A TLS-encrypted socket. */
	GIT_STREAM_TLS = 2
} git_stream_t;

/**
 * Register stream constructors for the library to use
 *
 * If a registration structure is already set, it will be overwritten.
 * Pass `NULL` in order to deregister the current constructor and return
 * to the system defaults.
 *
 * The type parameter may be a bitwise AND of types.
 *
 * @param type the type or types of stream to register
 * @param registration the registration data
 * @return 0 or an error code
 */
GIT_EXTERN(int) git_stream_register(
	git_stream_t type, git_stream_registration *registration);

#ifndef GIT_DEPRECATE_HARD

/** @name Deprecated TLS Stream Registration Functions
 *
 * These functions are retained for backward compatibility.  The newer
 * versions of these values should be preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

/**
 * @deprecated Provide a git_stream_registration to git_stream_register
 * @see git_stream_registration
 */
typedef int GIT_CALLBACK(git_stream_cb)(git_stream **out);

/**
 * Register a TLS stream constructor for the library to use.  This stream
 * will not support HTTP CONNECT proxies.  This internally calls
 * `git_stream_register` and is preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Provide a git_stream_registration to git_stream_register
 * @see git_stream_register
 */
GIT_EXTERN(int) git_stream_register_tls(git_stream_cb ctor);

/**@}*/

#endif

GIT_END_DECL

#endif
