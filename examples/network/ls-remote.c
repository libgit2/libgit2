#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

static void show_refs(git_headarray *refs)
{
	int i;
	git_remote_head *head;

// Take each head that the remote has advertised, store the string
// representation of the OID in a buffer and print it

	for(i = 0; i < refs->len; ++i){
		char oid[GIT_OID_HEXSZ + 1] = {0};
		head = refs->heads[i];
		git_oid_fmt(oid, &head->oid);
		printf("%s\t%s\n", oid, head->name);
	}
}

int use_unnamed(git_repository *repo, const char *url)
{
	git_remote *remote = NULL;
	git_headarray refs;
	int error;

	// Create an instance of a remote from the URL. The transport to use
	// is detected from the URL
	error = git_remote_new(&remote, repo, url);
	if (error < GIT_SUCCESS)
		goto cleanup;

	// When connecting, the underlying code needs to know wether we
	// want to push or fetch
	error = git_remote_connect(remote, GIT_DIR_FETCH);
	if (error < GIT_SUCCESS)
		goto cleanup;

	// With git_remote_ls we can retrieve the advertised heads
	error = git_remote_ls(remote, &refs);
	if (error < GIT_SUCCESS)
		goto cleanup;

	show_refs(&refs);

cleanup:
	git_remote_free(remote);

	return error;
}

int use_remote(git_repository *repo, char *name)
{
	git_remote *remote = NULL;
	git_config *cfg = NULL;
	git_headarray refs;
	int error;

	// Load the local configuration for the repository
	error = git_repository_config(&cfg, repo, NULL, NULL);
	if (error < GIT_SUCCESS)
		return error;

	// Find the remote by name
	error = git_remote_get(&remote, cfg, name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_remote_connect(remote, GIT_DIR_FETCH);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_remote_ls(remote, &refs);
	if (error < GIT_SUCCESS)
		goto cleanup;

	show_refs(&refs);

cleanup:
	git_remote_free(remote);

	return error;
}

// This gets called to do the work. The remote can be given either as
// the name of a configured remote or an URL.

int ls_remote(git_repository *repo, int argc, char **argv)
{
	git_headarray heads;
	git_remote_head *head;
	int error, i;

	/* If there's a ':' in the name, assume it's an URL */
	if (strchr(argv[1], ':') != NULL) {
		error = use_unnamed(repo, argv[1]);
	} else {
		error = use_remote(repo, argv[1]);
	}

	return error;
}
