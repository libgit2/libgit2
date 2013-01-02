#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

static int show_ref__cb(git_remote_head *head, void *payload)
{
	char oid[GIT_OID_HEXSZ + 1] = {0};

	payload = payload;
	git_oid_fmt(oid, &head->oid);
	printf("%s\t%s\n", oid, head->name);
	return 0;
}

static int use_unnamed(git_repository *repo, const char *url)
{
	git_remote *remote = NULL;
	int error;

	// Create an instance of a remote from the URL. The transport to use
	// is detected from the URL
	error = git_remote_create_inmemory(&remote, repo, NULL, url);
	if (error < 0)
		goto cleanup;

	// When connecting, the underlying code needs to know wether we
	// want to push or fetch
	error = git_remote_connect(remote, GIT_DIRECTION_FETCH);
	if (error < 0)
		goto cleanup;

	// With git_remote_ls we can retrieve the advertised heads
	error = git_remote_ls(remote, &show_ref__cb, NULL);

cleanup:
	git_remote_free(remote);
	return error;
}

static int use_remote(git_repository *repo, char *name)
{
	git_remote *remote = NULL;
	int error;

	// Find the remote by name
	error = git_remote_load(&remote, repo, name);
	if (error < 0)
		goto cleanup;

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

	argc = argc;
	/* If there's a ':' in the name, assume it's an URL */
	if (strchr(argv[1], ':') != NULL) {
		error = use_unnamed(repo, argv[1]);
	} else {
		error = use_remote(repo, argv[1]);
	}

	return error;
}
