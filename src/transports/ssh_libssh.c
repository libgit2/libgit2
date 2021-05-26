/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "../transports/ssh.h"
#include "git2/sys/credential.h"

#if defined(GIT_SSH) && defined(GIT_LIBSSH)

void git__ssh_error(git_ssh_session *s, const char *errmsg)
{
	const char *ssherr = ssh_get_error(s->session);

	git_error_set(GIT_ERROR_SSH, "%s: %s", errmsg, ssherr);
}

void git_ssh__session_free(git_ssh_session *s)
{
	if (!s)
		return;

	ssh_free(s->session);
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

	if (ssh_options_set(s->session, SSH_OPTIONS_HOST, &socket->host) != SSH_OK ||
		ssh_options_set(s->session, SSH_OPTIONS_FD, &socket->s) != SSH_OK) {
		git__ssh_error(s, "failed to set SSH options");
		goto cleanup;
	}

	rc = ssh_connect(s->session);
	if (rc != SSH_OK) {
		git__ssh_error(s, "failed to start SSH session");
		goto cleanup;
	}

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

	channel->channel = ssh_channel_new(s->session);
	if (channel->channel == NULL) {
		git_error_set_oom();
		return NULL;
	}

	if(ssh_channel_open_session(channel->channel) != SSH_OK) {
		ssh_channel_free(channel->channel);
		return NULL;
	}

	return channel;
}

void git_ssh_channel_free(git_ssh_channel *c)
{
	ssh_channel_free(c->channel);
	git__free(c);
}

int git_ssh_channel_read(char *buffer, size_t size, int is_stderr, git_ssh_channel *c)
{
	return ssh_channel_read(c->channel, buffer, size, is_stderr);
}

int git_ssh_channel_write(git_ssh_channel *c, const char *buffer, size_t size)
{
	return ssh_channel_write(c->channel, buffer, size);
}

int git_ssh_channel_exec(git_ssh_channel *c, const char *request)
{
	return ssh_channel_request_exec(c->channel, request);
}

int git_ssh_session_server_hostkey(git_ssh_session *s, git_cert_hostkey *cert)
{
	ssh_key pkey;
	unsigned char hash[32];
	unsigned char *hash_ptr = (unsigned char *)&hash;
	char *b64_key;
	size_t hash_len;

	if (ssh_get_server_publickey(s->session, &pkey) == SSH_OK) {

		if (ssh_pki_export_pubkey_base64(pkey, &b64_key) == SSH_OK) {
			cert->type |= GIT_CERT_SSH_RAW;
			memcpy(&cert->hostkey, b64_key, strlen(b64_key));
			switch (ssh_key_type(pkey)) {
				case SSH_KEYTYPE_RSA:
				case SSH_KEYTYPE_RSA1:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_RSA;
					break;
				case SSH_KEYTYPE_DSS:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_DSS;
					break;

				case SSH_KEYTYPE_ECDSA_P256:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_256;
					break;
				case SSH_KEYTYPE_ECDSA_P384:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_384;
					break;
				case SSH_KEYTYPE_ECDSA_P521:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_521;
					break;

				case SSH_KEYTYPE_ED25519:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ED25519;
					break;

				case SSH_KEYTYPE_UNKNOWN:
				default:
					cert->raw_type = GIT_CERT_SSH_RAW_TYPE_UNKNOWN;
					break;
			}
		}

		if (ssh_get_publickey_hash(pkey, SSH_PUBLICKEY_HASH_MD5, &hash_ptr, &hash_len) == SSH_OK) {
			cert->type |= GIT_CERT_SSH_MD5;
			memcpy(&cert->hash_md5, hash_ptr, hash_len);
			ssh_clean_pubkey_hash(&hash_ptr);
		}

		if (ssh_get_publickey_hash(pkey, SSH_PUBLICKEY_HASH_SHA1, &hash_ptr, &hash_len) == SSH_OK) {
			cert->type |= GIT_CERT_SSH_SHA1;
			memcpy(&cert->hash_sha1, hash_ptr, hash_len);
			ssh_clean_pubkey_hash(&hash_ptr);
		}

		if (ssh_get_publickey_hash(pkey, SSH_PUBLICKEY_HASH_SHA256, &hash_ptr, &hash_len) == SSH_OK) {
			cert->type |= GIT_CERT_SSH_SHA256;
			memcpy(&cert->hash_sha256, hash_ptr, hash_len);
			ssh_clean_pubkey_hash(&hash_ptr);
		}

		ssh_key_free(pkey);
	}

	if (cert->type == 0) {
		git_error_set(GIT_ERROR_SSH, "unable to get the host key");
		return -1;
	}

	cert->parent.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;

	return 0;
}

#define SSH_AUTH_PUBLICKEY "publickey"
#define SSH_AUTH_PASSWORD "password"
#define SSH_AUTH_KEYBOARD_INTERACTIVE "keyboard-interactive"

int git__ssh_list_auth_methods(int *out, git_ssh_session *s, const char *username)
{
	int method;
	int rc = ssh_userauth_none(s->session, NULL);
	if (rc == SSH_AUTH_SUCCESS || rc == SSH_AUTH_ERROR) {
		return rc;
	}

	method = ssh_userauth_list(s->session, username);

	*out = 0;

	if (method & SSH_AUTH_METHOD_PUBLICKEY) {
		*out |= GIT_CREDENTIAL_SSH_KEY;
		/* *out |= GIT_CREDTYPE_SSH_CUSTOM; */
#ifdef GIT_SSH_MEMORY_CREDENTIALS
		*out |= GIT_CREDTYPE_SSH_MEMORY;
#endif
	}

	if (method & SSH_AUTH_METHOD_PASSWORD) {
		*out |= GIT_CREDENTIAL_USERPASS_PLAINTEXT;
	}

	if (method & SSH_AUTH_METHOD_INTERACTIVE) {
		*out |= GIT_CREDENTIAL_SSH_INTERACTIVE;
	}

	return 0;
}

int git__ssh_agent_auth(git_ssh_session *s, git_credential_ssh_key *c)
{
	int rc = GIT_SSH_ERROR_NONE;

	rc = ssh_userauth_agent(s->session, c->username);
	if (rc != SSH_OK) {

	}

	rc = ssh_userauth_publickey(s->session, c->username, NULL);
	if (rc != SSH_OK) {

	}

	return rc;
}

int _git_ssh_authenticate_session(git_ssh_session *s, git_credential *cred)
{
	int rc;

	do {
		git_error_clear();
		switch (cred->credtype) {
			case GIT_CREDENTIAL_USERPASS_PLAINTEXT: {
				git_credential_userpass_plaintext *c = (git_credential_userpass_plaintext *)cred;
				rc = ssh_userauth_password(s->session, c->username, c->password);
				break;
			}
			case GIT_CREDENTIAL_SSH_KEY: {
				git_credential_ssh_key *c = (git_credential_ssh_key *)cred;

				if (c->privatekey) {
					ssh_key privkey;
					char *passphrase = c->passphrase ? c->passphrase : "";

					rc = ssh_pki_import_privkey_file(c->privatekey, passphrase, NULL, NULL, &privkey);
					if (rc != SSH_OK)
						break;

					rc = ssh_userauth_publickey(s->session, c->username, privkey);

					ssh_key_free(privkey);
				} else
					rc = git__ssh_agent_auth(s, c);

				break;
			}
			case GIT_CREDENTIAL_SSH_CUSTOM: {
				git_credential_ssh_custom *c = (git_credential_ssh_custom *)cred;

				/* AFAICS there's no sign callback stuff */
				GIT_UNUSED(c);

				rc = -1;
				break;
			}
			case GIT_CREDENTIAL_SSH_INTERACTIVE: {
				git_credential_ssh_interactive *c = (git_credential_ssh_interactive *)cred;

				rc = ssh_userauth_kbdint(s->session, c->username, NULL);
				if (rc == SSH_AUTH_INFO) {
					const char *name = ssh_userauth_kbdint_getname(s->session);
					const char *instr = ssh_userauth_kbdint_getinstruction(s->session);
					int num_prompts = ssh_userauth_kbdint_getnprompts(s->session);
					git_credential_ssh_interactive_prompt *prompts;
					git_credential_ssh_interactive_response *responses;
					int i = 0;

					prompts = git__calloc(num_prompts, sizeof(*prompts));
					responses = git__calloc(num_prompts, sizeof(*responses));

					for (i = 0; i < num_prompts; i++) {
						prompts[i].text = (char *)ssh_userauth_kbdint_getprompt(s->session, i, (char *)&prompts[i].echo);
						prompts[i].length = strlen(prompts[i].text);
					}

					c->prompt_callback(s, name, strlen(name),
									   instr, strlen(instr),
									   num_prompts, prompts, responses, NULL);

					for (i = 0; i < num_prompts; i++) {
						ssh_userauth_kbdint_setanswer(s->session, i, responses[i].text);
					}

					git__free(prompts);
					git__free(responses);
				}
				break;
			}
#ifdef GIT_SSH_MEMORY_CREDENTIALS
			case GIT_CREDTYPE_SSH_MEMORY: {
				git_cred_ssh_key *c = (git_cred_ssh_key *)cred;

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
				rc = SSH_AUTH_ERROR;
		}
	}
	while (rc == SSH_AUTH_AGAIN);

	if (rc == SSH_AUTH_DENIED)
		return GIT_EAUTH;

	if (rc != GIT_SSH_ERROR_NONE) {
		if (!git_error_last())
			git__ssh_error(s, "Failed to authenticate SSH session");
		return -1;
	}

	return 0;
}

#endif
