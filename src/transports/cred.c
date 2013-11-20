/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "smart.h"
#include "git2/cred_helpers.h"

int git_cred_has_username(git_cred *cred)
{
	int ret = 0;

	switch (cred->credtype) {
	case GIT_CREDTYPE_USERPASS_PLAINTEXT: {
		git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;
		ret = !!c->username;
		break;
	}
	case GIT_CREDTYPE_SSH_KEY: {
		git_cred_ssh_key *c = (git_cred_ssh_key *)cred;
		ret = !!c->username;
		break;
	}
	case GIT_CREDTYPE_SSH_CUSTOM: {
		git_cred_ssh_custom *c = (git_cred_ssh_custom *)cred;
		ret = !!c->username;
		break;
	}
	case GIT_CREDTYPE_DEFAULT: {
		ret = 0;
		break;
	}
	}

	return ret;
}

static void plaintext_free(struct git_cred *cred)
{
	git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;

	git__free(c->username);

	/* Zero the memory which previously held the password */
	if (c->password) {
		size_t pass_len = strlen(c->password);
		git__memzero(c->password, pass_len);
		git__free(c->password);
	}

	git__memzero(c, sizeof(*c));
	git__free(c);
}

int git_cred_userpass_plaintext_new(
	git_cred **cred,
	const char *username,
	const char *password)
{
	git_cred_userpass_plaintext *c;

	assert(cred && username && password);

	c = git__malloc(sizeof(git_cred_userpass_plaintext));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_USERPASS_PLAINTEXT;
	c->parent.free = plaintext_free;
	c->username = git__strdup(username);

	if (!c->username) {
		git__free(c);
		return -1;
	}

	c->password = git__strdup(password);

	if (!c->password) {
		git__free(c->username);
		git__free(c);
		return -1;
	}

	*cred = &c->parent;
	return 0;
}

static void ssh_key_free(struct git_cred *cred)
{
	git_cred_ssh_key *c =
		(git_cred_ssh_key *)cred;

	git__free(c->username);
	git__free(c->publickey);
	git__free(c->privatekey);

	if (c->passphrase) {
		/* Zero the memory which previously held the passphrase */
		size_t pass_len = strlen(c->passphrase);
		git__memzero(c->passphrase, pass_len);
		git__free(c->passphrase);
	}

	git__memzero(c, sizeof(*c));
	git__free(c);
}

static void ssh_custom_free(struct git_cred *cred)
{
	git_cred_ssh_custom *c = (git_cred_ssh_custom *)cred;

	git__free(c->username);
	git__free(c->publickey);

	git__memzero(c, sizeof(*c));
	git__free(c);
}

static void default_free(struct git_cred *cred)
{
	git_cred_default *c = (git_cred_default *)cred;

	git__free(c);
}

int git_cred_ssh_key_new(
	git_cred **cred,
	const char *username,
	const char *publickey,
	const char *privatekey,
	const char *passphrase)
{
	git_cred_ssh_key *c;

	assert(cred && privatekey);

	c = git__calloc(1, sizeof(git_cred_ssh_key));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_KEY;
	c->parent.free = ssh_key_free;

	if (username) {
		c->username = git__strdup(username);
		GITERR_CHECK_ALLOC(c->username);
	}

	c->privatekey = git__strdup(privatekey);
	GITERR_CHECK_ALLOC(c->privatekey);

	if (publickey) {
		c->publickey = git__strdup(publickey);
		GITERR_CHECK_ALLOC(c->publickey);
	}

	if (passphrase) {
		c->passphrase = git__strdup(passphrase);
		GITERR_CHECK_ALLOC(c->passphrase);
	}

	*cred = &c->parent;
	return 0;
}

int git_cred_ssh_key_from_agent(git_cred **cred, const char *username) {
	git_cred_ssh_key *c;

	assert(cred);

	c = git__calloc(1, sizeof(git_cred_ssh_key));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_KEY;
	c->parent.free = ssh_key_free;

	if (username) {
		c->username = git__strdup(username);
		GITERR_CHECK_ALLOC(c->username);
	}

	c->privatekey = NULL;

	*cred = &c->parent;
	return 0;
}

int git_cred_ssh_custom_new(
	git_cred **cred,
	const char *username,
	const char *publickey,
	size_t publickey_len,
	git_cred_sign_callback sign_callback,
	void *sign_data)
{
	git_cred_ssh_custom *c;

	assert(cred);

	c = git__calloc(1, sizeof(git_cred_ssh_custom));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_CUSTOM;
	c->parent.free = ssh_custom_free;

	if (username) {
		c->username = git__strdup(username);
		GITERR_CHECK_ALLOC(c->username);
	}

	if (publickey_len > 0) {
		c->publickey = git__malloc(publickey_len);
		GITERR_CHECK_ALLOC(c->publickey);

		memcpy(c->publickey, publickey, publickey_len);
	}

	c->publickey_len = publickey_len;
	c->sign_callback = sign_callback;
	c->sign_data = sign_data;

	*cred = &c->parent;
	return 0;
}

int git_cred_default_new(git_cred **cred)
{
	git_cred_default *c;

	assert(cred);

	c = git__calloc(1, sizeof(git_cred_default));
	GITERR_CHECK_ALLOC(c);

	c->credtype = GIT_CREDTYPE_DEFAULT;
	c->free = default_free;

	*cred = c;
	return 0;
}
