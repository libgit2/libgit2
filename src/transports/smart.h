/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "git2.h"
#include "vector.h"
#include "netops.h"
#include "buffer.h"

#define GIT_SIDE_BAND_DATA     1
#define GIT_SIDE_BAND_PROGRESS 2
#define GIT_SIDE_BAND_ERROR    3

#define GIT_CAP_OFS_DELTA "ofs-delta"
#define GIT_CAP_MULTI_ACK "multi_ack"
#define GIT_CAP_SIDE_BAND "side-band"
#define GIT_CAP_SIDE_BAND_64K "side-band-64k"
#define GIT_CAP_INCLUDE_TAG "include-tag"

enum git_pkt_type {
	GIT_PKT_CMD,
	GIT_PKT_FLUSH,
	GIT_PKT_REF,
	GIT_PKT_HAVE,
	GIT_PKT_ACK,
	GIT_PKT_NAK,
	GIT_PKT_PACK,
	GIT_PKT_COMMENT,
	GIT_PKT_ERR,
	GIT_PKT_DATA,
	GIT_PKT_PROGRESS,
};

/* Used for multi-ack */
enum git_ack_status {
	GIT_ACK_NONE,
	GIT_ACK_CONTINUE,
	GIT_ACK_COMMON,
	GIT_ACK_READY
};

/* This would be a flush pkt */
typedef struct {
	enum git_pkt_type type;
} git_pkt;

struct git_pkt_cmd {
	enum git_pkt_type type;
	char *cmd;
	char *path;
	char *host;
};

/* This is a pkt-line with some info in it */
typedef struct {
	enum git_pkt_type type;
	git_remote_head head;
	char *capabilities;
} git_pkt_ref;

/* Useful later */
typedef struct {
	enum git_pkt_type type;
	git_oid oid;
	enum git_ack_status status;
} git_pkt_ack;

typedef struct {
	enum git_pkt_type type;
	char comment[GIT_FLEX_ARRAY];
} git_pkt_comment;

typedef struct {
	enum git_pkt_type type;
	int len;
	char data[GIT_FLEX_ARRAY];
} git_pkt_data;

typedef git_pkt_data git_pkt_progress;

typedef struct {
	enum git_pkt_type type;
	char error[GIT_FLEX_ARRAY];
} git_pkt_err;

typedef struct transport_smart_caps {
	int common:1,
		ofs_delta:1,
		multi_ack: 1,
		side_band:1,
		side_band_64k:1,
		include_tag:1;
} transport_smart_caps;

typedef void (*packetsize_cb)(size_t received, void *payload);

typedef struct {
	git_transport parent;
	char *url;
	git_cred_acquire_cb cred_acquire_cb;
	int direction;
	int flags;
	git_transport_message_cb progress_cb;
	git_transport_message_cb error_cb;
	void *message_cb_payload;
	git_smart_subtransport *wrapped;
	git_smart_subtransport_stream *current_stream;
	transport_smart_caps caps;
	git_vector refs;
	git_vector common;
	git_atomic cancelled;
	packetsize_cb packetsize_cb;
	void *packetsize_payload;
	unsigned rpc : 1,
		have_refs : 1,
		connected : 1;
	gitno_buffer buffer;
	char buffer_data[65536];
} transport_smart;

/* smart_protocol.c */
int git_smart__store_refs(transport_smart *t, int flushes);
int git_smart__detect_caps(git_pkt_ref *pkt, transport_smart_caps *caps);

int git_smart__negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_remote_head * const *refs,
	size_t count);

int git_smart__download_pack(
	git_transport *transport,
	git_repository *repo,
	git_transfer_progress *stats,
	git_transfer_progress_callback progress_cb,
	void *progress_payload);

/* smart.c */
int git_smart__negotiation_step(git_transport *transport, void *data, size_t len);

/* smart_pkt.c */
int git_pkt_parse_line(git_pkt **head, const char *line, const char **out, size_t len);
int git_pkt_buffer_flush(git_buf *buf);
int git_pkt_send_flush(GIT_SOCKET s);
int git_pkt_buffer_done(git_buf *buf);
int git_pkt_buffer_wants(const git_remote_head * const *refs, size_t count, transport_smart_caps *caps, git_buf *buf);
int git_pkt_buffer_have(git_oid *oid, git_buf *buf);
void git_pkt_free(git_pkt *pkt);
