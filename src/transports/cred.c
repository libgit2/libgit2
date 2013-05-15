/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "smart.h"
#include "git2/cred_helpers.h"

static void plaintext_free(struct git_cred *cred)
{
	git_cred_userpass_plaintext *c = (git_cred_userpass_plaintext *)cred;
	size_t pass_len = strlen(c->password);

	git__free(c->username);

	/* Zero the memory which previously held the password */
	memset(c->password, 0x0, pass_len);
	git__free(c->password);

	memset(c, 0, sizeof(*c));

	git__free(c);
}

int git_cred_userpass_plaintext_new(
	git_cred **cred,
	const char *username,
	const char *password)
{
	git_cred_userpass_plaintext *c;

	if (!cred)
		return -1;

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

#ifdef GIT_SSH
static void ssh_keyfile_passphrase_free(struct git_cred *cred)
{
	git_cred_ssh_keyfile_passphrase *c = (git_cred_ssh_keyfile_passphrase *)cred;
	size_t pass_len = strlen(c->passphrase);

    if (c->publickey) {
        git__free(c->publickey);
    }
    
	git__free(c->privatekey);

    if (c->passphrase) {
        /* Zero the memory which previously held the passphrase */
        memset(c->passphrase, 0x0, pass_len);
        git__free(c->passphrase);
    }

	memset(c, 0, sizeof(*c));

	git__free(c);
}

int git_cred_ssh_keyfile_passphrase_new(
	git_cred **cred,
	const char *publickey,
	const char *privatekey,
	const char *passphrase)
{
	git_cred_ssh_keyfile_passphrase *c;

	assert(cred && privatekey);

	c = git__calloc(1, sizeof(git_cred_ssh_keyfile_passphrase));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_KEYFILE_PASSPHRASE;
	c->parent.free = ssh_keyfile_passphrase_free;
    
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

static void ssh_publickey_free(struct git_cred *cred)
{
	git_cred_ssh_publickey *c = (git_cred_ssh_publickey *)cred;

    git__free(c->publickey);

    c->sign_callback = NULL;
    c->sign_data = NULL;
    
	memset(c, 0, sizeof(*c));

	git__free(c);
}

int git_cred_ssh_publickey_new(
	git_cred **cred,
	const char *publickey,
    size_t publickey_len,
	LIBSSH2_USERAUTH_PUBLICKEY_SIGN_FUNC((*sign_callback)),
    void *sign_data)
{
	git_cred_ssh_publickey *c;

	if (!cred)
		return -1;

	c = git__malloc(sizeof(git_cred_ssh_publickey));
	GITERR_CHECK_ALLOC(c);

	c->parent.credtype = GIT_CREDTYPE_SSH_PUBLICKEY;
	c->parent.free = ssh_publickey_free;
    
    c->publickey = git__malloc(publickey_len);
	GITERR_CHECK_ALLOC(c->publickey);
	
    memcpy(c->publickey, publickey, publickey_len);
    
    c->publickey_len = publickey_len;
    c->sign_callback = sign_callback;
    c->sign_data = sign_data;

	*cred = &c->parent;
	return 0;
}
#endif
