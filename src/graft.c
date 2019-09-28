/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "graft.h"

int git__graft_register(git_graftmap *grafts, const git_oid *oid, git_array_oid_t parents)
{
	int res = 0, error = 0;
	size_t k, i;
	git_commit_graft *graft;
	git_oid *parent_oid;

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
	k = git_oidmap_put(grafts, &graft->oid, &res);
	if (res < 0) {
		error = -1;
		goto cleanup;
	}

	git_oidmap_set_value_at(grafts, k, graft);

	return 0;

cleanup:
	git_array_clear(graft->parents);
	git__free(graft);
	return error;
}

int git__graft_unregister(git_graftmap *grafts, const git_oid *oid)
{
	size_t idx;
	git_commit_graft *graft;

	assert(grafts && oid);

	idx = git_oidmap_lookup_index(grafts, oid);
	if (!git_oidmap_valid_index(grafts, idx))
		return GIT_ENOTFOUND;

	graft = git_oidmap_value_at(grafts, idx);
	if (graft) {
		git_oidmap_set_value_at(grafts, idx, NULL);
		git__free(graft);
	}
	git_oidmap_delete_at(grafts, idx);

	return 0;
}

void git__graft_clear(git_graftmap *grafts)
{
	git_commit_graft *graft;

	assert(grafts);

	git_oidmap_foreach_value(grafts, graft, {
		git__free(graft->parents.ptr);
		git__free(graft);
		git_oidmap_delete_at(grafts, __i);
	});
	git_oidmap_clear(grafts);
}

int git__graft_for_oid(git_commit_graft **out, git_graftmap *grafts, const git_oid *oid)
{
	size_t pos;

	assert(out && grafts && oid);

	pos = git_oidmap_lookup_index(grafts, oid);
	if (!git_oidmap_valid_index(grafts, pos))
		return GIT_ENOTFOUND;

	*out = git_oidmap_value_at(grafts, pos);

	return 0;
}
