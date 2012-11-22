/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/types.h"
#include "git2/oid.h"

#include "fetchhead.h"
#include "common.h"
#include "fileops.h"
#include "filebuf.h"
#include "refs.h"
#include "repository.h"


int git_fetchhead_ref_cmp(const void *a, const void *b)
{
	const git_fetchhead_ref *one = (const git_fetchhead_ref *)a;
	const git_fetchhead_ref *two = (const git_fetchhead_ref *)b;

	if (one->is_merge && !two->is_merge)
		return -1;
	if (two->is_merge && !one->is_merge)
		return 1;

	return strcmp(one->ref_name, two->ref_name);
}

int git_fetchhead_ref_create(
	git_fetchhead_ref **fetchhead_ref_out,
	git_oid *oid,
	int is_merge,
	const char *ref_name,
	const char *remote_url)
{
	git_fetchhead_ref *fetchhead_ref = NULL;

	assert(fetchhead_ref_out && oid && ref_name && remote_url);

	fetchhead_ref = git__malloc(sizeof(git_fetchhead_ref));
	GITERR_CHECK_ALLOC(fetchhead_ref);

	memset(fetchhead_ref, 0x0, sizeof(git_fetchhead_ref));

	git_oid_cpy(&fetchhead_ref->oid, oid);
	fetchhead_ref->is_merge = is_merge;
	fetchhead_ref->ref_name = git__strdup(ref_name);
	fetchhead_ref->remote_url = git__strdup(remote_url);

	*fetchhead_ref_out = fetchhead_ref;

	return 0;
}

static int fetchhead_ref_write(
	git_filebuf *file,
	git_fetchhead_ref *fetchhead_ref)
{
	char oid[GIT_OID_HEXSZ + 1];
	const char *type, *name;

	assert(file && fetchhead_ref);

	git_oid_fmt(oid, &fetchhead_ref->oid);
	oid[GIT_OID_HEXSZ] = '\0';

	if (git__prefixcmp(fetchhead_ref->ref_name, GIT_REFS_HEADS_DIR) == 0) {
		type = "branch ";
		name = fetchhead_ref->ref_name + strlen(GIT_REFS_HEADS_DIR);
	} else if(git__prefixcmp(fetchhead_ref->ref_name,
		GIT_REFS_TAGS_DIR) == 0) {
		type = "tag ";
		name = fetchhead_ref->ref_name + strlen(GIT_REFS_TAGS_DIR);
	} else {
		type = "";
		name = fetchhead_ref->ref_name;
	}

	return git_filebuf_printf(file, "%s\t%s\t%s'%s' of %s\n",
		oid,
		(fetchhead_ref->is_merge) ? "" : "not-for-merge",
		type,
		name,
		fetchhead_ref->remote_url);
}

int git_fetchhead_write(git_repository *repo, git_vector *fetchhead_refs)
{
	git_filebuf file = GIT_FILEBUF_INIT;
	git_buf path = GIT_BUF_INIT;
	unsigned int i;
	git_fetchhead_ref *fetchhead_ref;

	assert(repo && fetchhead_refs);

	if (git_buf_joinpath(&path, repo->path_repository, GIT_FETCH_HEAD_FILE) < 0)
		return -1;

	if (git_filebuf_open(&file, path.ptr, GIT_FILEBUF_FORCE) < 0) {
		git_buf_free(&path);
		return -1;
	}

	git_buf_free(&path);

	git_vector_sort(fetchhead_refs);

	git_vector_foreach(fetchhead_refs, i, fetchhead_ref)
		fetchhead_ref_write(&file, fetchhead_ref);

	return git_filebuf_commit(&file, GIT_REFS_FILE_MODE);
}

void git_fetchhead_ref_free(git_fetchhead_ref *fetchhead_ref)
{
	if (fetchhead_ref == NULL)
		return;

	git__free(fetchhead_ref->remote_url);
	git__free(fetchhead_ref->ref_name);
	git__free(fetchhead_ref);
}

