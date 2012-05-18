/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2/config.h"
#include "git2/types.h"
#include "git2/repository.h"
#include "git2/index.h"
#include "git2/submodule.h"
#include "buffer.h"
#include "vector.h"
#include "posix.h"
#include "config_file.h"
#include "config.h"
#include "repository.h"

static git_cvar_map _sm_update_map[] = {
	{GIT_CVAR_STRING, "checkout", GIT_SUBMODULE_UPDATE_CHECKOUT},
	{GIT_CVAR_STRING, "rebase", GIT_SUBMODULE_UPDATE_REBASE},
	{GIT_CVAR_STRING, "merge", GIT_SUBMODULE_UPDATE_MERGE}
};

static git_cvar_map _sm_ignore_map[] = {
	{GIT_CVAR_STRING, "all", GIT_SUBMODULE_IGNORE_ALL},
	{GIT_CVAR_STRING, "dirty", GIT_SUBMODULE_IGNORE_DIRTY},
	{GIT_CVAR_STRING, "untracked", GIT_SUBMODULE_IGNORE_UNTRACKED},
	{GIT_CVAR_STRING, "none", GIT_SUBMODULE_IGNORE_NONE}
};

static inline khint_t str_hash_no_trailing_slash(const char *s)
{
	khint_t h;

	for (h = 0; *s; ++s)
		if (s[1] || *s != '/')
			h = (h << 5) - h + *s;

	return h;
}

static inline int str_equal_no_trailing_slash(const char *a, const char *b)
{
	size_t alen = a ? strlen(a) : 0;
	size_t blen = b ? strlen(b) : 0;

	if (alen && a[alen] == '/')
		alen--;
	if (blen && b[blen] == '/')
		blen--;

	return (alen == blen && strncmp(a, b, alen) == 0);
}

__KHASH_IMPL(str, static inline, const char *, void *, 1, str_hash_no_trailing_slash, str_equal_no_trailing_slash);

static git_submodule *submodule_alloc(const char *name)
{
	git_submodule *sm = git__calloc(1, sizeof(git_submodule));
	if (sm == NULL)
		return sm;

	sm->path = sm->name = git__strdup(name);
	if (!sm->name) {
		git__free(sm);
		return NULL;
	}

	return sm;
}

static void submodule_release(git_submodule *sm, int decr)
{
	if (!sm)
		return;

	sm->refcount -= decr;

	if (sm->refcount == 0) {
		if (sm->name != sm->path)
			git__free(sm->path);
		git__free(sm->name);
		git__free(sm->url);
		git__free(sm);
	}
}

static int submodule_from_entry(
	git_strmap *smcfg, git_index_entry *entry)
{
	git_submodule *sm;
	void *old_sm;
	khiter_t pos;
	int error;

	pos = git_strmap_lookup_index(smcfg, entry->path);

	if (git_strmap_valid_index(smcfg, pos))
		sm = git_strmap_value_at(smcfg, pos);
	else
		sm = submodule_alloc(entry->path);

	git_oid_cpy(&sm->oid, &entry->oid);

	if (strcmp(sm->path, entry->path) != 0) {
		if (sm->path != sm->name) {
			git__free(sm->path);
			sm->path = sm->name;
		}
		sm->path = git__strdup(entry->path);
		if (!sm->path)
			goto fail;
	}

	git_strmap_insert2(smcfg, sm->path, sm, old_sm, error);
	if (error < 0)
		goto fail;
	sm->refcount++;

	if (old_sm && ((git_submodule *)old_sm) != sm) {
		/* TODO: log warning about multiple entrys for same submodule path */
		submodule_release(old_sm, 1);
	}

	return 0;

fail:
	submodule_release(sm, 0);
	return -1;
}

static int submodule_from_config(
	const char *key, const char *value, void *data)
{
	git_strmap *smcfg = data;
	const char *namestart;
	const char *property;
	git_buf name = GIT_BUF_INIT;
	git_submodule *sm;
	void *old_sm = NULL;
	bool is_path;
	khiter_t pos;
	int error;

	if (git__prefixcmp(key, "submodule.") != 0)
		return 0;

	namestart = key + strlen("submodule.");
	property  = strrchr(namestart, '.');
	if (property == NULL)
		return 0;
	property++;
	is_path = (strcmp(property, "path") == 0);

	if (git_buf_set(&name, namestart, property - namestart - 1) < 0)
		return -1;

	pos = git_strmap_lookup_index(smcfg, name.ptr);
	if (!git_strmap_valid_index(smcfg, pos) && is_path)
		pos = git_strmap_lookup_index(smcfg, value);
	if (!git_strmap_valid_index(smcfg, pos))
		sm = submodule_alloc(name.ptr);
	else
		sm = git_strmap_value_at(smcfg, pos);
	if (!sm)
		goto fail;

	if (strcmp(sm->name, name.ptr) != 0) {
		assert(sm->path == sm->name);
		sm->name = git_buf_detach(&name);

		git_strmap_insert2(smcfg, sm->name, sm, old_sm, error);
		if (error < 0)
			goto fail;
		sm->refcount++;
	}
	else if (is_path && strcmp(sm->path, value) != 0) {
		assert(sm->path == sm->name);
		sm->path = git__strdup(value);
		if (sm->path == NULL)
			goto fail;

		git_strmap_insert2(smcfg, sm->path, sm, old_sm, error);
		if (error < 0)
			goto fail;
		sm->refcount++;
	}
	git_buf_free(&name);

	if (old_sm && ((git_submodule *)old_sm) != sm) {
		/* TODO: log warning about multiple submodules with same path */
		submodule_release(old_sm, 1);
	}

	if (is_path)
		return 0;

	/* copy other properties into submodule entry */
	if (strcmp(property, "url") == 0) {
		if (sm->url) {
			git__free(sm->url);
			sm->url = NULL;
		}
		if ((sm->url = git__strdup(value)) == NULL)
			goto fail;
	}
	else if (strcmp(property, "update") == 0) {
		int val;
		if (git_config_lookup_map_value(
			_sm_update_map, ARRAY_SIZE(_sm_update_map), value, &val) < 0) {
			giterr_set(GITERR_INVALID,
				"Invalid value for submodule update property: '%s'", value);
			goto fail;
		}
		sm->update = (git_submodule_update_t)val;
	}
	else if (strcmp(property, "fetchRecurseSubmodules") == 0) {
		if (git__parse_bool(&sm->fetch_recurse, value) < 0) {
			giterr_set(GITERR_INVALID,
				"Invalid value for submodule 'fetchRecurseSubmodules' property: '%s'", value);
			goto fail;
		}
	}
	else if (strcmp(property, "ignore") == 0) {
		int val;
		if (git_config_lookup_map_value(
			_sm_ignore_map, ARRAY_SIZE(_sm_ignore_map), value, &val) < 0) {
			giterr_set(GITERR_INVALID,
				"Invalid value for submodule ignore property: '%s'", value);
			goto fail;
		}
		sm->ignore = (git_submodule_ignore_t)val;
	}
	/* ignore other unknown submodule properties */

	return 0;

fail:
	submodule_release(sm, 0);
	git_buf_free(&name);
	return -1;
}

static int load_submodule_config(git_repository *repo)
{
	int error;
	git_index *index;
	unsigned int i, max_i;
	git_oid gitmodules_oid;
	git_strmap *smcfg;
	struct git_config_file *mods = NULL;

	if (repo->submodules)
		return 0;

	/* submodule data is kept in a hashtable with each submodule stored
	 * under both its name and its path.  These are usually the same, but
	 * that is not guaranteed.
	 */
	smcfg = git_strmap_alloc();
	GITERR_CHECK_ALLOC(smcfg);

	/* scan index for gitmodules (and .gitmodules entry) */
	if ((error = git_repository_index__weakptr(&index, repo)) < 0)
		goto cleanup;
	memset(&gitmodules_oid, 0, sizeof(gitmodules_oid));
	max_i = git_index_entrycount(index);

	for (i = 0; i < max_i; i++) {
		git_index_entry *entry = git_index_get(index, i);
		if (S_ISGITLINK(entry->mode)) {
			if ((error = submodule_from_entry(smcfg, entry)) < 0)
				goto cleanup;
		}
		else if (strcmp(entry->path, ".gitmodules") == 0)
			git_oid_cpy(&gitmodules_oid, &entry->oid);
	}

	/* load .gitmodules from workdir if it exists */
	if (git_repository_workdir(repo) != NULL) {
		/* look in workdir for .gitmodules */
		git_buf path = GIT_BUF_INIT;
		if (!git_buf_joinpath(
				&path, git_repository_workdir(repo), ".gitmodules") &&
			git_path_isfile(path.ptr))
		{
			if (!(error = git_config_file__ondisk(&mods, path.ptr)))
				error = git_config_file_open(mods);
		}
		git_buf_free(&path);
	}

	/* load .gitmodules from object cache if not in workdir */
	if (!error && mods == NULL && !git_oid_iszero(&gitmodules_oid)) {
		/* TODO: is it worth loading gitmodules from object cache? */
	}

	/* process .gitmodules info */
	if (!error && mods != NULL)
		error = git_config_file_foreach(mods, submodule_from_config, smcfg);

	/* store submodule config in repo */
	if (!error)
		repo->submodules = smcfg;

cleanup:
	if (mods != NULL)
		git_config_file_free(mods);
	if (error)
		git_strmap_free(smcfg);
	return error;
}

void git_submodule_config_free(git_repository *repo)
{
	git_strmap *smcfg = repo->submodules;
	git_submodule *sm;

	repo->submodules = NULL;

	if (smcfg == NULL)
		return;

	git_strmap_foreach_value(smcfg, sm, {
		submodule_release(sm,1);
	});
	git_strmap_free(smcfg);
}

static int submodule_cmp(const void *a, const void *b)
{
	return strcmp(((git_submodule *)a)->name, ((git_submodule *)b)->name);
}

int git_submodule_foreach(
	git_repository *repo,
	int (*callback)(const char *name, void *payload),
	void *payload)
{
	int error;
	git_submodule *sm;
	git_vector seen = GIT_VECTOR_INIT;
	seen._cmp = submodule_cmp;

	if ((error = load_submodule_config(repo)) < 0)
		return error;

	git_strmap_foreach_value(repo->submodules, sm, {
		/* usually the following will not come into play */
		if (sm->refcount > 1) {
			if (git_vector_bsearch(&seen, sm) != GIT_ENOTFOUND)
				continue;
			if ((error = git_vector_insert(&seen, sm)) < 0)
				break;
		}

		if ((error = callback(sm->name, payload)) < 0)
			break;
	});

	git_vector_free(&seen);

	return error;
}

int git_submodule_lookup(
	git_submodule **sm_ptr, /* NULL allowed if user only wants to test */
	git_repository *repo,
	const char *name)       /* trailing slash is allowed */
{
	khiter_t pos;

	if (load_submodule_config(repo) < 0)
		return -1;

	pos = git_strmap_lookup_index(repo->submodules, name);
	if (!git_strmap_valid_index(repo->submodules, pos))
		return GIT_ENOTFOUND;

	if (sm_ptr)
		*sm_ptr = git_strmap_value_at(repo->submodules, pos);

	return 0;
}
