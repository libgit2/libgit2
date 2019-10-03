/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "grafts.h"

struct git_grafts {
	/* Map of `git_commit_graft`s */
	git_oidmap *commits;
};

int git_grafts_new(git_grafts **out)
{
	git_grafts *grafts;

	grafts = git__calloc(1, sizeof(*grafts));
	GIT_ERROR_CHECK_ALLOC(grafts);

	if ((git_oidmap_new(&grafts->commits)) < 0) {
		git__free(grafts);
		return -1;
	}

	*out = grafts;
	return 0;
}

void git_grafts_free(git_grafts *grafts)
{
	if (!grafts)
		return;
	git_grafts_clear(grafts);
	git_oidmap_free(grafts->commits);
	git__free(grafts);
}

void git_grafts_clear(git_grafts *grafts)
{
	git_commit_graft *graft;

	assert(grafts);

	git_oidmap_foreach_value(grafts->commits, graft, {
		git__free(graft->parents.ptr);
		git__free(graft);
	});

	git_oidmap_clear(grafts->commits);
}

int git_grafts_add(git_grafts *grafts, const git_oid *oid, git_array_oid_t parents)
{
	git_commit_graft *graft;
	git_oid *parent_oid;
	int error;
	size_t i;

	assert(grafts && oid);

	graft = git__calloc(1, sizeof(*graft));
	GIT_ERROR_CHECK_ALLOC(graft);

	git_array_init_to_size(graft->parents, git_array_size(parents));
	git_array_foreach(parents, i, parent_oid) {
		git_oid *id = git_array_alloc(graft->parents);
		GIT_ERROR_CHECK_ALLOC(id);

		git_oid_cpy(id, parent_oid);
	}
	git_oid_cpy(&graft->oid, oid);

	if ((error = git_oidmap_set(grafts->commits, &graft->oid, graft)) < 0)
		goto cleanup;

	return 0;

cleanup:
	git_array_clear(graft->parents);
	git__free(graft);
	return error;
}

int git_grafts_remove(git_grafts *grafts, const git_oid *oid)
{
	git_commit_graft *graft;
	int error;

	assert(grafts && oid);

	if ((graft = git_oidmap_get(grafts->commits, oid)) == NULL)
		return GIT_ENOTFOUND;

	if ((error = git_oidmap_delete(grafts->commits, oid)) < 0)
		return error;

	git__free(graft);

	return 0;
}

int git_grafts_get(git_commit_graft **out, git_grafts *grafts, const git_oid *oid)
{
	assert(out && grafts && oid);
	if ((*out = git_oidmap_get(grafts->commits, oid)) == NULL)
		return GIT_ENOTFOUND;
	return 0;
}

size_t git_grafts_size(git_grafts *grafts)
{
	return git_oidmap_size(grafts->commits);
}
