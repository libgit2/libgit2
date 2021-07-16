/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "../transports/ssh.h"
#include "git2/sys/credential.h"

#if defined(GIT_SSH) && defined(GIT_LIBSSH2)

void git__ssh_error(git_ssh_session *s, const char *errmsg)
{
	char *ssherr;
	libssh2_session_last_error(s->session, &ssherr, NULL, 0);

	git_error_set(GIT_ERROR_SSH, "%s: %s", errmsg, ssherr);
}

void git_ssh__session_free(git_ssh_session *s)
{
	if (!s)
		return;

	libssh2_session_free(s->session);
	git__free(s);
}

int git_ssh__session_create(
	git_ssh_session **out_session,
	git_stream *io)
{
	int rc = 0;
	git_ssh_session *s;
	git_socket_stream *socket = GIT_CONTAINER_OF(io, git_socket_stream, parent);

	assert(out_session);

	s = git__calloc(1, sizeof(*s));
	if (!s) {
		git_error_set_oom();
		return -1;
	}

	s->session = git_ssh_session_new();
	if (!s->session) {
		git_error_set(GIT_ERROR_NET, "failed to initialize SSH session");
		goto cleanup;
	}

	do {
		rc = libssh2_session_handshake(s->session, socket->s);
	} while (LIBSSH2_ERROR_EAGAIN == rc || LIBSSH2_ERROR_TIMEOUT == rc);

	if (rc != GIT_SSH_ERROR_NONE) {
		git__ssh_error(s, "failed to start SSH session");
		goto cleanup;
	}

	libssh2_session_set_blocking(s->session, 1);

	*out_session = s;

	return 0;

cleanup:
	git_ssh__session_free(s);

	return -1;
}

git_ssh_channel *git_ssh_channel_open(git_ssh_session *s)
{
	git_ssh_channel *channel;

	channel = git__calloc(1, sizeof(*channel));
	if (!channel) {
		git_error_set_oom();
		return NULL;
	}

	channel->channel = libssh2_channel_open_session(s->session);

	if (channel->channel)
		libssh2_channel_set_blocking(channel->channel, 1);

	return channel;
}

void git_ssh_channel_free(git_ssh_channel *c)
{
	libssh2_channel_free(c->channel);
	git__free(c);
}

int git_ssh_channel_read(char *buffer, size_t size, int is_stderr, git_ssh_channel *c)
{
	if (is_stderr)
		return libssh2_channel_read_stderr(c->channel, buffer, size);
	return libssh2_channel_read(c->channel, buffer, size);
}

int git_ssh_channel_write(git_ssh_channel *c, const char *buffer, size_t size)
{
	return libssh2_channel_write(c->channel, buffer, size);
}

int git_ssh_channel_exec(git_ssh_channel *c, const char *request)
{
	return libssh2_channel_exec(c->channel, request);
}

int git_ssh_session_server_hostkey(git_ssh_session *s, git_cert_hostkey *cert)
{
	const char *key;
	size_t cert_len;
	int cert_type;

	cert->parent.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;

	key = libssh2_session_hostkey(s->session, &cert_len, &cert_type);
	if (key != NULL) {
		cert->type |= GIT_CERT_SSH_RAW;
		cert->hostkey = key;
		cert->hostkey_len = cert_len;
		switch (cert_type) {
			case LIBSSH2_HOSTKEY_TYPE_RSA:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_RSA;
				break;
			case LIBSSH2_HOSTKEY_TYPE_DSS:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_DSS;
				break;

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
			case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_256;
				break;
			case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_384;
				break;
			case LIBSSH2_KNOWNHOST_KEY_ECDSA_521:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_521;
				break;
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
			case LIBSSH2_HOSTKEY_TYPE_ED25519:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ED25519;
				break;
#endif
			default:
				cert->raw_type = GIT_CERT_SSH_RAW_TYPE_UNKNOWN;
		}
	}

#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
	key = libssh2_hostkey_hash(s->session, LIBSSH2_HOSTKEY_HASH_SHA256);
	if (key != NULL) {
		cert->type |= GIT_CERT_SSH_SHA256;
		memcpy(&cert->hash_sha256, key, 32);
	}
#endif


	key = libssh2_hostkey_hash(s->session, LIBSSH2_HOSTKEY_HASH_SHA1);
	if (key != NULL) {
		cert->type |= GIT_CERT_SSH_SHA1;
		memcpy(&cert->hash_sha1, key, 20);
	}

	key = libssh2_hostkey_hash(s->session, LIBSSH2_HOSTKEY_HASH_MD5);
	if (key != NULL) {
		cert->type |= GIT_CERT_SSH_MD5;
		memcpy(&cert->hash_md5, key, 16);
	}

	if (cert->type == 0) {
		git_error_set(GIT_ERROR_SSH, "unable to get the host key");
		return -1;
	}

	return 0;
}

int git_ssh_session_server_is_known(git_ssh_session *s, int *valid)
{
	GIT_UNUSED(s);
	*valid = 0;
	return 0;
}

#define SSH_AUTH_PUBLICKEY "publickey"
#define SSH_AUTH_PASSWORD "password"
#define SSH_AUTH_KEYBOARD_INTERACTIVE "keyboard-interactive"

int git__ssh_list_auth_methods(int *out, git_ssh_session *s, const char *username)
{
	const char *list, *ptr;

	*out = 0;

	list = libssh2_userauth_list(s->session, username, strlen(username));

	/* either error, or the remote accepts NONE auth, which is bizarre, let's punt */
	if (list == NULL && !libssh2_userauth_authenticated(s->session)) {
		git__ssh_error(s, "Failed to retrieve list of SSH authentication methods");
		return -1;
	}

	ptr = list;
	while (ptr) {
		if (*ptr == ',')
			ptr++;

		if (!git__prefixcmp(ptr, SSH_AUTH_PUBLICKEY)) {
			*out |= GIT_CREDENTIAL_SSH_KEY;
			*out |= GIT_CREDENTIAL_SSH_CUSTOM;
#ifdef GIT_SSH_MEMORY_CREDENTIALS
			*out |= GIT_CREDENTIAL_SSH_MEMORY;
#endif
			ptr += strlen(SSH_AUTH_PUBLICKEY);
			continue;
		}

		if (!git__prefixcmp(ptr, SSH_AUTH_PASSWORD)) {
			*out |= GIT_CREDENTIAL_USERPASS_PLAINTEXT;
			ptr += strlen(SSH_AUTH_PASSWORD);
			continue;
		}

		if (!git__prefixcmp(ptr, SSH_AUTH_KEYBOARD_INTERACTIVE)) {
			*out |= GIT_CREDENTIAL_SSH_INTERACTIVE;
			ptr += strlen(SSH_AUTH_KEYBOARD_INTERACTIVE);
			continue;
		}

		/* Skipt it if we don't know it */
		ptr = strchr(ptr, ',');
	}

	return 0;
}

int git__ssh_agent_auth(git_ssh_session *s, git_credential_ssh_key *c)
{
	int rc = GIT_SSH_ERROR_NONE;

	struct libssh2_agent_publickey *curr, *prev = NULL;

	LIBSSH2_AGENT *agent = libssh2_agent_init(s->session);

	if (agent == NULL)
		return -1;

	rc = libssh2_agent_connect(agent);

	if (rc != LIBSSH2_ERROR_NONE)
		goto shutdown;

	rc = libssh2_agent_list_identities(agent);

	if (rc != LIBSSH2_ERROR_NONE)
		goto shutdown;

	while (1) {
		rc = libssh2_agent_get_identity(agent, &curr, prev);

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

		rc = libssh2_agent_userauth(agent, c->username, curr);

		if (rc == 0)
			break;

		prev = curr;
	}

shutdown:

	if (rc != LIBSSH2_ERROR_NONE)
		git__ssh_error(s, "error authenticating");

	libssh2_agent_disconnect(agent);
	libssh2_agent_free(agent);

	return rc;
}

struct git_ssh__sign_payload
{
	git_credential_sign_cb sign_cb;
	void *user_payload;
};

static inline LIBSSH2_USERAUTH_PUBLICKEY_SIGN_FUNC(git_ssh__sign_cb)
{
	struct git_ssh__sign_payload *payload = *abstract;

	return payload->sign_cb((git_ssh_session *)session,
							sig, sig_len,
							data, data_len, payload->user_payload);
}

struct git_ssh__kbdint_payload
{
	LIBSSH2_SESSION *session;
	git_credential_ssh_interactive_cb kbdint_cb;
	void *user_payload;
};

static LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC(git_ssh__kbdint_cb)
{
	struct git_ssh__kbdint_payload *payload = *abstract;

	return payload->kbdint_cb((git_ssh_session *)payload->session,
							  name, name_len,
							  instruction, instruction_len,
							  num_prompts,
							  (git_credential_ssh_interactive_prompt *)prompts,
							  (git_credential_ssh_interactive_response *)responses,
							  payload->user_payload);
}

int _git_ssh_authenticate_session(git_ssh_session *s, git_credential *cred)
{
	int rc;

	do {
		git_error_clear();
		switch (cred->credtype) {
			case GIT_CREDENTIAL_USERPASS_PLAINTEXT: {
				git_credential_userpass_plaintext *c = (git_credential_userpass_plaintext *)cred;
				rc = libssh2_userauth_password(s->session, c->username, c->password);
				break;
			}
			case GIT_CREDENTIAL_SSH_KEY: {
				git_credential_ssh_key *c = (git_credential_ssh_key *)cred;

				if (c->privatekey) {
					rc = libssh2_userauth_publickey_fromfile(
						s->session, c->username, c->publickey,
						c->privatekey, c->passphrase);
				} else
					rc = git__ssh_agent_auth(s, c);

				break;
			}
			case GIT_CREDENTIAL_SSH_CUSTOM: {
				git_credential_ssh_custom *c = (git_credential_ssh_custom *)cred;
				struct git_ssh__sign_payload payload = { c->sign_callback, &c->payload };

				rc = libssh2_userauth_publickey(
					s->session, c->username, (const unsigned char *)c->publickey,
					c->publickey_len, git_ssh__sign_cb, (void **)&payload);
				break;
			}
			case GIT_CREDENTIAL_SSH_INTERACTIVE: {
				git_credential_ssh_interactive *c = (git_credential_ssh_interactive *)cred;
				void **abstract = libssh2_session_abstract(s->session);
				void *old_abstract = *abstract;
				struct git_ssh__kbdint_payload payload;
				payload.kbdint_cb = c->prompt_callback;
				payload.session = s->session;
				payload.user_payload = c->payload;

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
				*abstract = &payload;

				rc = libssh2_userauth_keyboard_interactive(
					s->session, c->username, git_ssh__kbdint_cb);

				*abstract = old_abstract;

				break;
			}
#ifdef GIT_SSH_MEMORY_CREDENTIALS
			case GIT_CREDENTIAL_SSH_MEMORY: {
				git_credential_ssh_key *c = (git_credential_ssh_key *)cred;

				assert(c->username);
				assert(c->privatekey);

				rc = libssh2_userauth_publickey_frommemory(
					s->session,
					c->username,
					strlen(c->username),
					c->publickey,
					c->publickey ? strlen(c->publickey) : 0,
					c->privatekey,
					strlen(c->privatekey),
					c->passphrase);
				break;
			}
#endif
			default:
				rc = LIBSSH2_ERROR_AUTHENTICATION_FAILED;
		}
	}
	while (LIBSSH2_ERROR_EAGAIN == rc || LIBSSH2_ERROR_TIMEOUT == rc);

	if (rc == LIBSSH2_ERROR_PASSWORD_EXPIRED ||
		rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED ||
		rc == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED)
		return GIT_EAUTH;

	if (rc != GIT_SSH_ERROR_NONE) {
		if (!git_error_last())
			git__ssh_error(s, "Failed to authenticate SSH session");
		return -1;
	}

	return 0;
}

#endif
