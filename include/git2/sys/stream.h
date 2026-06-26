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

/**
 * @file git2/sys/stream.h
 * @brief Streaming file I/O functionality
 * @defgroup git_stream Streaming file I/O functionality
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** A git I/O stream. */
typedef struct git_stream git_stream;

/**
 * Connect the stream
 *
 * @param stream the stream to connect
 * @return 0 on success, or an error code
 */
typedef int GIT_CALLBACK(git_stream_connect_cb)(git_stream *stream);

/**
 * Certificate lookup for the stream
 *
 * @param[out] out the certificate
 * @param stream the stream to get the certificate for
 * @return 0 on success, or an error code
 */
typedef int GIT_CALLBACK(git_stream_certificate_cb)(
	git_cert **out,
	git_stream *stream);

/**
 * Set the proxy for the stream
 *
 * @param stream the stream to set the proxy for
 * @param proxy_opts the proxy options
 * @return 0 on success, or an error code
 */
typedef int GIT_CALLBACK(git_stream_set_proxy_cb)(
	git_stream *stream,
	const git_proxy_options *proxy_opts);

/**
 * Read from the stream.
 *
 * @param stream the stream to read from
 * @param buf the buffer to write to
 * @param size the size of the buffer
 * @return the number of bytes read, or an error code
 */
typedef ssize_t GIT_CALLBACK(git_stream_read_cb)(
	git_stream *stream, void *buf, size_t size);

/**
 * Write to the stream
 *
 * @param stream the stream to write to
 * @param data the data to write
 * @param len the length of the data to write
 * @param flags the write flags
 * @return the number of bytes written, or an error code
 */
typedef ssize_t GIT_CALLBACK(git_stream_write_cb)(
	git_stream *stream,
	const char *data,
	size_t len,
	int flags);

/**
 * Close the stream
 *
 * @param stream the stream to close
 * @return 0 on succes, or an error code
 */
typedef int GIT_CALLBACK(git_stream_close_cb)(git_stream *stream);

/**
 * Free the stream
 *
 * @param stream the stream to free
 */
typedef void GIT_CALLBACK(git_stream_free_cb)(git_stream *stream);

/** Current version for the `git_stream` structures */
#define GIT_STREAM_VERSION 1

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
struct git_stream {
	int version;

	unsigned int encrypted : 1,
	             proxy_support : 1;

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

	/** Callback to connect the stream. */
	git_stream_connect_cb connect;

	/** Certificate lookup. */
	git_stream_certificate_cb certificate;

	/** Set the proxy for the stream. */
	git_stream_set_proxy_cb set_proxy;

	/** Read from the stream. */
	git_stream_read_cb read;

	/** Write to the stream. */
	git_stream_write_cb write;

	/** Close the stream. */
	git_stream_close_cb close;

	/** Free the stream. */
	git_stream_free_cb free;
};

typedef struct {
	/** The `version` field should be set to `GIT_STREAM_VERSION`. */
	int version;

	/**
	 * Called to create a new connection to a given host.
	 *
	 * @param out The created stream
	 * @param host The hostname to connect to; may be a hostname or
	 *             IP address
	 * @param port The port to connect to; may be a port number or
	 *             service name
	 * @return 0 or an error code
	 */
	int GIT_CALLBACK(init)(git_stream **out, const char *host, const char *port);

	/**
	 * Called to create a new connection on top of the given stream.  If
	 * this is a TLS stream, then this function may be used to proxy a
	 * TLS stream over an HTTP CONNECT session.  If this is unset, then
	 * HTTP CONNECT proxies will not be supported.
	 *
	 * @param out The created stream
	 * @param in An existing stream to add TLS to
	 * @param host The hostname that the stream is connected to,
	 *             for certificate validation
	 * @return 0 or an error code
	 */
	int GIT_CALLBACK(wrap)(git_stream **out, git_stream *in, const char *host);
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
typedef int GIT_CALLBACK(git_stream_cb)(git_stream **out, const char *host, const char *port);

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

/**@}*/
GIT_END_DECL

/** @} */
GIT_END_DECL

#endif
