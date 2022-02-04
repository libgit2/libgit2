/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "sparse.h"
#include "attrcache.h"
#include "git2/sparse.h"
#include "config.h"
#include "filebuf.h"
#include "index.h"
#include "ignore.h"

static bool sparse_lookup_in_rules(
        int *checkout,
        git_attr_file *file,
        git_attr_path *path)
{
	size_t j;
	git_attr_fnmatch *match;
	
	git_vector_rforeach(&file->rules, j, match) {
		if (match->flags & GIT_ATTR_FNMATCH_DIRECTORY &&
				path->is_dir == GIT_DIR_FLAG_FALSE)
			continue;
		if (git_attr_fnmatch__match(match, path)) {
			*checkout = ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0) ?
			GIT_SPARSE_CHECKOUT : GIT_SPARSE_NOCHECKOUT;
			return true;
		}
	}
	
	return false;
}

static int parse_sparse_file(
        git_repository *repo,
        git_attr_file *attrs,
        const char *data,
        bool allow_macros)
{
	/* Todo: Support for cone mode */
	return parse_ignore_file(
			repo,
			attrs,
			data,
			NULL,
			allow_macros);
}

int git_sparse_attr_file__init_(
		int *file_exists,
        git_repository *repo,
        git_sparse *sparse)
{
    int error = 0;
	git_str infopath = GIT_STR_INIT;
    const char *filename = GIT_SPARSE_CHECKOUT_FILE;
	git_attr_file_source source = { GIT_ATTR_FILE_SOURCE_FILE, git_str_cstr(&infopath), filename, NULL };
	git_str filepath = GIT_STR_INIT;

	if ((error = git_str_joinpath(&infopath, repo->gitdir, "info")) < 0) {
		if (error != GIT_ENOTFOUND)
			goto done;
		error = 0;
	}

	source.base = git_str_cstr(&infopath);
	source.filename = filename;

	git_str_joinpath(&filepath, infopath.ptr, filename);

    /* Don't overwrite any existing sparse-checkout file */
	*file_exists = git_fs_path_exists(git_str_cstr(&filepath));
    if (!*file_exists) {
        if ((error = git_futils_creat_withpath(git_str_cstr(&filepath), 0777, 0666)) < 0)
			goto done;
    }

    error = git_attr_cache__get(&sparse->sparse, repo, NULL, &source, parse_sparse_file, false);

done:
	git_str_dispose(&infopath);
    return error;
}

int git_sparse_attr_file__init(
		git_repository *repo,
		git_sparse *sparse)
{
	int b = false;
	int error = git_sparse_attr_file__init_(&b, repo, sparse);
	return error;
}

int git_sparse__init_(
		int *file_exists,
        git_repository *repo,
        git_sparse *sparse)
{
	int error = 0;
	
	assert(repo && sparse);
	
	memset(sparse, 0, sizeof(*sparse));
	sparse->repo = repo;
	
	/* Read the ignore_case flag */
	if ((error = git_repository__configmap_lookup(
			&sparse->ignore_case, repo, GIT_CONFIGMAP_IGNORECASE)) < 0)
		goto cleanup;
	
	if ((error = git_attr_cache__init(repo)) < 0)
		goto cleanup;

    if ((error = git_sparse_attr_file__init_(file_exists, repo, sparse)) < 0) {
        if (error != GIT_ENOTFOUND)
            goto cleanup;
        error = 0;
    }
	
cleanup:
	if (error < 0)
		git_sparse__free(sparse);
	
	return error;
}

int git_sparse__init(
		git_repository *repo,
		git_sparse *sparse)
{
	int b = false;
	int error = git_sparse__init_(&b, repo, sparse);
	return error;
}

int git_sparse__lookup(
        git_sparse_status* status,
        git_sparse *sparse,
        const char* pathname,
        git_dir_flag dir_flag)
{
	git_attr_path path;
	const char *workdir;
	int error;

	GIT_ASSERT_ARG(status);
	GIT_ASSERT_ARG(sparse);
	GIT_ASSERT_ARG(pathname);

	*status = GIT_SPARSE_CHECKOUT;

	workdir = git_repository_workdir(sparse->repo);
	if ((error = git_attr_path__init(&path, pathname, workdir, dir_flag)))
		return -1;
	
	/* No match -> no checkout */
	*status = GIT_SPARSE_NOCHECKOUT;
	
	while (1) {
		if (sparse_lookup_in_rules(status, sparse->sparse, &path))
			goto cleanup;
		
		/* move up one directory */
		if (path.basename == path.path)
			break;
		path.basename[-1] = '\0';
		while (path.basename > path.path && *path.basename != '/')
			path.basename--;
		if (path.basename > path.path)
			path.basename++;
		path.is_dir = 1;
	}

cleanup:
	git_attr_path__free(&path);
	return 0;
}

void git_sparse__free(git_sparse *sparse)
{
	git_attr_file__free(sparse->sparse);
}

int git_sparse_checkout__list(
        git_vector *patterns,
        git_sparse *sparse)
{
    int error = 0;
    git_str data = GIT_STR_INIT;
    char *scan, *buf;

    GIT_ASSERT_ARG(patterns);
    GIT_ASSERT_ARG(sparse);

    if ((error = git_futils_readbuffer(&data, sparse->sparse->entry->fullpath)) < 0)
        return error;

    scan = (char *)git_str_cstr(&data);
    while (!error && *scan) {

		buf = git__strtok(&scan, "\r\n");
		if (buf)
			error = git_vector_insert(patterns, buf);
    }

    return error;
}

int git_sparse_checkout_list(git_strarray *patterns, git_repository *repo) {

    int error = 0;
    git_sparse sparse;
    git_vector patternlist;

    GIT_ASSERT_ARG(patterns);
    GIT_ASSERT_ARG(repo);

    if ((error = git_sparse__init(repo, &sparse)))
        goto done;

    if ((error = git_vector_init(&patternlist, 0, NULL)) < 0)
        goto done;

    if ((error = git_sparse_checkout__list(&patternlist, &sparse)))
        goto done;

    patterns->strings = (char **) git_vector_detach(&patterns->count, NULL, &patternlist);

done:
    git_sparse__free(&sparse);
    git_vector_free(&patternlist);


    return error;
}

int git_sparse_checkout__reapply(git_repository *repo, git_sparse *sparse)
{
	int error = 0;
	git_index *index;
	size_t i = 0;
	git_index_entry *entry;
	git_vector paths_to_checkout;
	git_checkout_options copts;
	const char *workdir = repo->workdir;

	if ((error = git_repository_index(&index, repo)) < 0)
		goto done;

	if ((error = git_vector_init(&paths_to_checkout, 0, NULL)) < 0)
		goto done;

	git_vector_foreach(&index->entries, i, entry)
	{
		int is_submodule = false;
		int has_conflict = false;
		unsigned int status_flags;
		int checkout = GIT_SPARSE_CHECKOUT;
		git_str fullpath = GIT_STR_INIT;

		/* Don't touch submodules */
		is_submodule = S_ISGITLINK(entry->mode);
		if (is_submodule)
			continue;

		/* Don't touch files with conflicts */
		has_conflict = GIT_INDEX_ENTRY_STAGE(entry) > 0;
		if (has_conflict)
			continue;

		/* Don't touch files that aren't current */
		if ((error = git_status_file(&status_flags, repo, entry->path)) < 0)
			goto done;
		if (status_flags != GIT_STATUS_CURRENT)
			continue;

		if ((error = git_str_joinpath(&fullpath, repo->workdir, entry->path)) < 0)
			goto done;

		if (git_sparse__lookup(&checkout, sparse, entry->path, GIT_DIR_FLAG_FALSE) == 0 &&
			checkout == GIT_SPARSE_NOCHECKOUT)
		{
			entry->flags_extended |= GIT_INDEX_ENTRY_SKIP_WORKTREE;

			if (!git_fs_path_exists(git_str_cstr(&fullpath)))
				continue;

			if ((error = git_futils_rmdir_r(entry->path, workdir, GIT_RMDIR_REMOVE_FILES | GIT_RMDIR_EMPTY_PARENTS)) < 0)
				goto done;
		}
		else
		{
			entry->flags_extended &= ~GIT_INDEX_ENTRY_SKIP_WORKTREE;
			git_vector_insert(&paths_to_checkout, (void*) entry->path);
		}
	}

	if ((error = git_checkout_options_init(&copts, GIT_CHECKOUT_OPTIONS_VERSION)) < 0)
		goto done;

	copts.paths.strings = (char**) git_vector_detach(&copts.paths.count, NULL, &paths_to_checkout);

	copts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
	if ((error = git_checkout_index(repo, index, &copts)) < 0)
		goto done;

	error = git_index_write(index);

done:
	git_index_free(index);
	git_vector_free(&paths_to_checkout);

	return error;
}

int git_sparse_checkout__set(
		git_vector *patterns,
		git_repository *repo,
		git_sparse *sparse)
{
	int error;
	size_t i;
	const char *pattern;
	git_str content = GIT_STR_INIT;

	git_vector_foreach(patterns, i, pattern) {
		git_str_join(&content, '\n', git_str_cstr(&content), pattern);
	}

	if ((error = git_futils_truncate(sparse->sparse->entry->fullpath, 0777)) < 0)
		goto done;

	if ((error = git_futils_writebuffer(&content, sparse->sparse->entry->fullpath, O_WRONLY, 0644)) < 0)
		goto done;

	/* Refresh the rules in the sparse info */
	git_vector_clear(&sparse->sparse->rules);
	if ((error = git_sparse_attr_file__init(repo, sparse)) < 0)
		goto done;

done:
	git_str_dispose(&content);

	return error;
}

int git_sparse_checkout__enable(git_repository *repo, git_sparse_checkout_init_options *opts)
{
	int error = 0;
	git_config *cfg;

	/* Can be used once cone mode is supported */
	GIT_UNUSED(opts);

	if ((error = git_repository_config__weakptr(&cfg, repo)) < 0)
		return error;

	if ((error = git_config_set_bool(cfg, "core.sparseCheckout", true)) < 0)
		goto done;

done:
	git_config_free(cfg);
	return error;
}

int git_sparse_checkout_init(git_repository *repo, git_sparse_checkout_init_options *opts)
{
    int error = 0;
    git_sparse sparse;
	int file_exists = false;
	git_vector default_patterns = GIT_VECTOR_INIT;

    GIT_ASSERT_ARG(repo);
    GIT_ASSERT_ARG(opts);

    if ((error = git_sparse_checkout__enable(repo, opts)) < 0)
		return error;

    if ((error = git_sparse__init_(&file_exists, repo, &sparse)) < 0)
        goto cleanup;

	if (!file_exists) {

		/* Default patterns that match every file in the root directory and no other directories */
		git_vector_insert(&default_patterns, "/*");
		git_vector_insert(&default_patterns, "!/*/");

		if ((error = git_sparse_checkout__set(&default_patterns, repo, &sparse)) < 0)
			goto cleanup;
	}

	if ((error = git_sparse_checkout__reapply(repo, &sparse)) < 0)
		goto cleanup;

cleanup:
    git_sparse__free(&sparse);
    return error;
}

int git_sparse_checkout_set(
        git_repository *repo,
        git_strarray *patterns)
{
    int error;
    git_config *cfg;
    git_sparse sparse;

    size_t i = 0;
    git_vector patternlist;

	git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(patterns);

    if ((error = git_repository_config(&cfg, repo)) < 0)
        goto done;

	if ((error = git_sparse_checkout__enable(repo, &opts) < 0))
		goto done;

    if ((error = git_sparse__init(repo, &sparse)) < 0)
        goto done;

    if ((error = git_vector_init(&patternlist, 0, NULL)) < 0)
        goto done;

    for (i = 0; i < patterns->count; i++) {
        git_vector_insert(&patternlist, patterns->strings[i]);
    }

    if ((error = git_sparse_checkout__set(&patternlist, repo, &sparse)) < 0)
        goto done;

	if ((error = git_sparse_checkout__reapply(repo, &sparse)) < 0)
		goto done;

done:
    git_config_free(cfg);
    git_sparse__free(&sparse);
    git_vector_free(&patternlist);

    return error;
}

int git_sparse_checkout__restore_wd(git_repository *repo)
{
	int error = 0;
	git_sparse sparse;
	git_vector old_patterns, patterns = GIT_VECTOR_INIT;

	if ((error = git_sparse__init(repo, &sparse)) < 0)
		return error;

	/* Store the old patterns so that we can put them back later */
	if ((error = git_vector_init(&old_patterns, 0 ,NULL)) < 0)
		goto done;

	if ((error = git_sparse_checkout__list(&old_patterns, &sparse)) < 0)
		goto done;

	/* Write down a pattern that will include everything */
	if ((error = git_vector_init(&patterns, 0 ,NULL)) < 0)
		goto done;

	if ((error = git_vector_insert(&patterns, "/*")) < 0)
		goto done;

	if ((error = git_sparse_checkout__set(&patterns, repo, &sparse)) < 0)
		goto done;

	/* Re-apply sparsity with our catch-all pattern */
	if ((error = git_sparse_checkout__reapply(repo, &sparse)) < 0)
		goto done;

	/* Restore the sparse-checkout patterns to how they were before */
	if ((error = git_sparse_checkout__set(&old_patterns, repo, &sparse)) < 0)
		goto done;

done:
	git_sparse__free(&sparse);
	git_vector_free(&old_patterns);
	git_vector_free(&patterns);
	return error;
}

int git_sparse_checkout_disable(git_repository *repo)
{
    int error = 0;
    git_config *cfg;

    GIT_ASSERT_ARG(repo);

    if ((error = git_repository_config(&cfg, repo)) < 0)
        return error;

    if ((error = git_config_set_bool(cfg, "core.sparseCheckout", false)) < 0)
        goto done;

	if ((error = git_sparse_checkout__restore_wd(repo)) < 0)
		goto done;

done:
    git_config_free(cfg);

    return error;
}

int git_sparse_checkout__add(
		git_repository *repo,
        git_vector *patterns,
        git_sparse *sparse)
{
    int error = 0;
    size_t i = 0;
    git_vector existing_patterns;
    git_vector new_patterns;
    char* pattern;

    if ((error = git_vector_init(&existing_patterns, 0, NULL)) < 0)
        goto done;

    if ((error = git_vector_init(&new_patterns, 0, NULL)) < 0)
        goto done;

    if ((error = git_sparse_checkout__list(&existing_patterns, sparse)) < 0)
        goto done;

    git_vector_foreach(&existing_patterns, i, pattern) {
        git_vector_insert(&new_patterns, pattern);
    }

    git_vector_foreach(patterns, i, pattern) {
        git_vector_insert(&new_patterns, pattern);
    }

    if ((error = git_sparse_checkout__set(&new_patterns, repo, sparse)) < 0)
        goto done;

done:
    git_vector_free(&existing_patterns);
    git_vector_free(&new_patterns);

    return error;
}

int git_sparse_checkout_add(
        git_repository *repo,
        git_strarray *patterns)
{
    int error;
    int is_enabled = false;
    git_config *cfg;
    git_sparse sparse;
    git_vector patternlist;
	size_t i;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(patterns);

    if ((error = git_repository_config__weakptr(&cfg, repo)) < 0)
        return error;

	error = git_config_get_bool(&is_enabled, cfg, "core.sparseCheckout");
    if (error < 0 && error != GIT_ENOTFOUND)
        goto done;

    if (!is_enabled)
	{
		git_error_set(GIT_ERROR_INVALID, "sparse checkout is not enabled");
		git_config_free(cfg);
		return -1;
	}

    if ((error = git_sparse__init(repo, &sparse)) < 0)
        goto done;

    if ((error = git_vector_init(&patternlist, 0, NULL)))
        goto done;

	for (i = 0; i < patterns->count; i++) {
		if ((error = git_vector_insert(&patternlist, patterns->strings[i])) < 0)
			return error;
	}

    if ((error = git_sparse_checkout__add(repo, &patternlist,  &sparse)) < 0)
        goto done;

	if ((error = git_sparse_checkout__reapply(repo, &sparse)) < 0)
		goto done;

done:
    git_config_free(cfg);
	git_sparse__free(&sparse);
	git_vector_free(&patternlist);

    return error;
}

int git_sparse_checkout_reapply(git_repository *repo) {
	int error;
	git_sparse sparse;

	GIT_ASSERT_ARG(repo);

	if ((error = git_sparse__init(repo, &sparse)) < 0)
		return error;

	if ((error = git_sparse_checkout__reapply(repo, &sparse)) < 0)
		goto done;

done:
	git_sparse__free(&sparse);
	return error;
}

int git_sparse_check_path(
        int *checkout,
        git_repository *repo,
        const char *pathname)
{
    int error;
    int sparse_checkout_enabled = false;
    git_sparse sparse;
    git_dir_flag dir_flag = GIT_DIR_FLAG_FALSE;

    GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(checkout);
	GIT_ASSERT_ARG(pathname);

    *checkout = GIT_SPARSE_CHECKOUT;

    if ((error = git_repository__configmap_lookup(&sparse_checkout_enabled, repo, GIT_CONFIGMAP_SPARSECHECKOUT)) < 0 ||
        sparse_checkout_enabled == false)
        return 0;

    if ((error = git_sparse__init(repo, &sparse)) < 0)
        goto cleanup;

    if (!git__suffixcmp(pathname, "/"))
        dir_flag = GIT_DIR_FLAG_TRUE;
    else if (git_repository_is_bare(repo))
        dir_flag = GIT_DIR_FLAG_FALSE;

    error = git_sparse__lookup(checkout, &sparse, pathname, dir_flag);

    cleanup:
    git_sparse__free(&sparse);
    return error;
}
