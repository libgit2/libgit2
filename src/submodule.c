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
#include "hashtable.h"
#include "vector.h"
#include "posix.h"
#include "config_file.h"
#include "config.h"
#include "repository.h"

static const char *sm_update_values[] = {
	"checkout",
	"rebase",
	"merge",
	NULL
};

static const char *sm_ignore_values[] = {
	"all",
	"dirty",
	"untracked",
	"none",
	NULL
};

static int lookup_enum(const char **values, const char *str)
{
	int i;
	for (i = 0; values[i]; ++i)
		if (strcasecmp(str, values[i]) == 0)
			return i;
	return -1;
}

static uint32_t strhash_no_trailing_slash(const void *key, int hash_id)
{
	static uint32_t hash_seeds[GIT_HASHTABLE_HASHES] = {
		0x01010101,
		0x12345678,
		0xFEDCBA98
	};

	size_t key_len = key ? strlen((const char *)key) : 0;
	if (key_len > 0 && ((const char *)key)[key_len - 1] == '/')
		key_len--;

	return git__hash(key, (int)key_len, hash_seeds[hash_id]);
}

static int strcmp_no_trailing_slash(const void *a, const void *b)
{
	const char *astr = (const char *)a;
	const char *bstr = (const char *)b;
	size_t alen = a ? strlen(astr) : 0;
	size_t blen = b ? strlen(bstr) : 0;
	int cmp;

	if (alen > 0 && astr[alen - 1] == '/')
		alen--;
	if (blen > 0 && bstr[blen - 1] == '/')
		blen--;

	cmp = strncmp(astr, bstr, min(alen, blen));
	if (cmp == 0)
		cmp = (alen < blen) ? -1 : (alen > blen) ? 1 : 0;

	return cmp;
}

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
	git_hashtable *smcfg, git_index_entry *entry)
{
	git_submodule *sm;
	void *old_sm;

	sm = git_hashtable_lookup(smcfg, entry->path);
	if (!sm)
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

	if (git_hashtable_insert2(smcfg, sm->path, sm, &old_sm) < 0)
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
	git_hashtable *smcfg = data;
	const char *namestart;
	const char *property;
	git_buf name = GIT_BUF_INIT;
	git_submodule *sm;
	void *old_sm = NULL;
	bool is_path;

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

	sm = git_hashtable_lookup(smcfg, name.ptr);
	if (!sm && is_path)
		sm = git_hashtable_lookup(smcfg, value);
	if (!sm)
		sm = submodule_alloc(name.ptr);
	if (!sm)
		goto fail;

	if (strcmp(sm->name, name.ptr) != 0) {
		assert(sm->path == sm->name);
		sm->name = git_buf_detach(&name);
		if (git_hashtable_insert2(smcfg, sm->name, sm, &old_sm) < 0)
			goto fail;
		sm->refcount++;
	}
	else if (is_path && strcmp(sm->path, value) != 0) {
		assert(sm->path == sm->name);
		if ((sm->path = git__strdup(value)) == NULL ||
			git_hashtable_insert2(smcfg, sm->path, sm, &old_sm) < 0)
			goto fail;
		sm->refcount++;
	}

	if (old_sm && ((git_submodule *)old_sm) != sm) {
		/* TODO: log entry about multiple submodules with same path */
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
		int val = lookup_enum(sm_update_values, value);
		if (val < 0) {
			giterr_set(GITERR_INVALID,
				"Invalid value for submodule update property: '%s'", value);
			goto fail;
		}
		sm->update = (git_submodule_update_t)val;
	}
	else if (strcmp(property, "fetchRecurseSubmodules") == 0) {
		if (git_config_parse_bool(&sm->fetch_recurse, value) < 0)
			goto fail;
	}
	else if (strcmp(property, "ignore") == 0) {
		int val = lookup_enum(sm_ignore_values, value);
		if (val < 0) {
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
	git_hashtable *smcfg;
	struct git_config_file *mods = NULL;

	if (repo->submodules)
		return 0;

	/* submodule data is kept in a hashtable with each submodule stored
	 * under both its name and its path.  These are usually the same, but
	 * that is not guaranteed.
	 */
	smcfg = git_hashtable_alloc(
		4, strhash_no_trailing_slash, strcmp_no_trailing_slash);
	GITERR_CHECK_ALLOC(smcfg);

	/* scan index for gitmodules (and .gitmodules entry) */
	if ((error = git_repository_index(&index, repo)) < 0)
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
		git_hashtable_free(smcfg);
	return error;
}

void git_submodule_config_free(git_repository *repo)
{
	git_hashtable *smcfg = repo->submodules;
	git_submodule *sm;

	repo->submodules = NULL;

	if (smcfg == NULL)
		return;

	GIT_HASHTABLE_FOREACH_VALUE(smcfg, sm, { submodule_release(sm,1); });
	git_hashtable_free(smcfg);
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

	GIT_HASHTABLE_FOREACH_VALUE(
		repo->submodules, sm, {
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
	git_submodule *sm;

	if (load_submodule_config(repo) < 0)
		return -1;

	sm = git_hashtable_lookup(repo->submodules, name);

	if (sm_ptr)
		*sm_ptr = sm;

	return sm ? 0 : GIT_ENOTFOUND;
}
