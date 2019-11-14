/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transports_ssh_h__
#define INCLUDE_transports_ssh_h__

#include "common.h"
#include "../streams/socket.h"

int git_transport_ssh_global_init(void);

#ifdef GIT_SSH

#ifdef GIT_LIBSSH2

#include <libssh2.h>

#define GIT_SSH_ERROR_NONE LIBSSH2_ERROR_NONE

struct git_ssh_session {
	LIBSSH2_SESSION *session;
};

#define git_ssh_session_new() libssh2_session_init()
#define git_ssh_session_disconnect(s, msg) \
	libssh2_session_disconnect(s->session, msg);

struct git_ssh_channel {
	LIBSSH2_CHANNEL *channel;
};

#define git_ssh_channel_close(c) libssh2_channel_close(c->channel)

#endif

typedef struct git_ssh_session git_ssh_session;
typedef struct git_ssh_channel git_ssh_channel;

void git__ssh_error(git_ssh_session *s, const char *errmsg);
void git_ssh__session_free(git_ssh_session *s);
int git_ssh__session_create( git_ssh_session **out_session, git_stream *io);

int _git_ssh_authenticate_session(git_ssh_session *s, git_credential *cred);
int git__ssh_agent_auth(git_ssh_session *s, git_credential_ssh_key *c);
int git__ssh_list_auth_methods(int *out, git_ssh_session *s, const char *username);
int git_ssh_session_server_hostkey(git_ssh_session *s, git_cert_hostkey *cert);

git_ssh_channel *git_ssh_channel_open(git_ssh_session *s);
void git_ssh_channel_free(git_ssh_channel *c);
int git_ssh_channel_read(char *buffer, size_t size, int is_stderr, git_ssh_channel *channel);
int git_ssh_channel_write(git_ssh_channel *channel, const char *buffer, size_t size);
int git_ssh_channel_exec(git_ssh_channel *channel, const char *request);

#endif

#endif
