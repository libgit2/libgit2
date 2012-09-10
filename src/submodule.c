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
#include "submodule.h"
#include "tree.h"
#include "iterator.h"

#define GIT_MODULES_FILE ".gitmodules"

static git_cvar_map _sm_update_map[] = {
	{GIT_CVAR_STRING, "checkout", GIT_SUBMODULE_UPDATE_CHECKOUT},
	{GIT_CVAR_STRING, "rebase", GIT_SUBMODULE_UPDATE_REBASE},
	{GIT_CVAR_STRING, "merge", GIT_SUBMODULE_UPDATE_MERGE},
	{GIT_CVAR_STRING, "none", GIT_SUBMODULE_UPDATE_NONE},
};

static git_cvar_map _sm_ignore_map[] = {
	{GIT_CVAR_STRING, "none", GIT_SUBMODULE_IGNORE_NONE},
	{GIT_CVAR_STRING, "untracked", GIT_SUBMODULE_IGNORE_UNTRACKED},
	{GIT_CVAR_STRING, "dirty", GIT_SUBMODULE_IGNORE_DIRTY},
	{GIT_CVAR_STRING, "all", GIT_SUBMODULE_IGNORE_ALL},
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

static int load_submodule_config(git_repository *repo, bool force);
static git_config_file *open_gitmodules(git_repository *, bool, const git_oid *);
static int lookup_head_remote(git_buf *url, git_repository *repo);
static int submodule_get(git_submodule **, git_repository *, const char *, const char *);
static void submodule_release(git_submodule *sm, int decr);
static int submodule_load_from_index(git_repository *, const git_index_entry *);
static int submodule_load_from_head(git_repository*, const char*, const git_oid*);
static int submodule_load_from_config(const git_config_entry *, void *);
static int submodule_load_from_wd_lite(git_submodule *, const char *, void *);
static int submodule_update_config(git_submodule *, const char *, const char *, bool, bool);
static void submodule_mode_mismatch(git_repository *, const char *, unsigned int);
static int submodule_index_status(unsigned int *status, git_submodule *sm);
static int submodule_wd_status(unsigned int *status, git_submodule *sm);

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

	if ((error = load_submodule_config(repo, false)) < 0)
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
	seen._cmp = submodule_cmp;

	assert(repo && callback);

	if ((error = load_submodule_config(repo, false)) < 0)
		return error;

	git_strmap_foreach_value(repo->submodules, sm, {
		/* Usually the following will not come into play - it just prevents
		 * us from issuing a callback twice for a submodule where the name
		 * and path are not the same.
		 */
		if (sm->refcount > 1) {
			if (git_vector_bsearch(&seen, sm) != GIT_ENOTFOUND)
				continue;
			if ((error = git_vector_insert(&seen, sm)) < 0)
				break;
		}

		if (callback(sm, sm->name, payload)) {
			giterr_clear();
			error = GIT_EUSER;
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

	git_strmap_foreach_value(smcfg, sm, {
		submodule_release(sm,1);
	});
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
	git_config_file *mods = NULL;
	git_submodule *sm;
	git_buf name = GIT_BUF_INIT, real_url = GIT_BUF_INIT;
	git_repository_init_options initopt;
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

	if (url[0] == '.' && (url[1] == '/' || (url[1] == '.' && url[2] == '/'))) {
		if (!(error = lookup_head_remote(&real_url, repo)))
			error = git_path_apply_relative(&real_url, url);
	} else if (strchr(url, ':') != NULL || url[0] == '/') {
		error = git_buf_sets(&real_url, url);
	} else {
		giterr_set(GITERR_SUBMODULE, "Invalid format for submodule URL");
		error = -1;
	}
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

	memset(&initopt, 0, sizeof(initopt));
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

	if ((error = git_repository_index__weakptr(&index, sm->owner)) < 0 ||
		(error = git_index_add(index, GIT_MODULES_FILE, 0)) < 0)
		return error;

	return git_submodule_add_to_index(sm, true);
}

int git_submodule_add_to_index(git_submodule *sm, int write_index)
{
	int error;
	git_repository *repo, *sm_repo;
	git_index *index;
	git_buf path = GIT_BUF_INIT;
	git_commit *head;
	git_index_entry entry;
	struct stat st;

	assert(sm);

	repo = sm->owner;

	/* force reload of wd OID by git_submodule_open */
	sm->flags = sm->flags & ~GIT_SUBMODULE_STATUS__WD_OID_VALID;

	if ((error = git_repository_index__weakptr(&index, repo)) < 0 ||
		(error = git_buf_joinpath(
			&path, git_repository_workdir(repo), sm->path)) < 0 ||
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
	git_index__init_entry_from_stat(&st, &entry);

	/* calling git_submodule_open will have set sm->wd_oid if possible */
	if ((sm->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID) == 0) {
		giterr_set(GITERR_SUBMODULE,
			"Cannot add submodule without HEAD to index");
		error = -1;
		goto cleanup;
	}
	git_oid_cpy(&entry.oid, &sm->wd_oid);

	if ((error = git_commit_lookup(&head, sm_repo, &sm->wd_oid)) < 0)
		goto cleanup;

	entry.ctime.seconds = git_commit_time(head);
	entry.ctime.nanoseconds = 0;
	entry.mtime.seconds = git_commit_time(head);
	entry.mtime.nanoseconds = 0;

	git_commit_free(head);

	/* add it */
	error = git_index_add2(index, &entry);

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

int git_submodule_save(git_submodule *submodule)
{
	int error = 0;
	git_config_file *mods;
	git_buf key = GIT_BUF_INIT;

	assert(submodule);

	mods = open_gitmodules(submodule->owner, true, NULL);
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

	if (!(error = submodule_config_key_trunc_puts(&key, "update")) &&
		submodule->update != GIT_SUBMODULE_UPDATE_DEFAULT)
	{
		const char *val = (submodule->update == GIT_SUBMODULE_UPDATE_CHECKOUT) ?
			NULL : _sm_update_map[submodule->update].str_match;
		error = git_config_file_set_string(mods, key.ptr, val);
	}
	if (error < 0)
		goto cleanup;

	if (!(error = submodule_config_key_trunc_puts(&key, "ignore")) &&
		submodule->ignore != GIT_SUBMODULE_IGNORE_DEFAULT)
	{
		const char *val = (submodule->ignore == GIT_SUBMODULE_IGNORE_NONE) ?
			NULL : _sm_ignore_map[submodule->ignore].str_match;
		error = git_config_file_set_string(mods, key.ptr, val);
	}
	if (error < 0)
		goto cleanup;

	if ((error = submodule_config_key_trunc_puts(
			&key, "fetchRecurseSubmodules")) < 0 ||
		(error = git_config_file_set_string(
			mods, key.ptr, submodule->fetch_recurse ? "true" : "false")) < 0)
		goto cleanup;

	/* update internal defaults */

	submodule->ignore_default = submodule->ignore;
	submodule->update_default = submodule->update;
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
	return submodule->owner;
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

int git_submodule_set_url(git_submodule *submodule, const char *url)
{
	assert(submodule && url);

	git__free(submodule->url);

	submodule->url = git__strdup(url);
	GITERR_CHECK_ALLOC(submodule->url);

	return 0;
}

const git_oid *git_submodule_index_oid(git_submodule *submodule)
{
	assert(submodule);

	if (submodule->flags & GIT_SUBMODULE_STATUS__INDEX_OID_VALID)
		return &submodule->index_oid;
	else
		return NULL;
}

const git_oid *git_submodule_head_oid(git_submodule *submodule)
{
	assert(submodule);

	if (submodule->flags & GIT_SUBMODULE_STATUS__HEAD_OID_VALID)
		return &submodule->head_oid;
	else
		return NULL;
}

const git_oid *git_submodule_wd_oid(git_submodule *submodule)
{
	assert(submodule);

	if (!(submodule->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID)) {
		git_repository *subrepo;

		/* calling submodule open grabs the HEAD OID if possible */
		if (!git_submodule_open(&subrepo, submodule))
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
	return submodule->ignore;
}

git_submodule_ignore_t git_submodule_set_ignore(
	git_submodule *submodule, git_submodule_ignore_t ignore)
{
	git_submodule_ignore_t old;

	assert(submodule);

	if (ignore == GIT_SUBMODULE_IGNORE_DEFAULT)
		ignore = submodule->ignore_default;

	old = submodule->ignore;
	submodule->ignore = ignore;
	return old;
}

git_submodule_update_t git_submodule_update(git_submodule *submodule)
{
	assert(submodule);
	return submodule->update;
}

git_submodule_update_t git_submodule_set_update(
	git_submodule *submodule, git_submodule_update_t update)
{
	git_submodule_update_t old;

	assert(submodule);

	if (update == GIT_SUBMODULE_UPDATE_DEFAULT)
		update = submodule->update_default;

	old = submodule->update;
	submodule->update = update;
	return old;
}

int git_submodule_fetch_recurse_submodules(
	git_submodule *submodule)
{
	assert(submodule);
	return submodule->fetch_recurse;
}

int git_submodule_set_fetch_recurse_submodules(
	git_submodule *submodule,
	int fetch_recurse_submodules)
{
	int old;

	assert(submodule);

	old = submodule->fetch_recurse;
	submodule->fetch_recurse = (fetch_recurse_submodules != 0);
	return old;
}

int git_submodule_init(git_submodule *submodule, int overwrite)
{
	int error;

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

	if (submodule->update == GIT_SUBMODULE_UPDATE_CHECKOUT)
		error = submodule_update_config(
			submodule, "update", NULL, (overwrite != 0), false);
	else if (submodule->update != GIT_SUBMODULE_UPDATE_DEFAULT)
		error = submodule_update_config(
			submodule, "update",
			_sm_update_map[submodule->update].str_match,
			(overwrite != 0), false);

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

int git_submodule_open(
	git_repository **subrepo,
	git_submodule *submodule)
{
	int error;
	git_buf path = GIT_BUF_INIT;
	git_repository *repo;
	const char *workdir;

	assert(submodule && subrepo);

	repo = submodule->owner;
	workdir = git_repository_workdir(repo);

	if (!workdir) {
		giterr_set(GITERR_REPOSITORY,
			"Cannot open submodule repository in a bare repo");
		return GIT_ENOTFOUND;
	}

	if ((submodule->flags & GIT_SUBMODULE_STATUS_IN_WD) == 0) {
		giterr_set(GITERR_REPOSITORY,
			"Cannot open submodule repository that is not checked out");
		return GIT_ENOTFOUND;
	}

	if (git_buf_joinpath(&path, workdir, submodule->path) < 0)
		return -1;

	error = git_repository_open(subrepo, path.ptr);

	git_buf_free(&path);

	/* if we have opened the submodule successfully, let's grab the HEAD OID */
	if (!error && !(submodule->flags & GIT_SUBMODULE_STATUS__WD_OID_VALID)) {
		if (!git_reference_name_to_oid(
				&submodule->wd_oid, *subrepo, GIT_HEAD_FILE))
			submodule->flags |= GIT_SUBMODULE_STATUS__WD_OID_VALID;
		else
			giterr_clear();
	}

	return error;
}

int git_submodule_reload_all(git_repository *repo)
{
	assert(repo);
	return load_submodule_config(repo, true);
}

int git_submodule_reload(git_submodule *submodule)
{
	git_repository *repo;
	git_index *index;
	int pos, error;
	git_tree *head;
	git_config_file *mods;

	assert(submodule);

	/* refresh index data */

	repo = submodule->owner;
	if (git_repository_index__weakptr(&index, repo) < 0)
		return -1;

	submodule->flags = submodule->flags &
		~(GIT_SUBMODULE_STATUS_IN_INDEX |
		  GIT_SUBMODULE_STATUS__INDEX_OID_VALID);

	pos = git_index_find(index, submodule->path);
	if (pos >= 0) {
		git_index_entry *entry = git_index_get(index, pos);

		if (S_ISGITLINK(entry->mode)) {
			if ((error = submodule_load_from_index(repo, entry)) < 0)
				return error;
		} else {
			submodule_mode_mismatch(
				repo, entry->path, GIT_SUBMODULE_STATUS__INDEX_NOT_SUBMODULE);
		}
	}

	/* refresh HEAD tree data */

	if (!(error = git_repository_head_tree(&head, repo))) {
		git_tree_entry *te;

		submodule->flags = submodule->flags &
			~(GIT_SUBMODULE_STATUS_IN_HEAD |
			  GIT_SUBMODULE_STATUS__HEAD_OID_VALID);

		if (!(error = git_tree_entry_bypath(&te, head, submodule->path))) {

			if (S_ISGITLINK(te->attr)) {
				error = submodule_load_from_head(repo, submodule->path, &te->oid);
			} else {
				submodule_mode_mismatch(
					repo, submodule->path,
					GIT_SUBMODULE_STATUS__HEAD_NOT_SUBMODULE);
			}

			git_tree_entry_free(te);
		}
		else if (error == GIT_ENOTFOUND) {
			giterr_clear();
			error = 0;
		}

		git_tree_free(head);
	}

	if (error < 0)
		return error;

	/* refresh config data */

	if ((mods = open_gitmodules(repo, false, NULL)) != NULL) {
		git_buf path = GIT_BUF_INIT;

		git_buf_sets(&path, "submodule\\.");
		git_buf_puts_escape_regex(&path, submodule->name);
		git_buf_puts(&path, ".*");

		if (git_buf_oom(&path))
			error = -1;
		else
			error = git_config_file_foreach_match(
				mods, path.ptr, submodule_load_from_config, repo);

		git_buf_free(&path);
		git_config_file_free(mods);
	}

	if (error < 0)
		return error;

	/* refresh wd data */

	submodule->flags = submodule->flags &
		~(GIT_SUBMODULE_STATUS_IN_WD | GIT_SUBMODULE_STATUS__WD_OID_VALID);

	error = submodule_load_from_wd_lite(submodule, submodule->path, NULL);

	return error;
}

int git_submodule_status(
	unsigned int *status,
	git_submodule *submodule)
{
	int error = 0;
	unsigned int status_val;

	assert(status && submodule);

	status_val = GIT_SUBMODULE_STATUS__CLEAR_INTERNAL(submodule->flags);

	if (submodule->ignore != GIT_SUBMODULE_IGNORE_ALL) {
		if (!(error = submodule_index_status(&status_val, submodule)))
			error = submodule_wd_status(&status_val, submodule);
	}

	*status = status_val;

	return error;
}

/*
 * INTERNAL FUNCTIONS
 */

static git_submodule *submodule_alloc(git_repository *repo, const char *name)
{
	git_submodule *sm;

	if (!name || !strlen(name)) {
		giterr_set(GITERR_SUBMODULE, "Invalid submodule name");
		return NULL;
	}

	sm = git__calloc(1, sizeof(git_submodule));
	if (sm == NULL)
		goto fail;

	sm->path = sm->name = git__strdup(name);
	if (!sm->name)
		goto fail;

	sm->owner = repo;
	sm->refcount = 1;

	return sm;

fail:
	submodule_release(sm, 0);
	return NULL;
}

static void submodule_release(git_submodule *sm, int decr)
{
	if (!sm)
		return;

	sm->refcount -= decr;

	if (sm->refcount == 0) {
		if (sm->name != sm->path) {
			git__free(sm->path);
			sm->path = NULL;
		}

		git__free(sm->name);
		sm->name = NULL;

		git__free(sm->url);
		sm->url = NULL;

		sm->owner = NULL;

		git__free(sm);
	}
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

		/* insert value at name - if another thread beats us to it, then use
		 * their record and release our own.
		 */
		pos = kh_put(str, smcfg, sm->name, &error);

		if (error < 0) {
			submodule_release(sm, 1);
			sm = NULL;
		} else if (error == 0) {
			submodule_release(sm, 1);
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

static int submodule_load_from_index(
	git_repository *repo, const git_index_entry *entry)
{
	git_submodule *sm;

	if (submodule_get(&sm, repo, entry->path, NULL) < 0)
		return -1;

	if (sm->flags & GIT_SUBMODULE_STATUS_IN_INDEX) {
		sm->flags |= GIT_SUBMODULE_STATUS__INDEX_MULTIPLE_ENTRIES;
		return 0;
	}

	sm->flags |= GIT_SUBMODULE_STATUS_IN_INDEX;

	git_oid_cpy(&sm->index_oid, &entry->oid);
	sm->flags |= GIT_SUBMODULE_STATUS__INDEX_OID_VALID;

	return 0;
}

static int submodule_load_from_head(
	git_repository *repo, const char *path, const git_oid *oid)
{
	git_submodule *sm;

	if (submodule_get(&sm, repo, path, NULL) < 0)
		return -1;

	sm->flags |= GIT_SUBMODULE_STATUS_IN_HEAD;

	git_oid_cpy(&sm->head_oid, oid);
	sm->flags |= GIT_SUBMODULE_STATUS__HEAD_OID_VALID;

	return 0;
}

static int submodule_config_error(const char *property, const char *value)
{
	giterr_set(GITERR_INVALID,
		"Invalid value for submodule '%s' property: '%s'", property, value);
	return -1;
}

static int submodule_load_from_config(
	const git_config_entry *entry, void *data)
{
	git_repository *repo = data;
	git_strmap *smcfg = repo->submodules;
	const char *namestart, *property, *alternate = NULL;
	const char *key = entry->name, *value = entry->value;
	git_buf name = GIT_BUF_INIT;
	git_submodule *sm;
	bool is_path;
	int error = 0;

	if (git__prefixcmp(key, "submodule.") != 0)
		return 0;

	namestart = key + strlen("submodule.");
	property  = strrchr(namestart, '.');
	if (property == NULL)
		return 0;
	property++;
	is_path = (strcasecmp(property, "path") == 0);

	if (git_buf_set(&name, namestart, property - namestart - 1) < 0)
		return -1;

	if (submodule_get(&sm, repo, name.ptr, is_path ? value : NULL) < 0) {
		git_buf_free(&name);
		return -1;
	}

	sm->flags |= GIT_SUBMODULE_STATUS_IN_CONFIG;

	/* Only from config might we get differing names & paths.  If so, then
	 * update the submodule and insert under the alternative key.
	 */

	/* TODO: if case insensitive filesystem, then the following strcmps
	 * should be strcasecmp
	 */

	if (strcmp(sm->name, name.ptr) != 0) {
		alternate = sm->name = git_buf_detach(&name);
	} else if (is_path && value && strcmp(sm->path, value) != 0) {
		alternate = sm->path = git__strdup(value);
		if (!sm->path)
			error = -1;
	}
	if (alternate) {
		void *old_sm = NULL;
		git_strmap_insert2(smcfg, alternate, sm, old_sm, error);

		if (error >= 0)
			sm->refcount++; /* inserted under a new key */

		/* if we replaced an old module under this key, release the old one */
		if (old_sm && ((git_submodule *)old_sm) != sm) {
			submodule_release(old_sm, 1);
			/* TODO: log warning about multiple submodules with same path */
		}
	}

	git_buf_free(&name);
	if (error < 0)
		return error;

	/* TODO: Look up path in index and if it is present but not a GITLINK
	 * then this should be deleted (at least to match git's behavior)
	 */

	if (is_path)
		return 0;

	/* copy other properties into submodule entry */
	if (strcasecmp(property, "url") == 0) {
		git__free(sm->url);
		sm->url = NULL;

		if (value != NULL && (sm->url = git__strdup(value)) == NULL)
			return -1;
	}
	else if (strcasecmp(property, "update") == 0) {
		int val;
		if (git_config_lookup_map_value(
			&val, _sm_update_map, ARRAY_SIZE(_sm_update_map), value) < 0)
			return submodule_config_error("update", value);
		sm->update_default = sm->update = (git_submodule_update_t)val;
	}
	else if (strcasecmp(property, "fetchRecurseSubmodules") == 0) {
		if (git__parse_bool(&sm->fetch_recurse, value) < 0)
			return submodule_config_error("fetchRecurseSubmodules", value);
	}
	else if (strcasecmp(property, "ignore") == 0) {
		int val;
		if (git_config_lookup_map_value(
			&val, _sm_ignore_map, ARRAY_SIZE(_sm_ignore_map), value) < 0)
			return submodule_config_error("ignore", value);
		sm->ignore_default = sm->ignore = (git_submodule_ignore_t)val;
	}
	/* ignore other unknown submodule properties */

	return 0;
}

static int submodule_load_from_wd_lite(
	git_submodule *sm, const char *name, void *payload)
{
	git_repository *repo = git_submodule_owner(sm);
	git_buf path = GIT_BUF_INIT;

	GIT_UNUSED(name);
	GIT_UNUSED(payload);

	if (git_buf_joinpath(&path, git_repository_workdir(repo), sm->path) < 0)
		return -1;

	if (git_path_isdir(path.ptr))
		sm->flags |= GIT_SUBMODULE_STATUS__WD_SCANNED;

	if (git_path_contains(&path, DOT_GIT))
		sm->flags |= GIT_SUBMODULE_STATUS_IN_WD;

	git_buf_free(&path);

	return 0;
}

static void submodule_mode_mismatch(
	git_repository *repo, const char *path, unsigned int flag)
{
	khiter_t pos = git_strmap_lookup_index(repo->submodules, path);

	if (git_strmap_valid_index(repo->submodules, pos)) {
		git_submodule *sm = git_strmap_value_at(repo->submodules, pos);

		sm->flags |= flag;
	}
}

static int load_submodule_config_from_index(
	git_repository *repo, git_oid *gitmodules_oid)
{
	int error;
	git_iterator *i;
	const git_index_entry *entry;

	if ((error = git_iterator_for_index(&i, repo)) < 0)
		return error;

	error = git_iterator_current(i, &entry);

	while (!error && entry != NULL) {

		if (S_ISGITLINK(entry->mode)) {
			error = submodule_load_from_index(repo, entry);
			if (error < 0)
				break;
		} else {
			submodule_mode_mismatch(
				repo, entry->path, GIT_SUBMODULE_STATUS__INDEX_NOT_SUBMODULE);

			if (strcmp(entry->path, GIT_MODULES_FILE) == 0)
				git_oid_cpy(gitmodules_oid, &entry->oid);
		}

		error = git_iterator_advance(i, &entry);
	}

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

	if ((error = git_repository_head_tree(&head, repo)) < 0)
		return error;

	if ((error = git_iterator_for_tree(&i, repo, head)) < 0) {
		git_tree_free(head);
		return error;
	}

	error = git_iterator_current(i, &entry);

	while (!error && entry != NULL) {

		if (S_ISGITLINK(entry->mode)) {
			error = submodule_load_from_head(repo, entry->path, &entry->oid);
			if (error < 0)
				break;
		} else {
			submodule_mode_mismatch(
				repo, entry->path, GIT_SUBMODULE_STATUS__HEAD_NOT_SUBMODULE);

			if (strcmp(entry->path, GIT_MODULES_FILE) == 0 &&
				git_oid_iszero(gitmodules_oid))
				git_oid_cpy(gitmodules_oid, &entry->oid);
		}

		error = git_iterator_advance(i, &entry);
	}

	git_iterator_free(i);
	git_tree_free(head);

	return error;
}

static git_config_file *open_gitmodules(
	git_repository *repo,
	bool okay_to_create,
	const git_oid *gitmodules_oid)
{
	const char *workdir = git_repository_workdir(repo);
	git_buf path = GIT_BUF_INIT;
	git_config_file *mods = NULL;

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

static int load_submodule_config(git_repository *repo, bool force)
{
	int error;
	git_oid gitmodules_oid;
	git_buf path = GIT_BUF_INIT;
	git_config_file *mods = NULL;

	if (repo->submodules && !force)
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

	if ((mods = open_gitmodules(repo, false, &gitmodules_oid)) != NULL)
		error = git_config_file_foreach(mods, submodule_load_from_config, repo);

	if (error != 0)
		goto cleanup;

	/* shallow scan submodules in work tree */

	if (!git_repository_is_bare(repo))
		error = git_submodule_foreach(repo, submodule_load_from_wd_lite, NULL);

cleanup:
	git_buf_free(&path);

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

	if ((error = git_branch_tracking(&remote, head)) < 0)
		goto cleanup;

	/* remote should refer to something like refs/remotes/ORIGIN/BRANCH */

	if (git_reference_type(remote) != GIT_REF_SYMBOLIC ||
		git__prefixcmp(git_reference_target(remote), GIT_REFS_REMOTES_DIR) != 0)
	{
		giterr_set(GITERR_SUBMODULE,
			"Cannot resolve relative URL when HEAD is not symbolic");
		error = GIT_ENOTFOUND;
		goto cleanup;
	}

	scan = tgt = git_reference_target(remote) + strlen(GIT_REFS_REMOTES_DIR);
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
	const char *old = NULL;

	assert(submodule);

	error = git_repository_config__weakptr(&config, submodule->owner);
	if (error < 0)
		return error;

	error = git_buf_printf(&key, "submodule.%s.%s", submodule->name, attr);
	if (error < 0)
		goto cleanup;

	if (git_config_get_string(&old, config, key.ptr) < 0)
		giterr_clear();

	if (!old && only_existing)
		goto cleanup;
	if (old && !overwrite)
		goto cleanup;
	if ((!old && !value) || (old && value && strcmp(old, value) == 0))
		goto cleanup;

	if (!value)
		error = git_config_delete(config, key.ptr);
	else
		error = git_config_set_string(config, key.ptr, value);

cleanup:
	git_buf_free(&key);
	return error;
}

static int submodule_index_status(unsigned int *status, git_submodule *sm)
{
	const git_oid *head_oid  = git_submodule_head_oid(sm);
	const git_oid *index_oid = git_submodule_index_oid(sm);

	if (!head_oid) {
		if (index_oid)
			*status |= GIT_SUBMODULE_STATUS_INDEX_ADDED;
	}
	else if (!index_oid)
		*status |= GIT_SUBMODULE_STATUS_INDEX_DELETED;
	else if (!git_oid_equal(head_oid, index_oid))
		*status |= GIT_SUBMODULE_STATUS_INDEX_MODIFIED;

	return 0;
}

static int submodule_wd_status(unsigned int *status, git_submodule *sm)
{
	int error = 0;
	const git_oid *wd_oid, *index_oid;
	git_repository *sm_repo = NULL;

	/* open repo now if we need it (so wd_oid() call won't reopen) */
	if ((sm->ignore == GIT_SUBMODULE_IGNORE_NONE ||
		 sm->ignore == GIT_SUBMODULE_IGNORE_UNTRACKED) &&
		(sm->flags & GIT_SUBMODULE_STATUS_IN_WD) != 0)
	{
		if ((error = git_submodule_open(&sm_repo, sm)) < 0)
			return error;
	}

	index_oid = git_submodule_index_oid(sm);
	wd_oid    = git_submodule_wd_oid(sm);

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

	if (sm_repo != NULL) {
		git_tree *sm_head;
		git_diff_options opt;
		git_diff_list *diff;

		/* the diffs below could be optimized with an early termination
		 * option to the git_diff functions, but for now this is sufficient
		 * (and certainly no worse that what core git does).
		 */

		/* perform head-to-index diff on submodule */

		if ((error = git_repository_head_tree(&sm_head, sm_repo)) < 0)
			return error;

		memset(&opt, 0, sizeof(opt));
		if (sm->ignore == GIT_SUBMODULE_IGNORE_NONE)
			opt.flags |= GIT_DIFF_INCLUDE_UNTRACKED;

		error = git_diff_index_to_tree(sm_repo, &opt, sm_head, &diff);

		if (!error) {
			if (git_diff_num_deltas(diff) > 0)
				*status |= GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED;

			git_diff_list_free(diff);
			diff = NULL;
		}

		git_tree_free(sm_head);

		if (error < 0)
			return error;

		/* perform index-to-workdir diff on submodule */

		error = git_diff_workdir_to_index(sm_repo, &opt, &diff);

		if (!error) {
			size_t untracked =
				git_diff_num_deltas_of_type(diff, GIT_DELTA_UNTRACKED);

			if (untracked > 0)
				*status |= GIT_SUBMODULE_STATUS_WD_UNTRACKED;

			if ((git_diff_num_deltas(diff) - untracked) > 0)
				*status |= GIT_SUBMODULE_STATUS_WD_WD_MODIFIED;

			git_diff_list_free(diff);
			diff = NULL;
		}

		git_repository_free(sm_repo);
	}

	return error;
}
