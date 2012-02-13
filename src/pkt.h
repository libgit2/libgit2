/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_pkt_h__
#define INCLUDE_pkt_h__

#include "common.h"
#include "transport.h"
#include "buffer.h"
#include "git2/net.h"

enum git_pkt_type {
	GIT_PKT_CMD,
	GIT_PKT_FLUSH,
	GIT_PKT_REF,
	GIT_PKT_HAVE,
	GIT_PKT_ACK,
	GIT_PKT_NAK,
	GIT_PKT_PACK,
	GIT_PKT_COMMENT,
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

int git_pkt_parse_line(git_pkt **head, const char *line, const char **out, size_t len);
int git_pkt_buffer_flush(git_buf *buf);
int git_pkt_send_flush(int s);
int git_pkt_buffer_done(git_buf *buf);
int git_pkt_send_done(int s);
int git_pkt_buffer_wants(const git_vector *refs, git_transport_caps *caps, git_buf *buf);
int git_pkt_send_wants(const git_vector *refs, git_transport_caps *caps, int fd);
int git_pkt_buffer_have(git_oid *oid, git_buf *buf);
int git_pkt_send_have(git_oid *oid, int fd);
void git_pkt_free(git_pkt *pkt);

#endif
