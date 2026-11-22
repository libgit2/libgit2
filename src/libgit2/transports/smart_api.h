/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transports_smart_api_h__
#define INCLUDE_transports_smart_api_h__

#include "git2/sys/transport.h"

/*
 * The interface for building a transport on the smart protocol.  This
 * uses only public types and is intended for `git2/sys/transport.h`.
 */

/*
 * A bidirectional stream carrying the smart protocol.  Streams are created
 * and owned by transports; the protocol code only reads and writes them.
 */
typedef git_smart_subtransport_stream git_transport_smart_stream;

/* The state of a smart protocol conversation. */
typedef struct git_transport_smart_session git_transport_smart_session;

/*
 * Called by the negotiation code to send one round of data to the server.
 * The implementation returns the stream that will carry the response:
 * stateful transports write to their existing stream, stateless ones
 * start a new request.
 */
typedef int (*git_transport_smart_round_cb)(
	git_transport_smart_stream **out,
	const char *data,
	size_t len,
	void *payload);

/*
 * The base for transports that speak the smart protocol.  It provides the
 * `git_transport` methods that do not depend on the connection.
 */
typedef struct {
	git_transport parent;
	git_transport_smart_session *session;
} git_transport_smart_base;

int git_transport_smart_base_init(
	git_transport_smart_base *t,
	git_remote *owner,
	int stateless);
void git_transport_smart_base_dispose(git_transport_smart_base *t);
int git_transport_smart_base_prepare_connect(
	git_transport_smart_base *t,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts);
int git_transport_smart_base_ensure_direction(
	git_transport_smart_base *t,
	int direction);
const char *git_transport_smart_base_url(git_transport_smart_base *t);
void git_transport_smart_base_set_connected(
	git_transport_smart_base *t,
	int connected);

/* Forgets the server's capabilities, before connecting again. */
void git_transport_smart_session_reset(git_transport_smart_session *s);

/* Ends the conversation; refs that were advertised remain available. */
void git_transport_smart_session_close(git_transport_smart_session *s);

int git_transport_smart_read_advertisement(
	git_transport_smart_session *s,
	git_transport_smart_stream *stream);

int git_transport_smart_negotiate(
	git_transport_smart_session *s,
	git_repository *repo,
	const git_fetch_negotiation *wants,
	git_transport_smart_round_cb round,
	void *round_payload);

int git_transport_smart_download_pack(
	git_transport_smart_session *s,
	git_transport_smart_stream *stream,
	git_repository *repo,
	git_indexer_progress *stats);

/* Must be called before opening the stream given to `push_send`. */
int git_transport_smart_push_prepare(
	git_transport_smart_session *s,
	git_push *push);

int git_transport_smart_push_send(
	git_transport_smart_session *s,
	git_transport_smart_stream *stream,
	git_push *push);

/* Tells the server of a stateful transport that we are done. */
void git_transport_smart_goodbye(git_transport_smart_stream *stream);

#endif
