/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "ssh.h"

#include "runtime.h"
#include "git2.h"
#include "buffer.h"
#include "net.h"
#include "netops.h"
#include "smart.h"
#include "streams/socket.h"

#include "git2/credential.h"
#include "git2/sys/credential.h"

#ifdef GIT_SSH

#define OWNING_SUBTRANSPORT(s) ((ssh_subtransport *)(s)->parent.subtransport)

static const char *ssh_prefixes[] = { "ssh://", "ssh+git://", "git+ssh://" };

static const char cmd_uploadpack[] = "git-upload-pack";
static const char cmd_receivepack[] = "git-receive-pack";

typedef struct {
	git_smart_subtransport_stream parent;
	git_stream *io;
	git_ssh_session *session;
	git_ssh_channel *channel;
	const char *cmd;
	char *url;
	unsigned sent_command : 1;
} ssh_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	ssh_stream *current_stream;
	git_credential *cred;
	char *cmd_uploadpack;
	char *cmd_receivepack;
} ssh_subtransport;

/*
 * Create a git protocol request.
 *
 * For example: git-upload-pack '/libgit2/libgit2'
 */
static int gen_proto(git_buf *request, const char *cmd, const char *url)
{
	const char *repo;
	int len;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ssh_prefixes); ++i) {
		const char *p = ssh_prefixes[i];

		if (!git__prefixcmp(url, p)) {
			url = url + strlen(p);
			repo = strchr(url, '/');
			if (repo && repo[1] == '~')
				++repo;

			goto done;
		}
	}
	repo = strchr(url, ':');
	if (repo) repo++;

done:
	if (!repo) {
		git_error_set(GIT_ERROR_NET, "malformed git protocol URL");
		return -1;
	}

	len = strlen(cmd) + 1 /* Space */ + 1 /* Quote */ + strlen(repo) + 1 /* Quote */ + 1;

	git_buf_grow(request, len);
	git_buf_puts(request, cmd);
	git_buf_puts(request, " '");
	git_buf_decode_percent(request, repo, strlen(repo));
	git_buf_puts(request, "'");

	if (git_buf_oom(request))
		return -1;

	return 0;
}

static int send_command(ssh_stream *s)
{
	int error;
	git_buf request = GIT_BUF_INIT;

	error = gen_proto(&request, s->cmd, s->url);
	if (error < 0)
		goto cleanup;

	error = git_ssh_channel_exec(s->channel, request.ptr);
	if (error < GIT_SSH_ERROR_NONE) {
		git__ssh_error(s->session, "SSH could not execute request");
		goto cleanup;
	}

	s->sent_command = 1;

cleanup:
	git_buf_dispose(&request);
	return error;
}

static int ssh_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	int rc;
	ssh_stream *s = GIT_CONTAINER_OF(stream, ssh_stream, parent);

	*bytes_read = 0;

	if (!s->sent_command && send_command(s) < 0)
		return -1;

	if ((rc = git_ssh_channel_read(buffer, buf_size, 0, s->channel)) < GIT_SSH_ERROR_NONE) {
		git__ssh_error(s->session, "SSH could not read data");
		return -1;
	}

	/*
	 * If we can't get anything out of stdout, it's typically a
	 * not-found error, so read from stderr and signal EOF on
	 * stderr.
	 */
	if (rc == 0) {
		if ((rc = git_ssh_channel_read(buffer, buf_size, 1, s->channel)) > 0) {
			git_error_set(GIT_ERROR_SSH, "%*s", rc, buffer);
			return GIT_EEOF;
		} else if (rc < GIT_SSH_ERROR_NONE) {
			git__ssh_error(s->session, "SSH could not read stderr");
			return -1;
		}
	}


	*bytes_read = rc;

	return 0;
}

static int ssh_stream_write(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	ssh_stream *s = GIT_CONTAINER_OF(stream, ssh_stream, parent);
	size_t off = 0;
	ssize_t ret = 0;

	if (!s->sent_command && send_command(s) < 0)
		return -1;

	do {
		ret = git_ssh_channel_write(s->channel, buffer + off, len - off);
		if (ret < 0)
			break;

		off += ret;

	} while (off < len);

	if (ret < 0) {
		git__ssh_error(s->session, "SSH could not write data");
		return -1;
	}

	return 0;
}

static void ssh_stream_free(git_smart_subtransport_stream *stream)
{
	ssh_stream *s = GIT_CONTAINER_OF(stream, ssh_stream, parent);
	ssh_subtransport *t;

	if (!stream)
		return;

	t = OWNING_SUBTRANSPORT(s);
	t->current_stream = NULL;

	if (s->channel) {
		git_ssh_channel_close(s->channel);
		git_ssh_channel_free(s->channel);
		s->channel = NULL;
	}

	if (s->session) {
		git_ssh_session_disconnect(s->session, "closing transport");
		git_ssh__session_free(s->session);
		s->session = NULL;
	}

	if (s->io) {
		git_stream_close(s->io);
		git_stream_free(s->io);
		s->io = NULL;
	}

	git__free(s->url);
	git__free(s);
}

static int ssh_stream_alloc(
	ssh_subtransport *t,
	const char *url,
	const char *cmd,
	git_smart_subtransport_stream **stream)
{
	ssh_stream *s;

	GIT_ASSERT_ARG(stream);

	s = git__calloc(sizeof(ssh_stream), 1);
	GIT_ERROR_CHECK_ALLOC(s);

	s->parent.subtransport = &t->parent;
	s->parent.read = ssh_stream_read;
	s->parent.write = ssh_stream_write;
	s->parent.free = ssh_stream_free;

	s->cmd = cmd;

	s->url = git__strdup(url);
	if (!s->url) {
		git__free(s);
		return -1;
	}

	*stream = &s->parent;
	return 0;
}

static int git_ssh_extract_url_parts(
	git_net_url *urldata,
	const char *url)
{
	char *colon, *at;
	const char *start;

	colon = strchr(url, ':');


	at = strchr(url, '@');
	if (at) {
		start = at + 1;
		urldata->username = git__substrdup(url, at - url);
		GIT_ERROR_CHECK_ALLOC(urldata->username);
	} else {
		start = url;
		urldata->username = NULL;
	}

	if (colon == NULL || (colon < start)) {
		git_error_set(GIT_ERROR_NET, "malformed URL");
		return -1;
	}

	urldata->host = git__substrdup(start, colon - start);
	GIT_ERROR_CHECK_ALLOC(urldata->host);

	return 0;
}

static int request_creds(git_credential **out, ssh_subtransport *t, const char *user, int auth_methods)
{
	int error, no_callback = 0;
	git_credential *cred = NULL;

	if (!t->owner->cred_acquire_cb) {
		no_callback = 1;
	} else {
		error = t->owner->cred_acquire_cb(&cred, t->owner->url, user, auth_methods,
						  t->owner->cred_acquire_payload);

		if (error == GIT_PASSTHROUGH) {
			no_callback = 1;
		} else if (error < 0) {
			return error;
		} else if (!cred) {
			git_error_set(GIT_ERROR_SSH, "callback failed to initialize SSH credentials");
			return -1;
		}
	}

	if (no_callback) {
		git_error_set(GIT_ERROR_SSH, "authentication required but no callback set");
		return -1;
	}

	if (!(cred->credtype & auth_methods)) {
		cred->free(cred);
		git_error_set(GIT_ERROR_SSH, "callback returned unsupported credentials type");
		return -1;
	}

	*out = cred;

	return 0;
}

#define SSH_DEFAULT_PORT "22"

static int _git_ssh_setup_conn(
	ssh_subtransport *t,
	const char *url,
	const char *cmd,
	git_smart_subtransport_stream **stream)
{
	git_net_url urldata = GIT_NET_URL_INIT;
	int auth_methods, error = 0;
	size_t i;
	ssh_stream *s;
	git_credential *cred = NULL;
	git_ssh_session *session = NULL;
	git_ssh_channel *channel = NULL;

	t->current_stream = NULL;

	*stream = NULL;
	if (ssh_stream_alloc(t, url, cmd, stream) < 0)
		return -1;

	s = (ssh_stream *)*stream;
	s->session = NULL;
	s->channel = NULL;

	for (i = 0; i < ARRAY_SIZE(ssh_prefixes); ++i) {
		const char *p = ssh_prefixes[i];

		if (!git__prefixcmp(url, p)) {
			if ((error = git_net_url_parse(&urldata, url)) < 0)
				goto done;

			goto post_extract;
		}
	}
	if ((error = git_ssh_extract_url_parts(&urldata, url)) < 0)
		goto done;

	if (urldata.port == NULL)
		urldata.port = git__strdup(SSH_DEFAULT_PORT);

	GIT_ERROR_CHECK_ALLOC(urldata.port);

post_extract:
	if ((error = git_socket_stream_new(&s->io, urldata.host, urldata.port)) < 0 ||
	    (error = git_stream_connect(s->io)) < 0)
		goto done;

	if ((error = git_ssh__session_create(&session, s->io)) < 0)
		goto done;

	if (t->owner->certificate_check_cb != NULL) {
		git_cert_hostkey cert = {{ 0 }};
		int valid;

		if ((error = git_ssh_session_server_hostkey(session, &cert)) < 0)
			goto done;

		/* We don't currently trust any hostkeys */
		if ((error = git_ssh_session_server_is_known(session, &valid)) < 0)
			goto done;

		git_error_clear();
		error = t->owner->certificate_check_cb((git_cert *) &cert, valid, urldata.host, t->owner->message_cb_payload);

		if (error < 0 && error != GIT_PASSTHROUGH) {
			if (!git_error_last())
				git_error_set(GIT_ERROR_NET, "user cancelled hostkey check");

			goto done;
		}
	}

	/* we need the username to ask for auth methods */
	if (!urldata.username) {
		if ((error = request_creds(&cred, t, NULL, GIT_CREDENTIAL_USERNAME)) < 0)
			goto done;

		urldata.username = git__strdup(((git_credential_username *) cred)->username);
		cred->free(cred);
		cred = NULL;
		if (!urldata.username)
			goto done;
	} else if (urldata.username && urldata.password) {
		if ((error = git_credential_userpass_plaintext_new(&cred, urldata.username, urldata.password)) < 0)
			goto done;
	}

	if ((error = git__ssh_list_auth_methods(&auth_methods, session, urldata.username)) < 0)
		goto done;

	error = GIT_EAUTH;
	/* if we already have something to try */
	if (cred && auth_methods & cred->credtype)
		error = _git_ssh_authenticate_session(session, cred);

	while (error == GIT_EAUTH) {
		if (cred) {
			cred->free(cred);
			cred = NULL;
		}

		if ((error = request_creds(&cred, t, urldata.username, auth_methods)) < 0)
			goto done;

		if (strcmp(urldata.username, git_credential_get_username(cred))) {
			git_error_set(GIT_ERROR_SSH, "username does not match previous request");
			error = -1;
			goto done;
		}

		error = _git_ssh_authenticate_session(session, cred);

		if (error == GIT_EAUTH) {
			/* refresh auth methods */
			if ((error = git__ssh_list_auth_methods(&auth_methods, session, urldata.username)) < 0)
				goto done;
			else
				error = GIT_EAUTH;
		}
	}

	if (error < 0)
		goto done;

	channel = git_ssh_channel_open(session);
	if (!channel) {
		error = -1;
		git__ssh_error(session, "Failed to open SSH channel");
		goto done;
	}

	s->session = session;
	s->channel = channel;

	t->current_stream = s;

done:
	if (error < 0) {
		ssh_stream_free(*stream);

		if (session)
			git_ssh__session_free(session);
	}

	if (cred)
		cred->free(cred);

	git_net_url_dispose(&urldata);

	return error;
}

static int ssh_uploadpack_ls(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	const char *cmd = t->cmd_uploadpack ? t->cmd_uploadpack : cmd_uploadpack;

	return _git_ssh_setup_conn(t, url, cmd, stream);
}

static int ssh_uploadpack(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	GIT_UNUSED(url);

	if (t->current_stream) {
		*stream = &t->current_stream->parent;
		return 0;
	}

	git_error_set(GIT_ERROR_NET, "must call UPLOADPACK_LS before UPLOADPACK");
	return -1;
}

static int ssh_receivepack_ls(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	const char *cmd = t->cmd_receivepack ? t->cmd_receivepack : cmd_receivepack;


	return _git_ssh_setup_conn(t, url, cmd, stream);
}

static int ssh_receivepack(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	GIT_UNUSED(url);

	if (t->current_stream) {
		*stream = &t->current_stream->parent;
		return 0;
	}

	git_error_set(GIT_ERROR_NET, "must call RECEIVEPACK_LS before RECEIVEPACK");
	return -1;
}

static int _ssh_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *subtransport,
	const char *url,
	git_smart_service_t action)
{
	ssh_subtransport *t = GIT_CONTAINER_OF(subtransport, ssh_subtransport, parent);

	switch (action) {
		case GIT_SERVICE_UPLOADPACK_LS:
			return ssh_uploadpack_ls(t, url, stream);

		case GIT_SERVICE_UPLOADPACK:
			return ssh_uploadpack(t, url, stream);

		case GIT_SERVICE_RECEIVEPACK_LS:
			return ssh_receivepack_ls(t, url, stream);

		case GIT_SERVICE_RECEIVEPACK:
			return ssh_receivepack(t, url, stream);
	}

	*stream = NULL;
	return -1;
}

static int _ssh_close(git_smart_subtransport *subtransport)
{
	ssh_subtransport *t = GIT_CONTAINER_OF(subtransport, ssh_subtransport, parent);

	GIT_ASSERT(!t->current_stream);

	GIT_UNUSED(t);

	return 0;
}

static void _ssh_free(git_smart_subtransport *subtransport)
{
	ssh_subtransport *t = GIT_CONTAINER_OF(subtransport, ssh_subtransport, parent);

	git__free(t->cmd_uploadpack);
	git__free(t->cmd_receivepack);
	git__free(t);
}
#endif

int git_smart_subtransport_ssh(
	git_smart_subtransport **out, git_transport *owner, void *param)
{
#ifdef GIT_SSH
	ssh_subtransport *t;

	GIT_ASSERT_ARG(out);

	GIT_UNUSED(param);

	t = git__calloc(sizeof(ssh_subtransport), 1);
	GIT_ERROR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = _ssh_action;
	t->parent.close = _ssh_close;
	t->parent.free = _ssh_free;

	*out = (git_smart_subtransport *) t;
	return 0;
#else
	GIT_UNUSED(owner);
	GIT_UNUSED(param);

	GIT_ASSERT_ARG(out);
	*out = NULL;

	git_error_set(GIT_ERROR_INVALID, "cannot create SSH transport. Library was built without SSH support");
	return -1;
#endif
}

int git_transport_ssh_with_paths(git_transport **out, git_remote *owner, void *payload)
{
#ifdef GIT_SSH
	git_strarray *paths = (git_strarray *) payload;
	git_transport *transport;
	transport_smart *smart;
	ssh_subtransport *t;
	int error;
	git_smart_subtransport_definition ssh_definition = {
		git_smart_subtransport_ssh,
		0, /* no RPC */
		NULL,
	};

	if (paths->count != 2) {
		git_error_set(GIT_ERROR_SSH, "invalid ssh paths, must be two strings");
		return GIT_EINVALIDSPEC;
	}

	if ((error = git_transport_smart(&transport, owner, &ssh_definition)) < 0)
		return error;

	smart = (transport_smart *) transport;
	t = (ssh_subtransport *) smart->wrapped;

	t->cmd_uploadpack = git__strdup(paths->strings[0]);
	GIT_ERROR_CHECK_ALLOC(t->cmd_uploadpack);
	t->cmd_receivepack = git__strdup(paths->strings[1]);
	GIT_ERROR_CHECK_ALLOC(t->cmd_receivepack);

	*out = transport;
	return 0;
#else
	GIT_UNUSED(owner);
	GIT_UNUSED(payload);

	GIT_ASSERT_ARG(out);
	*out = NULL;

	git_error_set(GIT_ERROR_INVALID, "cannot create SSH transport. Library was built without SSH support");
	return -1;
#endif
}

#ifdef GIT_SSH
static void shutdown_ssh(void)
{
#ifdef GIT_LIBSSH2
	libssh2_exit();
#elif defined(GIT_LIBSSH)
	ssh_finalize();
#endif
}
#endif

int git_transport_ssh_global_init(void)
{
#ifdef GIT_SSH
#ifdef GIT_LIBSSH2
	if (libssh2_init(0) < 0) {
		git_error_set(GIT_ERROR_SSH, "unable to initialize libssh2");
		return -1;
	}
#elif defined(GIT_LIBSSH)
	if (ssh_init() < 0) {
		git_error_set(GIT_ERROR_SSH, "unable to initialize libssh");
		return -1;
	}
#endif

	return git_runtime_shutdown_register(shutdown_ssh);

#else

	/* Nothing to initialize */
	return 0;

#endif
}
