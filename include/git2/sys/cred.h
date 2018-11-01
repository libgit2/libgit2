/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_cred_h__
#define INCLUDE_sys_git_cred_h__

#include "common.h"

#include "git2/transport.h"

struct git_cred {
	git_credtype_t credtype; /**< A type of credential */
	void (*free)(git_cred *cred);
};

/** A plaintext username and password */
typedef struct {
	git_cred parent;
	char *username;
	char *password;
} git_cred_userpass_plaintext;

/** Username-only credential information */
typedef struct git_cred_username {
	git_cred parent;
	char username[1];
} git_cred_username;

/** A key for NTLM/Kerberos "default" credentials */
typedef struct git_cred git_cred_default;

/**
 * A ssh key from disk
 */
typedef struct git_cred_ssh_key {
	git_cred parent;
	char *username;
	char *publickey;
	char *privatekey;
	char *passphrase;
} git_cred_ssh_key;

/**
 * Keyboard-interactive based ssh authentication
 */
typedef struct git_cred_ssh_interactive {
	git_cred parent;
	char *username;
	git_cred_ssh_interactive_callback prompt_callback;
	void *payload;
} git_cred_ssh_interactive;

/**
 * A key with a custom signature function
 */
typedef struct git_cred_ssh_custom {
	git_cred parent;
	char *username;
	char *publickey;
	size_t publickey_len;
	git_cred_sign_callback sign_callback;
	void *payload;
} git_cred_ssh_custom;

#endif
