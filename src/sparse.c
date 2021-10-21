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

static bool sparse_lookup_in_rules(
 int *checkout, git_attr_file *file, git_attr_path *path)
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

static int parse_sparse_file(git_repository *repo, git_attr_file *attrs, const char *data, bool allow_macros)
{
	int error = 0;
	int ignore_case = false;
	const char *scan = data;
	git_attr_fnmatch *match = NULL;
	GIT_UNUSED(allow_macros);
	
	if (git_repository__configmap_lookup(&ignore_case, repo, GIT_CONFIGMAP_IGNORECASE) < 0)
		git_error_clear();
	
	if (git_mutex_lock(&attrs->lock) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to lock sparse-checkout file");
		return -1;
	}
	
	while (!error && *scan) {
		int valid_rule = 1;
		
		if (!match && !(match = git__calloc(1, sizeof(*match)))) {
			error = -1;
			break;
		}
		
		match->flags =
		GIT_ATTR_FNMATCH_ALLOWSPACE | GIT_ATTR_FNMATCH_ALLOWNEG;
		
		if (!(error = git_attr_fnmatch__parse(
			match, &attrs->pool, NULL, &scan)))
		{
			match->flags |= GIT_ATTR_FNMATCH_IGNORE;
			
			if (ignore_case)
				match->flags |= GIT_ATTR_FNMATCH_ICASE;
			
			scan = git__next_line(scan);
			
			/*
			 * If a negative match doesn't actually do anything,
			 * throw it away. As we cannot always verify whether a
			 * rule containing wildcards negates another rule, we
			 * do not optimize away these rules, though.
			 * */
			if (match->flags & GIT_ATTR_FNMATCH_NEGATIVE
					&& !(match->flags & GIT_ATTR_FNMATCH_HASWILD))
				error = git_attr__does_negate_rule(&valid_rule, &attrs->rules, match);
			
			if (!error && valid_rule)
				error = git_vector_insert(&attrs->rules, match);
		}
		
		if (error != 0 || !valid_rule) {
			match->pattern = NULL;
			
			if (error == GIT_ENOTFOUND)
				error = 0;
		} else {
			match = NULL; /* vector now "owns" the match */
		}
	}
	
	git_mutex_unlock(&attrs->lock);
	git__free(match);
	
	return error;
}

int git_sparse_attr_file__init(
        git_repository *repo,
        git_sparse *sparse,
        git_str *infopath) {
    int error = 0;
    const char *filename = GIT_SPARSE_CHECKOUT_FILE;
    git_attr_file_source source = { GIT_ATTR_FILE_SOURCE_FILE, infopath->ptr, filename, NULL };
    git_str filepath = GIT_STR_INIT;
    bool fileExists;

    git_str_joinpath(&filepath, infopath->ptr, filename);

    /* Don't overwrite any existing sparse-checkout file */
    fileExists = git_path_exists(filepath.ptr);
    if (!fileExists) {
        if ((error = git_futils_creat_withpath(filepath.ptr, 0777, 0666)) < 0)
            return error;
    }

    error = git_attr_cache__get(&sparse->sparse, repo, NULL, &source, parse_sparse_file, false);

    return error;
}

int git_sparse__init(git_repository *repo, git_sparse *sparse)
{
	int error = 0;
	git_str infopath = GIT_STR_INIT;
	
	assert(repo && sparse);
	
	memset(sparse, 0, sizeof(*sparse));
	sparse->repo = repo;
	
	/* Read the ignore_case flag */
	if ((error = git_repository__configmap_lookup(
			&sparse->ignore_case, repo, GIT_CONFIGMAP_IGNORECASE)) < 0)
		goto cleanup;
	
	if ((error = git_attr_cache__init(repo)) < 0)
		goto cleanup;
	
	/* load .git/info/sparse_checkout if possible */
    if ((error = git_repository__item_path(&infopath, repo, GIT_REPOSITORY_ITEM_INFO)) < 0) {
        if (error != GIT_ENOTFOUND)
            goto cleanup;
        error = 0;
    }

    if ((error = git_sparse_attr_file__init(repo, sparse, &infopath)) < 0) {
        if (error != GIT_ENOTFOUND)
            goto cleanup;
        error = 0;
    }
	
cleanup:
	git_str_dispose(&infopath);
	if (error < 0)
		git_sparse__free(sparse);
	
	return error;
}

int git_sparse__lookup(int* checkout, git_sparse* sparse, const char* pathname, git_dir_flag dir_flag)
{
	git_attr_path path;
	const char *workdir;
	int error;
	
	assert(checkout && pathname && sparse);
	
	*checkout = GIT_SPARSE_CHECKOUT;
	
	workdir = git_repository_workdir(sparse->repo);
	if ((error = git_attr_path__init(&path, pathname, workdir, dir_flag)))
		return -1;
	
	/* No match -> no checkout */
	*checkout = GIT_SPARSE_NOCHECKOUT;
	
	while (1) {
		if (sparse_lookup_in_rules(checkout, sparse->sparse, &path))
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


int git_sparse_checkout_init(git_sparse_checkout_init_options *opts, git_repository *repo) {

    int error = 0;
    git_config *cfg;
    git_sparse sparse;

    GIT_ASSERT_ARG(opts);
    GIT_ASSERT_ARG(repo);

    if ((error = git_repository_config__weakptr(&cfg, repo)) < 0)
        return error;

    if ((error = git_config_set_bool(cfg, "core.sparseCheckout", true)) < 0)
        return error;

    if ((error = git_config_set_bool(cfg, "core.sparseCheckoutCone", opts->cone)))
        return error;

    if ((error = git_sparse__init(repo, &sparse)) < 0)
        goto cleanup;

cleanup:
    git_config_free(cfg);
    git_sparse__free(&sparse);
    return error;
}

int git_sparse_checkout__set(
        git_strarray *patterns,
        git_sparse *sparse)
{
    int error = 0;
    size_t i = 0;

    git_str filepath = GIT_STR_INIT;
    git_str content = GIT_STR_INIT;

    GIT_ASSERT_ARG(patterns);
    GIT_ASSERT_ARG(sparse);

    if ((error = git_str_joinpath(&filepath, sparse->sparse->source.base, sparse->sparse->source.filename)) < 0)
        goto done;


    for (; i < patterns->count; i++) {
        git_str_join(&content, '\n', git_str_cstr(&content), patterns->strings[i]);
    }

    if ((error = git_futils_truncate(filepath.ptr, 0777)) < 0)
        goto done;

    if ((error = git_futils_writebuffer(&content, filepath.ptr, O_WRONLY, 0644)) < 0)
        goto done;

done:
    git_str_dispose(&filepath);
    git_str_dispose(&content);

    return error;
}

int git_sparse_checkout_set(
        git_strarray *patterns,
        git_repository *repo)
{
    int err = 0;
    int is_enabled;
    git_config *cfg;
    git_sparse sparse;

    if ((err = git_repository_config__weakptr(&cfg, repo)) < 0)
        return err;

    err = git_config_get_bool(&is_enabled, cfg, "core.sparseCheckout");
    if (err < 0 && err != GIT_ENOTFOUND)
        return err;

    if (!is_enabled) {
        git_sparse_checkout_init_options opts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
        if ((err = git_sparse_checkout_init(&opts, repo) < 0))
            return err;
    }

    if ((err = git_sparse__init(repo, &sparse)) < 0)
        return err;

    if ((err = git_sparse_checkout__set(patterns, &sparse)) < 0)
        return err;

    return err;
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
	
	assert(repo && checkout && pathname);
	
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
