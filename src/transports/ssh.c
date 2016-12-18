/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef GIT_WIN32
#include <dlfcn.h>
#endif

#include "git2.h"
#include "buffer.h"
#include "netops.h"
#include "smart.h"
#include "cred.h"
#include "socket_stream.h"
#include "global.h"
#include "ssh.h"

#ifdef GIT_SSH

#include "libssh2.h"

/* These macros allow us to refer to libssh2 symbols while
 * supporting dynamic linking.
 */
#ifdef GIT_WIN32

static HMODULE git_libssh2_handle;

#define LOOKUP_LIBSSH2_SYMBOL(symbol) \
	git_ ## symbol = GetProcAddress(git_libssh2_handle, #symbol)

#else

static void *git_libssh2_handle;

#define LOOKUP_LIBSSH2_SYMBOL(symbol) \
	git_ ## symbol = dlsym(git_libssh2_handle, #symbol)

#endif

static int (* git_libssh2_agent_connect)(LIBSSH2_AGENT *agent);
static int (* git_libssh2_agent_disconnect)(LIBSSH2_AGENT *agent);
static void (* git_libssh2_agent_free)(LIBSSH2_AGENT *agent);
static int (* git_libssh2_agent_get_identity)(LIBSSH2_AGENT *agent, struct libssh2_agent_publickey **store, struct libssh2_agent_publickey *prev);
static LIBSSH2_AGENT * (* git_libssh2_agent_init)(LIBSSH2_SESSION *session);
static int (* git_libssh2_agent_list_identities)(LIBSSH2_AGENT *agent);
static int (* git_libssh2_agent_userauth)(LIBSSH2_AGENT *agent, const char *username, struct libssh2_agent_publickey *identity);
static int (* git_libssh2_channel_close)(LIBSSH2_CHANNEL *channel);
static int (* git_libssh2_channel_free)(LIBSSH2_CHANNEL *channel);
static LIBSSH2_CHANNEL * (*git_libssh2_channel_open_ex)(LIBSSH2_SESSION *session, const char *channel_type, unsigned int channel_type_len, unsigned int window_size, unsigned int packet_size, const char *message, unsigned int message_len);
static int (* git_libssh2_channel_process_startup)(LIBSSH2_CHANNEL *channel, const char *request, unsigned int request_len, const char *message, unsigned int message_len);
static ssize_t (* git_libssh2_channel_read_ex)(LIBSSH2_CHANNEL *channel, int stream_id, char *buf, size_t buflen);
static void (* git_libssh2_channel_set_blocking)(LIBSSH2_CHANNEL *channel, int blocking);
static ssize_t (* git_libssh2_channel_write_ex)(LIBSSH2_CHANNEL *channel, int stream_id, const char *buf, size_t buflen);
static const char * (* git_libssh2_hostkey_hash)(LIBSSH2_SESSION *session, int hash_type);
static int (* git_libssh2_init)(int flags);
static void ** (*git_libssh2_session_abstract)(LIBSSH2_SESSION *session);
static int (* git_libssh2_session_free)(LIBSSH2_SESSION *session);
static LIBSSH2_SESSION * (* git_libssh2_session_init_ex)(LIBSSH2_ALLOC_FUNC((*my_alloc)), LIBSSH2_FREE_FUNC((*my_free)), LIBSSH2_REALLOC_FUNC((*my_realloc)), void *abstract);
static int (*git_libssh2_session_last_error)(LIBSSH2_SESSION *session, char **errmsg, int *errmsg_len, int want_buf);
static void (*git_libssh2_session_set_blocking)(LIBSSH2_SESSION* session, int blocking);
static int (*git_libssh2_session_startup)(LIBSSH2_SESSION *session, int sock);
static int (*git_libssh2_userauth_authenticated)(LIBSSH2_SESSION *session);
static int (*git_libssh2_userauth_keyboard_interactive_ex)(LIBSSH2_SESSION* session, const char *username, unsigned int username_len, LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC((*response_callback)));
static char * (*git_libssh2_userauth_list)(LIBSSH2_SESSION *session, const char *username, unsigned int username_len);
static int (*git_libssh2_userauth_password_ex)(LIBSSH2_SESSION *session, const char *username, unsigned int username_len, const char *password, unsigned int password_len, LIBSSH2_PASSWD_CHANGEREQ_FUNC((*passwd_change_cb)));
static int (*git_libssh2_userauth_publickey)(LIBSSH2_SESSION *session, const char *username, const unsigned char *pubkeydata, size_t pubkeydata_len, LIBSSH2_USERAUTH_PUBLICKEY_SIGN_FUNC((*sign_callback)), void **abstract);
static int (*git_libssh2_userauth_publickey_fromfile_ex)(LIBSSH2_SESSION *session, const char *username, unsigned int username_len, const char *publickey, const char *privatekey, const char *passphrase);
static int (*git_libssh2_userauth_publickey_frommemory)(LIBSSH2_SESSION *session, const char *username, size_t username_len, const char *publickeyfiledata, size_t publickeyfiledata_len, const char *privatekeyfiledata, size_t privatekeyfiledata_len, const char *passphrase);

/*
 * The simpler API are macros that call to the _ex variant of the functions.
 * This makes those macros use our variables to call the functions instead.
 */
#define libssh2_channel_open_ex git_libssh2_channel_open_ex
#define libssh2_session_init_ex git_libssh2_session_init_ex
#define libssh2_userauth_keyboard_interactive_ex git_libssh2_userauth_keyboard_interactive_ex
#define libssh2_userauth_password_ex git_libssh2_userauth_password_ex
#define libssh2_userauth_publickey_fromfile_ex git_libssh2_userauth_publickey_fromfile_ex
#define libssh2_channel_process_startup git_libssh2_channel_process_startup
#define libssh2_channel_read_ex git_libssh2_channel_read_ex
#define libssh2_channel_write_ex git_libssh2_channel_write_ex

#define OWNING_SUBTRANSPORT(s) ((ssh_subtransport *)(s)->parent.subtransport)

static const char *ssh_prefixes[] = { "ssh://", "ssh+git://", "git+ssh://" };

static const char cmd_uploadpack[] = "git-upload-pack";
static const char cmd_receivepack[] = "git-receive-pack";

typedef struct {
	git_smart_subtransport_stream parent;
	git_stream *io;
	LIBSSH2_SESSION *session;
	LIBSSH2_CHANNEL *channel;
	const char *cmd;
	char *url;
	unsigned sent_command : 1;
} ssh_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	ssh_stream *current_stream;
	git_cred *cred;
	char *cmd_uploadpack;
	char *cmd_receivepack;
} ssh_subtransport;

static int list_auth_methods(int *out, LIBSSH2_SESSION *session, const char *username);

static void ssh_error(LIBSSH2_SESSION *session, const char *errmsg)
{
	char *ssherr;
	git_libssh2_session_last_error(session, &ssherr, NULL, 0);

	giterr_set(GITERR_SSH, "%s: %s", errmsg, ssherr);
}

/*
 * Create a git protocol request.
 *
 * For example: git-upload-pack '/libgit2/libgit2'
 */
static int gen_proto(git_buf *request, const char *cmd, const char *url)
{
	char *repo;
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
		giterr_set(GITERR_NET, "malformed git protocol URL");
		return -1;
	}

	len = strlen(cmd) + 1 /* Space */ + 1 /* Quote */ + strlen(repo) + 1 /* Quote */ + 1;

	git_buf_grow(request, len);
	git_buf_printf(request, "%s '%s'", cmd, repo);
	git_buf_putc(request, '\0');

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

	error = libssh2_channel_exec(s->channel, request.ptr);
	if (error < LIBSSH2_ERROR_NONE) {
		ssh_error(s->session, "SSH could not execute request");
		goto cleanup;
	}

	s->sent_command = 1;

cleanup:
	git_buf_free(&request);
	return error;
}

static int ssh_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	int rc;
	ssh_stream *s = (ssh_stream *)stream;

	*bytes_read = 0;

	if (!s->sent_command && send_command(s) < 0)
		return -1;

	if ((rc = libssh2_channel_read(s->channel, buffer, buf_size)) < LIBSSH2_ERROR_NONE) {
		ssh_error(s->session, "SSH could not read data");
		return -1;
	}

	/*
	 * If we can't get anything out of stdout, it's typically a
	 * not-found error, so read from stderr and signal EOF on
	 * stderr.
	 */
	if (rc == 0) {
		if ((rc = libssh2_channel_read_stderr(s->channel, buffer, buf_size)) > 0) {
			giterr_set(GITERR_SSH, "%*s", rc, buffer);
			return GIT_EEOF;
		} else if (rc < LIBSSH2_ERROR_NONE) {
			ssh_error(s->session, "SSH could not read stderr");
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
	ssh_stream *s = (ssh_stream *)stream;
	size_t off = 0;
	ssize_t ret = 0;

	if (!s->sent_command && send_command(s) < 0)
		return -1;

	do {
		ret = libssh2_channel_write(s->channel, buffer + off, len - off);
		if (ret < 0)
			break;

		off += ret;

	} while (off < len);

	if (ret < 0) {
		ssh_error(s->session, "SSH could not write data");
		return -1;
	}

	return 0;
}

static void ssh_stream_free(git_smart_subtransport_stream *stream)
{
	ssh_stream *s = (ssh_stream *)stream;
	ssh_subtransport *t;

	if (!stream)
		return;

	t = OWNING_SUBTRANSPORT(s);
	t->current_stream = NULL;

	if (s->channel) {
		git_libssh2_channel_close(s->channel);
		git_libssh2_channel_free(s->channel);
		s->channel = NULL;
	}

	if (s->session) {
		git_libssh2_session_free(s->session);
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

	assert(stream);

	s = git__calloc(sizeof(ssh_stream), 1);
	GITERR_CHECK_ALLOC(s);

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
	char **host,
	char **username,
	const char *url)
{
	char *colon, *at;
	const char *start;

	colon = strchr(url, ':');


	at = strchr(url, '@');
	if (at) {
		start = at + 1;
		*username = git__substrdup(url, at - url);
		GITERR_CHECK_ALLOC(*username);
	} else {
		start = url;
		*username = NULL;
	}

	if (colon == NULL || (colon < start)) {
		giterr_set(GITERR_NET, "malformed URL");
		return -1;
	}

	*host = git__substrdup(start, colon - start);
	GITERR_CHECK_ALLOC(*host);

	return 0;
}

static int ssh_agent_auth(LIBSSH2_SESSION *session, git_cred_ssh_key *c) {
	int rc = LIBSSH2_ERROR_NONE;

	struct libssh2_agent_publickey *curr, *prev = NULL;

	LIBSSH2_AGENT *agent = git_libssh2_agent_init(session);

	if (agent == NULL)
		return -1;

	rc = git_libssh2_agent_connect(agent);

	if (rc != LIBSSH2_ERROR_NONE)
		goto shutdown;

	rc = git_libssh2_agent_list_identities(agent);

	if (rc != LIBSSH2_ERROR_NONE)
		goto shutdown;

	while (1) {
		rc = git_libssh2_agent_get_identity(agent, &curr, prev);

		if (rc < 0)
			goto shutdown;

		/* rc is set to 1 whenever the ssh agent ran out of keys to check.
		 * Set the error code to authentication failure rather than erroring
		 * out with an untranslatable error code.
		 */
		if (rc == 1) {
			rc = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
			goto shutdown;
		}

		rc = git_libssh2_agent_userauth(agent, c->username, curr);

		if (rc == 0)
			break;

		prev = curr;
	}

shutdown:

	if (rc != LIBSSH2_ERROR_NONE)
		ssh_error(session, "error authenticating");

	git_libssh2_agent_disconnect(agent);
	git_libssh2_agent_free(agent);

	return rc;
}

static int _git_ssh_authenticate_session(
	LIBSSH2_SESSION* session,
	git_cred* cred)
{
	int rc;

	do {
		giterr_clear();
		switch (cred->credtype) {
		case GIT_CREDTYPE_USERPASS_PLAINTEXT: {
			git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;
			rc = libssh2_userauth_password(session, c->username, c->password);
			break;
		}
		case GIT_CREDTYPE_SSH_KEY: {
			git_cred_ssh_key *c = (git_cred_ssh_key *)cred;

			if (c->privatekey)
				rc = libssh2_userauth_publickey_fromfile(
					session, c->username, c->publickey,
					c->privatekey, c->passphrase);
			else
				rc = ssh_agent_auth(session, c);

			break;
		}
		case GIT_CREDTYPE_SSH_CUSTOM: {
			git_cred_ssh_custom *c = (git_cred_ssh_custom *)cred;

			rc = git_libssh2_userauth_publickey(
				session, c->username, (const unsigned char *)c->publickey,
				c->publickey_len, c->sign_callback, &c->payload);
			break;
		}
		case GIT_CREDTYPE_SSH_INTERACTIVE: {
			void **abstract = git_libssh2_session_abstract(session);
			git_cred_ssh_interactive *c = (git_cred_ssh_interactive *)cred;

			/* ideally, we should be able to set this by calling
			 * libssh2_session_init_ex() instead of libssh2_session_init().
			 * libssh2's API is inconsistent here i.e. libssh2_userauth_publickey()
			 * allows you to pass the `abstract` as part of the call, whereas
			 * libssh2_userauth_keyboard_interactive() does not!
			 *
			 * The only way to set the `abstract` pointer is by calling
			 * libssh2_session_abstract(), which will replace the existing
			 * pointer as is done below. This is safe for now (at time of writing),
			 * but may not be valid in future.
			 */
			*abstract = c->payload;

			rc = libssh2_userauth_keyboard_interactive(
				session, c->username, c->prompt_callback);
			break;
		}
		case GIT_CREDTYPE_SSH_MEMORY: {
			git_cred_ssh_key *c = (git_cred_ssh_key *)cred;

			if (!git_libssh2_userauth_publickey_frommemory) {
				rc = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
				break;
			}

			assert(c->username);
			assert(c->privatekey);

			rc = git_libssh2_userauth_publickey_frommemory(
				session,
				c->username,
				strlen(c->username),
				c->publickey,
				c->publickey ? strlen(c->publickey) : 0,
				c->privatekey,
				strlen(c->privatekey),
				c->passphrase);
			break;
		}
		default:
			rc = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
		}
	} while (LIBSSH2_ERROR_EAGAIN == rc || LIBSSH2_ERROR_TIMEOUT == rc);

        if (rc == LIBSSH2_ERROR_PASSWORD_EXPIRED || rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED)
                return GIT_EAUTH;

	if (rc != LIBSSH2_ERROR_NONE) {
		if (!giterr_last())
			ssh_error(session, "Failed to authenticate SSH session");
		return -1;
	}

	return 0;
}

static int request_creds(git_cred **out, ssh_subtransport *t, const char *user, int auth_methods)
{
	int error, no_callback = 0;
	git_cred *cred = NULL;

	if (!t->owner->cred_acquire_cb) {
		no_callback = 1;
	} else {
		error = t->owner->cred_acquire_cb(&cred, t->owner->url, user, auth_methods,
						  t->owner->cred_acquire_payload);

		if (error == GIT_PASSTHROUGH)
			no_callback = 1;
		else if (error < 0)
			return error;
		else if (!cred) {
			giterr_set(GITERR_SSH, "callback failed to initialize SSH credentials");
			return -1;
		}
	}

	if (no_callback) {
		giterr_set(GITERR_SSH, "authentication required but no callback set");
		return -1;
	}

	if (!(cred->credtype & auth_methods)) {
		cred->free(cred);
		giterr_set(GITERR_SSH, "callback returned unsupported credentials type");
		return -1;
	}

	*out = cred;

	return 0;
}

static int _git_ssh_session_create(
	LIBSSH2_SESSION** session,
	git_stream *io)
{
	int rc = 0;
	LIBSSH2_SESSION* s;
	git_socket_stream *socket = (git_socket_stream *) io;

	assert(session);

	s = libssh2_session_init();
	if (!s) {
		giterr_set(GITERR_NET, "failed to initialize SSH session");
		return -1;
	}

	do {
		rc = git_libssh2_session_startup(s, socket->s);
	} while (LIBSSH2_ERROR_EAGAIN == rc || LIBSSH2_ERROR_TIMEOUT == rc);

	if (rc != LIBSSH2_ERROR_NONE) {
		ssh_error(s, "failed to start SSH session");
		git_libssh2_session_free(s);
		return -1;
	}

	git_libssh2_session_set_blocking(s, 1);

	*session = s;

	return 0;
}

static int _git_ssh_setup_conn(
	ssh_subtransport *t,
	const char *url,
	const char *cmd,
	git_smart_subtransport_stream **stream)
{
	char *host=NULL, *port=NULL, *path=NULL, *user=NULL, *pass=NULL;
	const char *default_port="22";
	int auth_methods, error = 0;
	size_t i;
	ssh_stream *s;
	git_cred *cred = NULL;
	LIBSSH2_SESSION* session=NULL;
	LIBSSH2_CHANNEL* channel=NULL;

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
			if ((error = gitno_extract_url_parts(&host, &port, &path, &user, &pass, url, default_port)) < 0)
				goto done;

			goto post_extract;
		}
	}
	if ((error = git_ssh_extract_url_parts(&host, &user, url)) < 0)
		goto done;
	port = git__strdup(default_port);
	GITERR_CHECK_ALLOC(port);

post_extract:
	if ((error = git_socket_stream_new(&s->io, host, port)) < 0 ||
	    (error = git_stream_connect(s->io)) < 0)
		goto done;

	if ((error = _git_ssh_session_create(&session, s->io)) < 0)
		goto done;

	if (t->owner->certificate_check_cb != NULL) {
		git_cert_hostkey cert = {{ 0 }}, *cert_ptr;
		const char *key;

		cert.parent.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;

		key = git_libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
		if (key != NULL) {
			cert.type |= GIT_CERT_SSH_SHA1;
			memcpy(&cert.hash_sha1, key, 20);
		}

		key = git_libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_MD5);
		if (key != NULL) {
			cert.type |= GIT_CERT_SSH_MD5;
			memcpy(&cert.hash_md5, key, 16);
		}

		if (cert.type == 0) {
			giterr_set(GITERR_SSH, "unable to get the host key");
			error = -1;
			goto done;
		}

		/* We don't currently trust any hostkeys */
		giterr_clear();

		cert_ptr = &cert;

		error = t->owner->certificate_check_cb((git_cert *) cert_ptr, 0, host, t->owner->message_cb_payload);
		if (error < 0) {
			if (!giterr_last())
				giterr_set(GITERR_NET, "user cancelled hostkey check");

			goto done;
		}
	}

	/* we need the username to ask for auth methods */
	if (!user) {
		if ((error = request_creds(&cred, t, NULL, GIT_CREDTYPE_USERNAME)) < 0)
			goto done;

		user = git__strdup(((git_cred_username *) cred)->username);
		cred->free(cred);
		cred = NULL;
		if (!user)
			goto done;
	} else if (user && pass) {
		if ((error = git_cred_userpass_plaintext_new(&cred, user, pass)) < 0)
			goto done;
	}

	if ((error = list_auth_methods(&auth_methods, session, user)) < 0)
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

		if ((error = request_creds(&cred, t, user, auth_methods)) < 0)
			goto done;

		if (strcmp(user, git_cred__username(cred))) {
			giterr_set(GITERR_SSH, "username does not match previous request");
			error = -1;
			goto done;
		}

		error = _git_ssh_authenticate_session(session, cred);
	}

	if (error < 0)
		goto done;

	channel = libssh2_channel_open_session(session);
	if (!channel) {
		error = -1;
		ssh_error(session, "Failed to open SSH channel");
		goto done;
	}

	git_libssh2_channel_set_blocking(channel, 1);

	s->session = session;
	s->channel = channel;

	t->current_stream = s;

done:
	if (error < 0) {
		ssh_stream_free(*stream);

		if (session)
			git_libssh2_session_free(session);
	}

	if (cred)
		cred->free(cred);

	git__free(host);
	git__free(port);
	git__free(path);
	git__free(user);
	git__free(pass);

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

	giterr_set(GITERR_NET, "must call UPLOADPACK_LS before UPLOADPACK");
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

	giterr_set(GITERR_NET, "must call RECEIVEPACK_LS before RECEIVEPACK");
	return -1;
}

static int _ssh_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *subtransport,
	const char *url,
	git_smart_service_t action)
{
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

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
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

	assert(!t->current_stream);

	GIT_UNUSED(t);

	return 0;
}

static void _ssh_free(git_smart_subtransport *subtransport)
{
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

	assert(!t->current_stream);

	git__free(t->cmd_uploadpack);
	git__free(t->cmd_receivepack);
	git__free(t);
}

#define SSH_AUTH_PUBLICKEY "publickey"
#define SSH_AUTH_PASSWORD "password"
#define SSH_AUTH_KEYBOARD_INTERACTIVE "keyboard-interactive"

static int list_auth_methods(int *out, LIBSSH2_SESSION *session, const char *username)
{
	const char *list, *ptr;

	*out = 0;

	list = git_libssh2_userauth_list(session, username, strlen(username));

	/* either error, or the remote accepts NONE auth, which is bizarre, let's punt */
	if (list == NULL && !git_libssh2_userauth_authenticated(session)) {
		ssh_error(session, "Failed to retrieve list of SSH authentication methods");
		return -1;
	}

	ptr = list;
	while (ptr) {
		if (*ptr == ',')
			ptr++;

		if (!git__prefixcmp(ptr, SSH_AUTH_PUBLICKEY)) {
			*out |= GIT_CREDTYPE_SSH_KEY;
			*out |= GIT_CREDTYPE_SSH_CUSTOM;

			if (git_libssh2_userauth_publickey_frommemory)
				*out |= GIT_CREDTYPE_SSH_MEMORY;

			ptr += strlen(SSH_AUTH_PUBLICKEY);
			continue;
		}

		if (!git__prefixcmp(ptr, SSH_AUTH_PASSWORD)) {
			*out |= GIT_CREDTYPE_USERPASS_PLAINTEXT;
			ptr += strlen(SSH_AUTH_PASSWORD);
			continue;
		}

		if (!git__prefixcmp(ptr, SSH_AUTH_KEYBOARD_INTERACTIVE)) {
			*out |= GIT_CREDTYPE_SSH_INTERACTIVE;
			ptr += strlen(SSH_AUTH_KEYBOARD_INTERACTIVE);
			continue;
		}

		/* Skipt it if we don't know it */
		ptr = strchr(ptr, ',');
	}

	return 0;
}
#endif

int git_smart_subtransport_ssh(
	git_smart_subtransport **out, git_transport *owner, void *param)
{
#ifdef GIT_SSH
	ssh_subtransport *t;

	assert(out);

	GIT_UNUSED(param);

	if (git_libssh2_handle == NULL) {
		*out = NULL;

		giterr_set(GITERR_INVALID, "Cannot create SSH transport. SSH support unavailable");
		return -1;
	}

	t = git__calloc(sizeof(ssh_subtransport), 1);
	GITERR_CHECK_ALLOC(t);

	t->owner = (transport_smart *)owner;
	t->parent.action = _ssh_action;
	t->parent.close = _ssh_close;
	t->parent.free = _ssh_free;

	*out = (git_smart_subtransport *) t;
	return 0;
#else
	GIT_UNUSED(owner);
	GIT_UNUSED(param);

	assert(out);
	*out = NULL;

	giterr_set(GITERR_INVALID, "cannot create SSH transport. Library was built without SSH support");
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

	assert(out);

	if (git_libssh2_handle == NULL) {
		*out = NULL;

		giterr_set(GITERR_INVALID, "Cannot create SSH transport. SSH support unavailable");
		return -1;
	}

	if (paths->count != 2) {
		giterr_set(GITERR_SSH, "invalid ssh paths, must be two strings");
		return GIT_EINVALIDSPEC;
	}

	if ((error = git_transport_smart(&transport, owner, &ssh_definition)) < 0)
		return error;

	smart = (transport_smart *) transport;
	t = (ssh_subtransport *) smart->wrapped;

	t->cmd_uploadpack = git__strdup(paths->strings[0]);
	GITERR_CHECK_ALLOC(t->cmd_uploadpack);
	t->cmd_receivepack = git__strdup(paths->strings[1]);
	GITERR_CHECK_ALLOC(t->cmd_receivepack);

	*out = transport;
	return 0;
#else
	GIT_UNUSED(owner);
	GIT_UNUSED(payload);

	assert(out);
	*out = NULL;

	giterr_set(GITERR_INVALID, "cannot create SSH transport. Library was built without SSH support");
	return -1;
#endif
}

static const char *supported_filenames[] = {
#ifdef __APPLE__
	"libssh2.1.dylib",
#else
	"libssh2.so.2.1.0",
#endif
	NULL,
};

/**
 * Try to load a supported version of libssh2
 */
static void load_libssh2(void)
{
#ifdef GIT_WIN32
	/* Windows doesn't do SOVERSIONs but you ship your own library so we're probably OK */
	git_libssh2_handle = LoadLibrary("libssh2.dll");
#else
	int i;
	for (i = 0; !git_libssh2_handle; i++){
		const char *filename = supported_filenames[i];
		if (!filename)
			break;

		git_libssh2_handle = dlopen(filename, RTLD_LAZY);
	}

#endif
}

static void unload_libssh2(void)
{
#ifdef GIT_WIN32
	FreeLibrary(git_libssh2_handle);
#else
	dlclose(git_libssh2_handle);
#endif
}

void git_transport_ssh_global_shutdown(void);

int git_transport_ssh_global_init(void)
{
#ifdef GIT_SSH
	load_libssh2();

	if (git_libssh2_handle == NULL) {
		return 0;
	}


	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_connect);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_disconnect);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_free);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_get_identity);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_init);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_list_identities);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_agent_userauth);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_close);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_free);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_open_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_process_startup);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_read_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_set_blocking);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_channel_write_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_hostkey_hash);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_init);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_abstract);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_free);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_init_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_last_error);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_set_blocking);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_session_startup);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_authenticated);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_keyboard_interactive_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_list);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_password_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_publickey);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_publickey_fromfile_ex);
	LOOKUP_LIBSSH2_SYMBOL(libssh2_userauth_publickey_frommemory);

	git__on_shutdown(git_transport_ssh_global_shutdown);

	git_libssh2_init(0);
	return 0;

#else

	/* Nothing to initialize */
	return 0;
#endif
}

void git_transport_ssh_global_shutdown(void)
{
	unload_libssh2();
	git_libssh2_handle = NULL;
}
