/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "git2/config.h"
#include "git2/sys/config.h"
#include "git2/types.h"
#include "git2/index.h"
#include "buffer.h"
#include "buf_text.h"
#include "vector.h"
#include "posix.h"
#include "config_file.h"
#include "config.h"
#include "repository.h"
#include "submodule.h"
#include "tree.h"
#include "iterator.h"
#include "path.h"
#include "index.h"

#define GIT_MODULES_FILE ".gitmodules"

static git_cvar_map _sm_update_map[] = {
	{GIT_CVAR_STRING, "checkout", GIT_SUBMODULE_UPDATE_CHECKOUT},
	{GIT_CVAR_STRING, "rebase", GIT_SUBMODULE_UPDATE_REBASE},
	{GIT_CVAR_STRING, "merge", GIT_SUBMODULE_UPDATE_MERGE},
	{GIT_CVAR_STRING, "none", GIT_SUBMODULE_UPDATE_NONE},
	{GIT_CVAR_FALSE, NULL, GIT_SUBMODULE_UPDATE_NONE},
	{GIT_CVAR_TRUE, NULL, GIT_SUBMODULE_UPDATE_CHECKOUT},
};

static git_cvar_map _sm_ignore_map[] = {
	{GIT_CVAR_STRING, "none", GIT_SUBMODULE_IGNORE_NONE},
	{GIT_CVAR_STRING, "untracked", GIT_SUBMODULE_IGNORE_UNTRACKED},
	{GIT_CVAR_STRING, "dirty", GIT_SUBMODULE_IGNORE_DIRTY},
	{GIT_CVAR_STRING, "all", GIT_SUBMODULE_IGNORE_ALL},
	{GIT_CVAR_FALSE, NULL, GIT_SUBMODULE_IGNORE_NONE},
	{GIT_CVAR_TRUE, NULL, GIT_SUBMODULE_IGNORE_ALL},
};

static git_cvar_map _sm_recurse_map[] = {
	{GIT_CVAR_STRING, "on-demand", GIT_SUBMODULE_RECURSE_ONDEMAND},
	{GIT_CVAR_FALSE, NULL, GIT_SUBMODULE_RECURSE_NO},
	{GIT_CVAR_TRUE, NULL, GIT_SUBMODULE_RECURSE_YES},
};

static kh_inline khint_t str_hash_no_trailing_slash(const char *s)
{
	khint_t h;

	for (h = 0; *s; ++s)
		if (s[1] != '\0' || *s != '/')
			h = (h << 5) - h + *s;

	return h;
}

static kh_inline int str_equal_no_trailing_slash(const char *a, const char *b)
{
	size_t alen = a ? strlen(a) : 0;
	size_t blen = b ? strlen(b) : 0;

	if (alen > 0 && a[alen - 1] == '/')
		alen--;
	if (blen > 0 && b[blen - 1] == '/')
		blen--;

	return (alen == blen && strncmp(a, b, alen) == 0);
}

__KHASH_IMPL(
	str, static kh_inline, const char *, void *, 1,
	str_hash_no_trailing_slash, str_equal_no_trailing_slash);

static int load_submodule_config(git_repository *repo);
static git_config_backend *open_gitmodules(git_repository *, bool, const git_oid *);
static int lookup_head_remote(git_buf *url, git_repository *repo);
static int submodule_get(git_submodule **, git_repository *, const char *, const char *);
static int submodule_load_from_config(const git_config_entry *, void *);
static int submodule_load_from_wd_lite(git_submodule *, const char *, void *);
static int submodule_update_config(git_submodule *, const char *, const char *, bool, bool);
static void submodule_get_index_status(unsigned int *, git_submodule *);
static void submodule_get_wd_status(unsigned int *, git_submodule *, git_repository *, git_submodule_ignore_t);

static int submodule_cmp(const void *a, const void *b)
{
	return strcmp(((git_submodule *)a)->name, ((git_submodule *)b)->name);
}

static int submodule_config_key_trunc_puts(git_buf *key, const char *suffix)
{
	ssize_t idx = git_buf_rfind(key, '.');
	git_buf_truncate(key, (size_t)(idx + 1));
	return git_buf_puts(key, suffix);
}

/*
 * PUBLIC APIS
 */

int git_submodule_lookup(
	git_submodule **sm_ptr, /* NULL if user only wants to test existence */
	git_repository *repo,
	const char *name)       /* trailing slash is allowed */
{
	int error;
	khiter_t pos;

	assert(repo && name);

	if ((error = load_submodule_config(repo)) < 0)
		return error;

	pos = git_strmap_lookup_index(repo->submodules, name);

	if (!git_strmap_valid_index(repo->submodules, pos)) {
		error = GIT_ENOTFOUND;

		/* check if a plausible submodule exists at path */
		if (git_repository_workdir(repo)) {
			git_buf path = GIT_BUF_INIT;

			if (git_buf_joinpath(&path, git_repository_workdir(repo), name) < 0)
				return -1;

			if (git_path_contains_dir(&path, DOT_GIT))
				error = GIT_EEXISTS;

			git_buf_free(&path);
		}

		giterr_set(GITERR_SUBMODULE, (error == GIT_ENOTFOUND) ?
			"No submodule named '%s'" :
			"Submodule '%s' has not been added yet", name);

		return error;
	}

	if (sm_ptr)
		*sm_ptr = git_strmap_value_at(repo->submodules, pos);

	return 0;
}

int git_submodule_foreach(
	git_repository *repo,
	int (*callback)(git_submodule *sm, const char *name, void *payload),
	void *payload)
{
	int error;
	git_submodule *sm;
	git_vector seen = GIT_VECTOR_INIT;
	git_vector_set_cmp(&seen, submodule_cmp);

	assert(repo && callback);

	if ((error = load_submodule_config(repo)) < 0)
		return error;

	git_strmap_foreach_value(repo->submodules, sm, {
		/* Usually the following will not come into play - it just prevents
		 * us from issuing a callback twice for a submodule where the name
		 * and path are not the same.
		 */
		if (GIT_REFCOUNT_VAL(sm) > 1) {
			if (git_vector_bsearch(NULL, &seen, sm) != GIT_ENOTFOUND)
				continue;
			if ((error = git_vector_insert(&seen, sm)) < 0)
				break;
		}

		if ((error = callback(sm, sm->name, payload)) != 0) {
			giterr_set_after_callback(error);
			break;
		}
	});

	git_vector_free(&seen);

	return error;
}

void git_submodule_config_free(git_repository *repo)
{
	git_strmap *smcfg;
	git_submodule *sm;

	assert(repo);

	smcfg = repo->submodules;
	repo->submodules = NULL;

	if (smcfg == NULL)
		return;

	git_strmap_foreach_value(smcfg, sm, { git_submodule_free(sm); });
	git_strmap_free(smcfg);
}

int git_submodule_add_setup(
	git_submodule **submodule,
	git_repository *repo,
	const char *url,
	const char *path,
	int use_gitlink)
{
	int error = 0;
	git_config_backend *mods = NULL;
	git_submodule *sm;
	git_buf name = GIT_BUF_INIT, real_url = GIT_BUF_INIT;
	git_repository_init_options initopt = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	git_repository *subrepo = NULL;

	assert(repo && url && path);

	/* see if there is already an entry for this submodule */

	if (git_submodule_lookup(&sm, repo, path) < 0)
		giterr_clear();
	else {
		giterr_set(GITERR_SUBMODULE,
			"Attempt to add a submodule that already exists");
		return GIT_EEXISTS;
	}

	/* resolve parameters */
	error = git_submodule_resolve_url(&real_url, repo, url);
	if (error)
		goto cleanup;

	/* validate and normalize path */

	if (git__prefixcmp(path, git_repository_workdir(repo)) == 0)
		path += strlen(git_repository_workdir(repo));

	if (git_path_root(path) >= 0) {
		giterr_set(GITERR_SUBMODULE, "Submodule path must be a relative path");
		error = -1;
		goto cleanup;
	}

	/* update .gitmodules */

	if ((mods = open_gitmodules(repo, true, NULL)) == NULL) {
		giterr_set(GITERR_SUBMODULE,
			"Adding submodules to a bare repository is not supported (for now)");
		return -1;
	}

	if ((error = git_buf_printf(&name, "submodule.%s.path", path)) < 0 ||
		(error = git_config_file_set_string(mods, name.ptr, path)) < 0)
		goto cleanup;

	if ((error = submodule_config_key_trunc_puts(&name, "url")) < 0 ||
		(error = git_config_file_set_string(mods, name.ptr, real_url.ptr)) < 0)
		goto cleanup;

	git_buf_clear(&name);

	/* init submodule repository and add origin remote as needed */

	error = git_buf_joinpath(&name, git_repository_workdir(repo), path);
	if (error < 0)
		goto cleanup;

	/* New style: sub-repo goes in <repo-dir>/modules/<name>/ with a
	 * gitlink in the sub-repo workdir directory to that repository
	 *
	 * Old style: sub-repo goes directly into repo/<name>/.git/
	 */

	initopt.flags = GIT_REPOSITORY_INIT_MKPATH |
		GIT_REPOSITORY_INIT_NO_REINIT;
	initopt.origin_url = real_url.ptr;

	if (git_path_exists(name.ptr) &&
		git_path_contains(&name, DOT_GIT))
	{
		/* repo appears to already exist - reinit? */
	}
	else if (use_gitlink) {
		git_buf repodir = GIT_BUF_INIT;

		error = git_buf_join_n(
			&repodir, '/', 3, git_repository_path(repo), "modules", path);
		if (error < 0)
			goto cleanup;

		initopt.workdir_path = name.ptr;
		initopt.flags |= GIT_REPOSITORY_INIT_NO_DOTGIT_DIR;

		error = git_repository_init_ext(&subrepo, repodir.ptr, &initopt);

		git_buf_free(&repodir);
	}
	else {
		error = git_repository_init_ext(&subrepo, name.ptr, &initopt);
	}
	if (error < 0)
		goto cleanup;

	/* add submodule to hash and "reload" it */

	if (!(error = submodule_get(&sm, repo, path, NULL)) &&
		!(error = git_submodule_reload(sm)))
		error = git_submodule_init(sm, false);

cleanup:
	if (submodule != NULL)
		*submodule = !error ? sm : NULL;

	if (mods != NULL)
		git_config_file_free(mods);
	git_repository_free(subrepo);
	git_buf_free(&real_url);
	git_buf_free(&name);

	return error;
}

int git_submodule_add_finalize(git_submodule *sm)
{
	int error;
	git_index *index;

	assert(sm);

	if ((error = git_repository_index__weakptr(&index, sm->repo)) < 0 ||
		(error = git_index_add_bypath(index, GIT_MODULES_FILE)) < 0)
		return error;

	return git_submodule_add_to_index(sm, true);
}

int git_submodule_add_to_index(git_submodule *sm, int write_index)
{
	int error;
	git_repository *sm_repo = NULL;
	git_index *index;
	git_buf path = GIT_BUF_INIT;
	git_commit *head;
	git_index_entry entry;
	struct stat st;

	assert(sm);

	/* force reload of wd OID by git_submodule_open */
	sm->flags = sm->flags & ~GIT_SUBMODULE_STATUS__WD_OID_VALID;

	if ((error = git_repository_index__weakptr(&index, sm->repo)) < 0 ||
		(error = git_buf_joinpath(
			&path, git_repository_workdir(sm->repo), sm->path)) < 0 ||
		(error = git_submodule_open(&sm_repo, sm)) < 0)
		goto cleanup;

	/* read stat information for submodule working directory */
	if (p_stat(path.ptr, &st) < 0) {
		giterr_set(GITERR_SUBMODULE,
			"Cannot add submodule without working directory");
		error = -1;
		goto cleanup;
	}

	memset(&entry, 0, sizeof(entry));
	entry.path = sm->path;
	git_index_entry__init_from_stat(
		&entry, &st, !(git_index_caps(index) & GIT_INDEXCAP_NO_FILEMODE));

	/* calling git_submodule_open will have set sm->wd_oid if possible */
	if ((sm->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID) == 0) {
		giterr_set(GITERR_SUBMODULE,
			"Cannot add submodule without HEAD to index");
		error = -1;
		goto cleanup;
	}
	git_oid_cpy(&entry.id, &sm->wd_oid);

	if ((error = git_commit_lookup(&head, sm_repo, &sm->wd_oid)) < 0)
		goto cleanup;

	entry.ctime.seconds = git_commit_time(head);
	entry.ctime.nanoseconds = 0;
	entry.mtime.seconds = git_commit_time(head);
	entry.mtime.nanoseconds = 0;

	git_commit_free(head);

	/* add it */
	error = git_index_add(index, &entry);

	/* write it, if requested */
	if (!error && write_index) {
		error = git_index_write(index);

		if (!error)
			git_oid_cpy(&sm->index_oid, &sm->wd_oid);
	}

cleanup:
	git_repository_free(sm_repo);
	git_buf_free(&path);
	return error;
}

const char *git_submodule_ignore_to_str(git_submodule_ignore_t ignore)
{
	int i;
	for (i = 0; i < (int)ARRAY_SIZE(_sm_ignore_map); ++i)
		if (_sm_ignore_map[i].map_value == ignore)
			return _sm_ignore_map[i].str_match;
	return NULL;
}

const char *git_submodule_update_to_str(git_submodule_update_t update)
{
	int i;
	for (i = 0; i < (int)ARRAY_SIZE(_sm_update_map); ++i)
		if (_sm_update_map[i].map_value == update)
			return _sm_update_map[i].str_match;
	return NULL;
}

const char *git_submodule_recurse_to_str(git_submodule_recurse_t recurse)
{
	int i;
	for (i = 0; i < (int)ARRAY_SIZE(_sm_recurse_map); ++i)
		if (_sm_recurse_map[i].map_value == recurse)
			return _sm_recurse_map[i].str_match;
	return NULL;
}

int git_submodule_save(git_submodule *submodule)
{
	int error = 0;
	git_config_backend *mods;
	git_buf key = GIT_BUF_INIT;
	const char *val;

	assert(submodule);

	mods = open_gitmodules(submodule->repo, true, NULL);
	if (!mods) {
		giterr_set(GITERR_SUBMODULE,
			"Adding submodules to a bare repository is not supported (for now)");
		return -1;
	}

	if ((error = git_buf_printf(&key, "submodule.%s.", submodule->name)) < 0)
		goto cleanup;

	/* save values for path, url, update, ignore, fetchRecurseSubmodules */

	if ((error = submodule_config_key_trunc_puts(&key, "path")) < 0 ||
		(error = git_config_file_set_string(mods, key.ptr, submodule->path)) < 0)
		goto cleanup;

	if ((error = submodule_config_key_trunc_puts(&key, "url")) < 0 ||
		(error = git_config_file_set_string(mods, key.ptr, submodule->url)) < 0)
		goto cleanup;

	if ((error = submodule_config_key_trunc_puts(&key, "branch")) < 0 ||
		(error = git_config_file_set_string(mods, key.ptr, submodule->branch)) < 0)
		goto cleanup;

	if (!(error = submodule_config_key_trunc_puts(&key, "update")) &&
		(val = git_submodule_update_to_str(submodule->update)) != NULL)
		error = git_config_file_set_string(mods, key.ptr, val);
	if (error < 0)
		goto cleanup;

	if (!(error = submodule_config_key_trunc_puts(&key, "ignore")) &&
		(val = git_submodule_ignore_to_str(submodule->ignore)) != NULL)
		error = git_config_file_set_string(mods, key.ptr, val);
	if (error < 0)
		goto cleanup;

	if (!(error = submodule_config_key_trunc_puts(&key, "fetchRecurseSubmodules")) &&
		(val = git_submodule_recurse_to_str(submodule->fetch_recurse)) != NULL)
		error = git_config_file_set_string(mods, key.ptr, val);
	if (error < 0)
		goto cleanup;

	/* update internal defaults */

	submodule->ignore_default = submodule->ignore;
	submodule->update_default = submodule->update;
	submodule->fetch_recurse_default = submodule->fetch_recurse;
	submodule->flags |= GIT_SUBMODULE_STATUS_IN_CONFIG;

cleanup:
	if (mods != NULL)
		git_config_file_free(mods);
	git_buf_free(&key);

	return error;
}

git_repository *git_submodule_owner(git_submodule *submodule)
{
	assert(submodule);
	return submodule->repo;
}

const char *git_submodule_name(git_submodule *submodule)
{
	assert(submodule);
	return submodule->name;
}

const char *git_submodule_path(git_submodule *submodule)
{
	assert(submodule);
	return submodule->path;
}

const char *git_submodule_url(git_submodule *submodule)
{
	assert(submodule);
	return submodule->url;
}

int git_submodule_resolve_url(git_buf *out, git_repository *repo, const char *url)
{
	int error = 0;

	assert(url);

	if (url[0] == '.' && (url[1] == '/' || (url[1] == '.' && url[2] == '/'))) {
		if (!(error = lookup_head_remote(out, repo)))
			error = git_path_apply_relative(out, url);
	} else if (strchr(url, ':') != NULL || url[0] == '/') {
		error = git_buf_sets(out, url);
	} else {
		giterr_set(GITERR_SUBMODULE, "Invalid format for submodule URL");
		error = -1;
	}

	return error;
}

const char *git_submodule_branch(git_submodule *submodule)
{
	assert(submodule);
	return submodule->branch;
}

int git_submodule_set_url(git_submodule *submodule, const char *url)
{
	assert(submodule && url);

	git__free(submodule->url);

	submodule->url = git__strdup(url);
	GITERR_CHECK_ALLOC(submodule->url);

	return 0;
}

const git_oid *git_submodule_index_id(git_submodule *submodule)
{
	assert(submodule);

	if (submodule->flags & GIT_SUBMODULE_STATUS__INDEX_OID_VALID)
		return &submodule->index_oid;
	else
		return NULL;
}

const git_oid *git_submodule_head_id(git_submodule *submodule)
{
	assert(submodule);

	if (submodule->flags & GIT_SUBMODULE_STATUS__HEAD_OID_VALID)
		return &submodule->head_oid;
	else
		return NULL;
}

const git_oid *git_submodule_wd_id(git_submodule *submodule)
{
	assert(submodule);

	/* load unless we think we have a valid oid */
	if (!(submodule->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID)) {
		git_repository *subrepo;

		/* calling submodule open grabs the HEAD OID if possible */
		if (!git_submodule_open_bare(&subrepo, submodule))
			git_repository_free(subrepo);
		else
			giterr_clear();
	}

	if (submodule->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID)
		return &submodule->wd_oid;
	else
		return NULL;
}

git_submodule_ignore_t git_submodule_ignore(git_submodule *submodule)
{
	assert(submodule);
	return (submodule->ignore < GIT_SUBMODULE_IGNORE_NONE) ?
		GIT_SUBMODULE_IGNORE_NONE : submodule->ignore;
}

git_submodule_ignore_t git_submodule_set_ignore(
	git_submodule *submodule, git_submodule_ignore_t ignore)
{
	git_submodule_ignore_t old;

	assert(submodule);

	if (ignore == GIT_SUBMODULE_IGNORE_RESET)
		ignore = submodule->ignore_default;

	old = submodule->ignore;
	submodule->ignore = ignore;
	return old;
}

git_submodule_update_t git_submodule_update(git_submodule *submodule)
{
	assert(submodule);
	return (submodule->update < GIT_SUBMODULE_UPDATE_CHECKOUT) ?
		GIT_SUBMODULE_UPDATE_CHECKOUT : submodule->update;
}

git_submodule_update_t git_submodule_set_update(
	git_submodule *submodule, git_submodule_update_t update)
{
	git_submodule_update_t old;

	assert(submodule);

	if (update == GIT_SUBMODULE_UPDATE_RESET)
		update = submodule->update_default;

	old = submodule->update;
	submodule->update = update;
	return old;
}

git_submodule_recurse_t git_submodule_fetch_recurse_submodules(
	git_submodule *submodule)
{
	assert(submodule);
	return submodule->fetch_recurse;
}

git_submodule_recurse_t git_submodule_set_fetch_recurse_submodules(
	git_submodule *submodule,
	git_submodule_recurse_t fetch_recurse_submodules)
{
	git_submodule_recurse_t old;

	assert(submodule);

	if (fetch_recurse_submodules == GIT_SUBMODULE_RECURSE_RESET)
		fetch_recurse_submodules = submodule->fetch_recurse_default;

	old = submodule->fetch_recurse;
	submodule->fetch_recurse = fetch_recurse_submodules;
	return old;
}

int git_submodule_init(git_submodule *submodule, int overwrite)
{
	int error;
	const char *val;

	/* write "submodule.NAME.url" */

	if (!submodule->url) {
		giterr_set(GITERR_SUBMODULE,
			"No URL configured for submodule '%s'", submodule->name);
		return -1;
	}

	error = submodule_update_config(
		submodule, "url", submodule->url, overwrite != 0, false);
	if (error < 0)
		return error;

	/* write "submodule.NAME.update" if not default */

	val = (submodule->update == GIT_SUBMODULE_UPDATE_CHECKOUT) ?
		NULL : git_submodule_update_to_str(submodule->update);
	error = submodule_update_config(
		submodule, "update", val, (overwrite != 0), false);

	return error;
}

int git_submodule_sync(git_submodule *submodule)
{
	if (!submodule->url) {
		giterr_set(GITERR_SUBMODULE,
			"No URL configured for submodule '%s'", submodule->name);
		return -1;
	}

	/* copy URL over to config only if it already exists */

	return submodule_update_config(
		submodule, "url", submodule->url, true, true);
}

static int git_submodule__open(
	git_repository **subrepo, git_submodule *sm, bool bare)
{
	int error;
	git_buf path = GIT_BUF_INIT;
	unsigned int flags = GIT_REPOSITORY_OPEN_NO_SEARCH;
	const char *wd;

	assert(sm && subrepo);

	if (git_repository__ensure_not_bare(
			sm->repo, "open submodule repository") < 0)
		return GIT_EBAREREPO;

	wd = git_repository_workdir(sm->repo);

	if (git_buf_joinpath(&path, wd, sm->path) < 0 ||
		git_buf_joinpath(&path, path.ptr, DOT_GIT) < 0)
		return -1;

	sm->flags = sm->flags &
		~(GIT_SUBMODULE_STATUS_IN_WD |
		  GIT_SUBMODULE_STATUS__WD_OID_VALID |
		  GIT_SUBMODULE_STATUS__WD_SCANNED);

	if (bare)
		flags |= GIT_REPOSITORY_OPEN_BARE;

	error = git_repository_open_ext(subrepo, path.ptr, flags, wd);

	/* if we opened the submodule successfully, grab HEAD OID, etc. */
	if (!error) {
		sm->flags |= GIT_SUBMODULE_STATUS_IN_WD |
			GIT_SUBMODULE_STATUS__WD_SCANNED;

		if (!git_reference_name_to_id(&sm->wd_oid, *subrepo, GIT_HEAD_FILE))
			sm->flags |= GIT_SUBMODULE_STATUS__WD_OID_VALID;
		else
			giterr_clear();
	} else if (git_path_exists(path.ptr)) {
		sm->flags |= GIT_SUBMODULE_STATUS__WD_SCANNED |
			GIT_SUBMODULE_STATUS_IN_WD;
	} else {
		git_buf_rtruncate_at_char(&path, '/'); /* remove "/.git" */

		if (git_path_isdir(path.ptr))
			sm->flags |= GIT_SUBMODULE_STATUS__WD_SCANNED;
	}

	git_buf_free(&path);

	return error;
}

int git_submodule_open_bare(git_repository **subrepo, git_submodule *sm)
{
	return git_submodule__open(subrepo, sm, true);
}

int git_submodule_open(git_repository **subrepo, git_submodule *sm)
{
	return git_submodule__open(subrepo, sm, false);
}

int git_submodule_reload_all(git_repository *repo)
{
	assert(repo);
	git_submodule_config_free(repo);
	return load_submodule_config(repo);
}

static void submodule_update_from_index_entry(
	git_submodule *sm, const git_index_entry *ie)
{
	bool already_found = (sm->flags & GIT_SUBMODULE_STATUS_IN_INDEX) != 0;

	if (!S_ISGITLINK(ie->mode)) {
		if (!already_found)
			sm->flags |= GIT_SUBMODULE_STATUS__INDEX_NOT_SUBMODULE;
	} else {
		if (already_found)
			sm->flags |= GIT_SUBMODULE_STATUS__INDEX_MULTIPLE_ENTRIES;
		else
			git_oid_cpy(&sm->index_oid, &ie->id);

		sm->flags |= GIT_SUBMODULE_STATUS_IN_INDEX |
			GIT_SUBMODULE_STATUS__INDEX_OID_VALID;
	}
}

static int submodule_update_index(git_submodule *sm)
{
	git_index *index;
	const git_index_entry *ie;

	if (git_repository_index__weakptr(&index, sm->repo) < 0)
		return -1;

	sm->flags = sm->flags &
		~(GIT_SUBMODULE_STATUS_IN_INDEX |
		  GIT_SUBMODULE_STATUS__INDEX_OID_VALID);

	if (!(ie = git_index_get_bypath(index, sm->path, 0)))
		return 0;

	submodule_update_from_index_entry(sm, ie);

	return 0;
}

static void submodule_update_from_head_data(
	git_submodule *sm, mode_t mode, const git_oid *id)
{
	if (!S_ISGITLINK(mode))
		sm->flags |= GIT_SUBMODULE_STATUS__HEAD_NOT_SUBMODULE;
	else {
		git_oid_cpy(&sm->head_oid, id);

		sm->flags |= GIT_SUBMODULE_STATUS_IN_HEAD |
			GIT_SUBMODULE_STATUS__HEAD_OID_VALID;
	}
}

static int submodule_update_head(git_submodule *submodule)
{
	git_tree *head = NULL;
	git_tree_entry *te = NULL;

	submodule->flags = submodule->flags &
		~(GIT_SUBMODULE_STATUS_IN_HEAD |
		  GIT_SUBMODULE_STATUS__HEAD_OID_VALID);

	/* if we can't look up file in current head, then done */
	if (git_repository_head_tree(&head, submodule->repo) < 0 ||
		git_tree_entry_bypath(&te, head, submodule->path) < 0)
		giterr_clear();
	else
		submodule_update_from_head_data(submodule, te->attr, &te->oid);

	git_tree_entry_free(te);
	git_tree_free(head);
	return 0;
}

int git_submodule_reload(git_submodule *submodule)
{
	int error = 0;
	git_config_backend *mods;

	assert(submodule);

	/* refresh index data */
	if ((error = submodule_update_index(submodule)) < 0)
		return error;

	/* refresh HEAD tree data */
	if ((error = submodule_update_head(submodule)) < 0)
		return error;

	/* refresh config data */
	mods = open_gitmodules(submodule->repo, false, NULL);
	if (mods != NULL) {
		git_buf path = GIT_BUF_INIT;

		git_buf_sets(&path, "submodule\\.");
		git_buf_text_puts_escape_regex(&path, submodule->name);
		git_buf_puts(&path, ".*");

		if (git_buf_oom(&path))
			error = -1;
		else
			error = git_config_file_foreach_match(
				mods, path.ptr, submodule_load_from_config, submodule->repo);

		git_buf_free(&path);
		git_config_file_free(mods);

		if (error < 0)
			return error;
	}

	/* refresh wd data */
	submodule->flags = submodule->flags &
		~(GIT_SUBMODULE_STATUS_IN_WD | GIT_SUBMODULE_STATUS__WD_OID_VALID);

	return submodule_load_from_wd_lite(
		submodule, submodule->path, submodule->repo);
}

static void submodule_copy_oid_maybe(
	git_oid *tgt, const git_oid *src, bool valid)
{
	if (tgt) {
		if (valid)
			memcpy(tgt, src, sizeof(*tgt));
		else
			memset(tgt, 0, sizeof(*tgt));
	}
}

int git_submodule__status(
	unsigned int *out_status,
	git_oid *out_head_id,
	git_oid *out_index_id,
	git_oid *out_wd_id,
	git_submodule *sm,
	git_submodule_ignore_t ign)
{
	unsigned int status;
	git_repository *smrepo = NULL;

	if (ign < GIT_SUBMODULE_IGNORE_NONE)
		ign = sm->ignore;

	/* only return location info if ignore == all */
	if (ign == GIT_SUBMODULE_IGNORE_ALL) {
		*out_status = (sm->flags & GIT_SUBMODULE_STATUS__IN_FLAGS);
		return 0;
	}

	/* refresh the index OID */
	if (submodule_update_index(sm) < 0)
		return -1;

	/* refresh the HEAD OID */
	if (submodule_update_head(sm) < 0)
		return -1;

	/* for ignore == dirty, don't scan the working directory */
	if (ign == GIT_SUBMODULE_IGNORE_DIRTY) {
		/* git_submodule_open_bare will load WD OID data */
		if (git_submodule_open_bare(&smrepo, sm) < 0)
			giterr_clear();
		else
			git_repository_free(smrepo);
		smrepo = NULL;
	} else if (git_submodule_open(&smrepo, sm) < 0) {
		giterr_clear();
		smrepo = NULL;
	}

	status = GIT_SUBMODULE_STATUS__CLEAR_INTERNAL(sm->flags);

	submodule_get_index_status(&status, sm);
	submodule_get_wd_status(&status, sm, smrepo, ign);

	git_repository_free(smrepo);

	*out_status = status;

	submodule_copy_oid_maybe(out_head_id, &sm->head_oid,
		(sm->flags & GIT_SUBMODULE_STATUS__HEAD_OID_VALID) != 0);
	submodule_copy_oid_maybe(out_index_id, &sm->index_oid,
		(sm->flags & GIT_SUBMODULE_STATUS__INDEX_OID_VALID) != 0);
	submodule_copy_oid_maybe(out_wd_id, &sm->wd_oid,
		(sm->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID) != 0);

	return 0;
}

int git_submodule_status(unsigned int *status, git_submodule *sm)
{
	assert(status && sm);

	return git_submodule__status(status, NULL, NULL, NULL, sm, 0);
}

int git_submodule_location(unsigned int *location, git_submodule *sm)
{
	assert(location && sm);

	return git_submodule__status(
		location, NULL, NULL, NULL, sm, GIT_SUBMODULE_IGNORE_ALL);
}


/*
 * INTERNAL FUNCTIONS
 */

static git_submodule *submodule_alloc(git_repository *repo, const char *name)
{
	size_t namelen;
	git_submodule *sm;

	if (!name || !(namelen = strlen(name))) {
		giterr_set(GITERR_SUBMODULE, "Invalid submodule name");
		return NULL;
	}

	sm = git__calloc(1, sizeof(git_submodule));
	if (sm == NULL)
		return NULL;

	sm->name = sm->path = git__strdup(name);
	if (!sm->name) {
		git__free(sm);
		return NULL;
	}

	GIT_REFCOUNT_INC(sm);
	sm->ignore = sm->ignore_default = GIT_SUBMODULE_IGNORE_NONE;
	sm->update = sm->update_default = GIT_SUBMODULE_UPDATE_CHECKOUT;
	sm->fetch_recurse = sm->fetch_recurse_default = GIT_SUBMODULE_RECURSE_NO;
	sm->repo   = repo;
	sm->branch = NULL;

	return sm;
}

static void submodule_release(git_submodule *sm)
{
	if (!sm)
		return;

	if (sm->path != sm->name)
		git__free(sm->path);
	git__free(sm->name);
	git__free(sm->url);
	git__memzero(sm, sizeof(*sm));
	git__free(sm);
}

void git_submodule_free(git_submodule *sm)
{
	if (!sm)
		return;
	GIT_REFCOUNT_DEC(sm, submodule_release);
}

static int submodule_get(
	git_submodule **sm_ptr,
	git_repository *repo,
	const char *name,
	const char *alternate)
{
	git_strmap *smcfg = repo->submodules;
	khiter_t pos;
	git_submodule *sm;
	int error;

	assert(repo && name);

	pos = git_strmap_lookup_index(smcfg, name);

	if (!git_strmap_valid_index(smcfg, pos) && alternate)
		pos = git_strmap_lookup_index(smcfg, alternate);

	if (!git_strmap_valid_index(smcfg, pos)) {
		sm = submodule_alloc(repo, name);
		GITERR_CHECK_ALLOC(sm);

		/* insert value at name - if another thread beats us to it, then use
		 * their record and release our own.
		 */
		pos = kh_put(str, smcfg, sm->name, &error);

		if (error < 0) {
			git_submodule_free(sm);
			sm = NULL;
		} else if (error == 0) {
			git_submodule_free(sm);
			sm = git_strmap_value_at(smcfg, pos);
		} else {
			git_strmap_set_value_at(smcfg, pos, sm);
		}
	} else {
		sm = git_strmap_value_at(smcfg, pos);
	}

	*sm_ptr = sm;

	return (sm != NULL) ? 0 : -1;
}

static int submodule_config_error(const char *property, const char *value)
{
	giterr_set(GITERR_INVALID,
		"Invalid value for submodule '%s' property: '%s'", property, value);
	return -1;
}

int git_submodule_parse_ignore(git_submodule_ignore_t *out, const char *value)
{
	int val;

	if (git_config_lookup_map_value(
			&val, _sm_ignore_map, ARRAY_SIZE(_sm_ignore_map), value) < 0) {
		*out = GIT_SUBMODULE_IGNORE_NONE;
		return submodule_config_error("ignore", value);
	}

	*out = (git_submodule_ignore_t)val;
	return 0;
}

int git_submodule_parse_update(git_submodule_update_t *out, const char *value)
{
	int val;

	if (git_config_lookup_map_value(
			&val, _sm_update_map, ARRAY_SIZE(_sm_update_map), value) < 0) {
		*out = GIT_SUBMODULE_UPDATE_CHECKOUT;
		return submodule_config_error("update", value);
	}

	*out = (git_submodule_update_t)val;
	return 0;
}

int git_submodule_parse_recurse(git_submodule_recurse_t *out, const char *value)
{
	int val;

	if (git_config_lookup_map_value(
			&val, _sm_recurse_map, ARRAY_SIZE(_sm_recurse_map), value) < 0) {
		*out = GIT_SUBMODULE_RECURSE_YES;
		return submodule_config_error("recurse", value);
	}

	*out = (git_submodule_recurse_t)val;
	return 0;
}

static int submodule_load_from_config(
	const git_config_entry *entry, void *payload)
{
	git_repository *repo = payload;
	git_strmap *smcfg = repo->submodules;
	const char *namestart, *property, *alternate = NULL;
	const char *key = entry->name, *value = entry->value, *path;
	git_buf name = GIT_BUF_INIT;
	git_submodule *sm;
	int error = 0;

	if (git__prefixcmp(key, "submodule.") != 0)
		return 0;

	namestart = key + strlen("submodule.");
	property  = strrchr(namestart, '.');

	if (!property || (property == namestart))
		return 0;

	property++;
	path = !strcasecmp(property, "path") ? value : NULL;

	if ((error = git_buf_set(&name, namestart, property - namestart - 1)) < 0 ||
		(error = submodule_get(&sm, repo, name.ptr, path)) < 0)
		goto done;

	sm->flags |= GIT_SUBMODULE_STATUS_IN_CONFIG;

	/* Only from config might we get differing names & paths.  If so, then
	 * update the submodule and insert under the alternative key.
	 */

	/* TODO: if case insensitive filesystem, then the following strcmps
	 * should be strcasecmp
	 */

	if (strcmp(sm->name, name.ptr) != 0) {
		alternate = sm->name = git_buf_detach(&name);
	} else if (path && strcmp(path, sm->path) != 0) {
		alternate = sm->path = git__strdup(value);
		if (!sm->path) {
			error = -1;
			goto done;
		}
	}

	if (alternate) {
		void *old_sm = NULL;
		git_strmap_insert2(smcfg, alternate, sm, old_sm, error);

		if (error < 0)
			goto done;
		if (error >= 0)
			GIT_REFCOUNT_INC(sm); /* inserted under a new key */

		/* if we replaced an old module under this key, release the old one */
		if (old_sm && ((git_submodule *)old_sm) != sm) {
			git_submodule_free(old_sm);
			/* TODO: log warning about multiple submodules with same path */
		}
	}

	/* TODO: Look up path in index and if it is present but not a GITLINK
	 * then this should be deleted (at least to match git's behavior)
	 */

	if (path)
		goto done;

	/* copy other properties into submodule entry */
	if (strcasecmp(property, "url") == 0) {
		git__free(sm->url);
		sm->url = NULL;

		if (value != NULL && (sm->url = git__strdup(value)) == NULL) {
			error = -1;
			goto done;
		}
	}
	else if (strcasecmp(property, "branch") == 0) {
		git__free(sm->branch);
		sm->branch = NULL;

		if (value != NULL && (sm->branch = git__strdup(value)) == NULL) {
			error = -1;
			goto done;
		}
	}
	else if (strcasecmp(property, "update") == 0) {
		if ((error = git_submodule_parse_update(&sm->update, value)) < 0)
			goto done;
		sm->update_default = sm->update;
	}
	else if (strcasecmp(property, "fetchRecurseSubmodules") == 0) {
		if (git_submodule_parse_recurse(&sm->fetch_recurse, value) < 0)
			return -1;
		sm->fetch_recurse_default = sm->fetch_recurse;
	}
	else if (strcasecmp(property, "ignore") == 0) {
		if ((error = git_submodule_parse_ignore(&sm->ignore, value)) < 0)
			goto done;
		sm->ignore_default = sm->ignore;
	}
	/* ignore other unknown submodule properties */

done:
	git_buf_free(&name);
	return error;
}

static int submodule_load_from_wd_lite(
	git_submodule *sm, const char *name, void *payload)
{
	git_buf path = GIT_BUF_INIT;

	GIT_UNUSED(name); GIT_UNUSED(payload);

	if (git_repository_is_bare(sm->repo))
		return 0;

	if (git_buf_joinpath(&path, git_repository_workdir(sm->repo), sm->path) < 0)
		return -1;

	if (git_path_isdir(path.ptr))
		sm->flags |= GIT_SUBMODULE_STATUS__WD_SCANNED;

	if (git_path_contains(&path, DOT_GIT))
		sm->flags |= GIT_SUBMODULE_STATUS_IN_WD;

	git_buf_free(&path);
	return 0;
}

static int load_submodule_config_from_index(
	git_repository *repo, git_oid *gitmodules_oid)
{
	int error;
	git_index *index;
	git_iterator *i;
	const git_index_entry *entry;

	if ((error = git_repository_index__weakptr(&index, repo)) < 0 ||
		(error = git_iterator_for_index(&i, index, 0, NULL, NULL)) < 0)
		return error;

	while (!(error = git_iterator_advance(&entry, i))) {
		khiter_t pos = git_strmap_lookup_index(repo->submodules, entry->path);
		git_submodule *sm;

		if (git_strmap_valid_index(repo->submodules, pos)) {
			sm = git_strmap_value_at(repo->submodules, pos);

			if (S_ISGITLINK(entry->mode))
				submodule_update_from_index_entry(sm, entry);
			else
				sm->flags |= GIT_SUBMODULE_STATUS__INDEX_NOT_SUBMODULE;
		} else if (S_ISGITLINK(entry->mode)) {
			if (!submodule_get(&sm, repo, entry->path, NULL))
				submodule_update_from_index_entry(sm, entry);
		} else if (strcmp(entry->path, GIT_MODULES_FILE) == 0)
			git_oid_cpy(gitmodules_oid, &entry->id);
	}

	if (error == GIT_ITEROVER)
		error = 0;

	git_iterator_free(i);

	return error;
}

static int load_submodule_config_from_head(
	git_repository *repo, git_oid *gitmodules_oid)
{
	int error;
	git_tree *head;
	git_iterator *i;
	const git_index_entry *entry;

	/* if we can't look up current head, then there's no submodule in it */
	if (git_repository_head_tree(&head, repo) < 0) {
		giterr_clear();
		return 0;
	}

	if ((error = git_iterator_for_tree(&i, head, 0, NULL, NULL)) < 0) {
		git_tree_free(head);
		return error;
	}

	while (!(error = git_iterator_advance(&entry, i))) {
		khiter_t pos = git_strmap_lookup_index(repo->submodules, entry->path);
		git_submodule *sm;

		if (git_strmap_valid_index(repo->submodules, pos)) {
			sm = git_strmap_value_at(repo->submodules, pos);

			if (S_ISGITLINK(entry->mode))
				submodule_update_from_head_data(
					sm, entry->mode, &entry->id);
			else
				sm->flags |= GIT_SUBMODULE_STATUS__HEAD_NOT_SUBMODULE;
		} else if (S_ISGITLINK(entry->mode)) {
			if (!submodule_get(&sm, repo, entry->path, NULL))
				submodule_update_from_head_data(
					sm, entry->mode, &entry->id);
		} else if (strcmp(entry->path, GIT_MODULES_FILE) == 0 &&
				   git_oid_iszero(gitmodules_oid)) {
			git_oid_cpy(gitmodules_oid, &entry->id);
		}
	}

	if (error == GIT_ITEROVER)
		error = 0;

	git_iterator_free(i);
	git_tree_free(head);

	return error;
}

static git_config_backend *open_gitmodules(
	git_repository *repo,
	bool okay_to_create,
	const git_oid *gitmodules_oid)
{
	const char *workdir = git_repository_workdir(repo);
	git_buf path = GIT_BUF_INIT;
	git_config_backend *mods = NULL;

	if (workdir != NULL) {
		if (git_buf_joinpath(&path, workdir, GIT_MODULES_FILE) != 0)
			return NULL;

		if (okay_to_create || git_path_isfile(path.ptr)) {
			/* git_config_file__ondisk should only fail if OOM */
			if (git_config_file__ondisk(&mods, path.ptr) < 0)
				mods = NULL;
			/* open should only fail here if the file is malformed */
			else if (git_config_file_open(mods, GIT_CONFIG_LEVEL_LOCAL) < 0) {
				git_config_file_free(mods);
				mods = NULL;
			}
		}
	}

	if (!mods && gitmodules_oid && !git_oid_iszero(gitmodules_oid)) {
		/* TODO: Retrieve .gitmodules content from ODB */

		/* Should we actually do this?  Core git does not, but it means you
		 * can't really get much information about submodules on bare repos.
		 */
	}

	git_buf_free(&path);

	return mods;
}

static int load_submodule_config(git_repository *repo)
{
	int error;
	git_oid gitmodules_oid;
	git_config_backend *mods = NULL;

	if (repo->submodules)
		return 0;

	memset(&gitmodules_oid, 0, sizeof(gitmodules_oid));

	/* Submodule data is kept in a hashtable keyed by both name and path.
	 * These are usually the same, but that is not guaranteed.
	 */
	if (!repo->submodules) {
		repo->submodules = git_strmap_alloc();
		GITERR_CHECK_ALLOC(repo->submodules);
	}

	/* add submodule information from index */

	if ((error = load_submodule_config_from_index(repo, &gitmodules_oid)) < 0)
		goto cleanup;

	/* add submodule information from HEAD */

	if ((error = load_submodule_config_from_head(repo, &gitmodules_oid)) < 0)
		goto cleanup;

	/* add submodule information from .gitmodules */

	if ((mods = open_gitmodules(repo, false, &gitmodules_oid)) != NULL &&
		(error = git_config_file_foreach(
			mods, submodule_load_from_config, repo)) < 0)
		goto cleanup;

	/* shallow scan submodules in work tree */

	if (!git_repository_is_bare(repo))
		error = git_submodule_foreach(repo, submodule_load_from_wd_lite, NULL);

cleanup:
	if (mods != NULL)
		git_config_file_free(mods);

	if (error)
		git_submodule_config_free(repo);

	return error;
}

static int lookup_head_remote(git_buf *url, git_repository *repo)
{
	int error;
	git_config *cfg;
	git_reference *head = NULL, *remote = NULL;
	const char *tgt, *scan;
	git_buf key = GIT_BUF_INIT;

	/* 1. resolve HEAD -> refs/heads/BRANCH
	 * 2. lookup config branch.BRANCH.remote -> ORIGIN
	 * 3. lookup remote.ORIGIN.url
	 */

	if ((error = git_repository_config__weakptr(&cfg, repo)) < 0)
		return error;

	if (git_reference_lookup(&head, repo, GIT_HEAD_FILE) < 0) {
		giterr_set(GITERR_SUBMODULE,
			"Cannot resolve relative URL when HEAD cannot be resolved");
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	if (git_reference_type(head) != GIT_REF_SYMBOLIC) {
		giterr_set(GITERR_SUBMODULE,
			"Cannot resolve relative URL when HEAD is not symbolic");
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	if ((error = git_branch_upstream(&remote, head)) < 0)
		goto cleanup;

	/* remote should refer to something like refs/remotes/ORIGIN/BRANCH */

	if (git_reference_type(remote) != GIT_REF_SYMBOLIC ||
		git__prefixcmp(git_reference_symbolic_target(remote), GIT_REFS_REMOTES_DIR) != 0)
	{
		giterr_set(GITERR_SUBMODULE,
			"Cannot resolve relative URL when HEAD is not symbolic");
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	scan = tgt = git_reference_symbolic_target(remote) + strlen(GIT_REFS_REMOTES_DIR);
	while (*scan && (*scan != '/' || (scan > tgt && scan[-1] != '\\')))
		scan++; /* find non-escaped slash to end ORIGIN name */

	error = git_buf_printf(&key, "remote.%.*s.url", (int)(scan - tgt), tgt);
	if (error < 0)
		goto cleanup;

	if ((error = git_config_get_string(&tgt, cfg, key.ptr)) < 0)
		goto cleanup;

	error = git_buf_sets(url, tgt);

cleanup:
	git_buf_free(&key);
	git_reference_free(head);
	git_reference_free(remote);

	return error;
}

static int submodule_update_config(
	git_submodule *submodule,
	const char *attr,
	const char *value,
	bool overwrite,
	bool only_existing)
{
	int error;
	git_config *config;
	git_buf key = GIT_BUF_INIT;
	const git_config_entry *ce = NULL;

	assert(submodule);

	error = git_repository_config__weakptr(&config, submodule->repo);
	if (error < 0)
		return error;

	error = git_buf_printf(&key, "submodule.%s.%s", submodule->name, attr);
	if (error < 0)
		goto cleanup;

	if ((error = git_config__lookup_entry(&ce, config, key.ptr, false)) < 0)
		goto cleanup;

	if (!ce && only_existing)
		goto cleanup;
	if (ce && !overwrite)
		goto cleanup;
	if (value && ce && ce->value && !strcmp(ce->value, value))
		goto cleanup;
	if (!value && (!ce || !ce->value))
		goto cleanup;

	if (!value)
		error = git_config_delete_entry(config, key.ptr);
	else
		error = git_config_set_string(config, key.ptr, value);

cleanup:
	git_buf_free(&key);
	return error;
}

static void submodule_get_index_status(unsigned int *status, git_submodule *sm)
{
	const git_oid *head_oid  = git_submodule_head_id(sm);
	const git_oid *index_oid = git_submodule_index_id(sm);

	*status = *status & ~GIT_SUBMODULE_STATUS__INDEX_FLAGS;

	if (!head_oid) {
		if (index_oid)
			*status |= GIT_SUBMODULE_STATUS_INDEX_ADDED;
	}
	else if (!index_oid)
		*status |= GIT_SUBMODULE_STATUS_INDEX_DELETED;
	else if (!git_oid_equal(head_oid, index_oid))
		*status |= GIT_SUBMODULE_STATUS_INDEX_MODIFIED;
}

static void submodule_get_wd_status(
	unsigned int *status,
	git_submodule *sm,
	git_repository *sm_repo,
	git_submodule_ignore_t ign)
{
	const git_oid *index_oid = git_submodule_index_id(sm);
	const git_oid *wd_oid =
		(sm->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID) ? &sm->wd_oid : NULL;
	git_tree *sm_head = NULL;
	git_index *index = NULL;
	git_diff_options opt = GIT_DIFF_OPTIONS_INIT;
	git_diff *diff;

	*status = *status & ~GIT_SUBMODULE_STATUS__WD_FLAGS;

	if (!index_oid) {
		if (wd_oid)
			*status |= GIT_SUBMODULE_STATUS_WD_ADDED;
	}
	else if (!wd_oid) {
		if ((sm->flags & GIT_SUBMODULE_STATUS__WD_SCANNED) != 0 &&
			(sm->flags & GIT_SUBMODULE_STATUS_IN_WD) == 0)
			*status |= GIT_SUBMODULE_STATUS_WD_UNINITIALIZED;
		else
			*status |= GIT_SUBMODULE_STATUS_WD_DELETED;
	}
	else if (!git_oid_equal(index_oid, wd_oid))
		*status |= GIT_SUBMODULE_STATUS_WD_MODIFIED;

	/* if we have no repo, then we're done */
	if (!sm_repo)
		return;

	/* the diffs below could be optimized with an early termination
	 * option to the git_diff functions, but for now this is sufficient
	 * (and certainly no worse that what core git does).
	 */

	if (ign == GIT_SUBMODULE_IGNORE_NONE)
		opt.flags |= GIT_DIFF_INCLUDE_UNTRACKED;

	(void)git_repository_index__weakptr(&index, sm_repo);

	/* if we don't have an unborn head, check diff with index */
	if (git_repository_head_tree(&sm_head, sm_repo) < 0)
		giterr_clear();
	else {
		/* perform head to index diff on submodule */
		if (git_diff_tree_to_index(&diff, sm_repo, sm_head, index, &opt) < 0)
			giterr_clear();
		else {
			if (git_diff_num_deltas(diff) > 0)
				*status |= GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED;
			git_diff_free(diff);
			diff = NULL;
		}

		git_tree_free(sm_head);
	}

	/* perform index-to-workdir diff on submodule */
	if (git_diff_index_to_workdir(&diff, sm_repo, index, &opt) < 0)
		giterr_clear();
	else {
		size_t untracked =
			git_diff_num_deltas_of_type(diff, GIT_DELTA_UNTRACKED);

		if (untracked > 0)
			*status |= GIT_SUBMODULE_STATUS_WD_UNTRACKED;

		if (git_diff_num_deltas(diff) != untracked)
			*status |= GIT_SUBMODULE_STATUS_WD_WD_MODIFIED;

		git_diff_free(diff);
		diff = NULL;
	}
}
