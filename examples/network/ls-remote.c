#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

/** Callback to show each item */
static int show_ref__cb(git_remote_head *head, void *payload)
{
	char oid[GIT_OID_HEXSZ + 1] = {0};

	(void)payload;
	git_oid_fmt(oid, &head->oid);
	printf("%s\t%s\n", oid, head->name);
	return 0;
}

static int use_remote(git_repository *repo, char *name)
{
	git_remote *remote = NULL;
	int error;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;

	// Find the remote by name
	error = git_remote_load(&remote, repo, name);
	if (error < 0) {
		error = git_remote_create_inmemory(&remote, repo, NULL, name);
		if (error < 0)
			goto cleanup;
	}

	/**
	 * Connect to the remote and call the printing function for
	 * each of the remote references.
	 */
	callbacks.credentials = cred_acquire_cb;
	git_remote_set_callbacks(remote, &callbacks);

	error = git_remote_connect(remote, GIT_DIRECTION_FETCH);
	if (error < 0)
		goto cleanup;

	error = git_remote_ls(remote, &show_ref__cb, NULL);

cleanup:
	git_remote_free(remote);
	return error;
}

/** Entry point for this command */
int ls_remote(git_repository *repo, int argc, char **argv)
{
	int error;

	if (argc < 2) {
		fprintf(stderr, "usage: %s ls-remote <remote>\n", argv[-1]);
		return EXIT_FAILURE;
	}

	error = use_remote(repo, argv[1]);

	return error;
}
