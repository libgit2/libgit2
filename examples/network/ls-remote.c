#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

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

	// Find the remote by name
	error = git_remote_load(&remote, repo, name);
	if (error < 0) {
		error = git_remote_create_inmemory(&remote, repo, NULL, name);
		if (error < 0)
			goto cleanup;
	}

	git_remote_set_cred_acquire_cb(remote, &cred_acquire_cb, NULL);

	error = git_remote_connect(remote, GIT_DIRECTION_FETCH);
	if (error < 0)
		goto cleanup;

	error = git_remote_ls(remote, &show_ref__cb, NULL);

cleanup:
	git_remote_free(remote);
	return error;
}

// This gets called to do the work. The remote can be given either as
// the name of a configured remote or an URL.

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
