/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/sparse.h"
#include "git2/checkout.h"
#include "git2/diff.h"
#include "git2/index.h"
#include "git2/tree.h"

#include "repository.h"
#include "config.h"
#include "filebuf.h"
#include "futils.h"
#include "path.h"
#include "vector.h"

static int validate_directory(git_repository *repo, const char *directory)
{
	const char *component, *slash;

	if (!directory || !*directory || directory[0] == '/' ||
	    directory[strlen(directory) - 1] == '/' ||
	    strchr(directory, '\\') || strstr(directory, "//")) {
		git_error_set(
		        GIT_ERROR_INVALID,
		        "sparse checkout directory must be a relative path");
		return GIT_EINVALID;
	}

	for (component = directory; component; component = slash + 1) {
		slash = strchr(component, '/');

		if ((slash && slash == component) ||
		    (!slash && component[0] == '\0') ||
		    ((slash ? (size_t)(slash - component) :
		              strlen(component)) == 1 &&
		     component[0] == '.') ||
		    ((slash ? (size_t)(slash - component) :
		              strlen(component)) == 2 &&
		     component[0] == '.' && component[1] == '.')) {
			git_error_set(
			        GIT_ERROR_INVALID,
			        "sparse checkout directory contains an invalid component");
			return GIT_EINVALID;
		}

		if (!slash)
			break;
	}

	if (!git_path_is_valid(
	            repo, directory, GIT_FILEMODE_TREE,
	            GIT_PATH_REJECT_WORKDIR_DEFAULTS)) {
		git_error_set(
		        GIT_ERROR_INVALID,
		        "invalid sparse checkout directory '%s'", directory);
		return GIT_EINVALID;
	}

	return 0;
}

static int
write_cone_patterns(git_filebuf *file, const git_strarray *directories)
{
	size_t i;
	int error;

	if ((error = git_filebuf_write(file, "/*\n!/*/\n", 8)) < 0)
		return error;

	for (i = 0; i < directories->count; i++) {
		const char *directory = directories->strings[i];
		const char *slash;
		size_t length;

		for (slash = directory;; slash++) {
			if (*slash != '/' && *slash != '\0')
				continue;

			length = (size_t)(slash - directory);

			if ((error = git_filebuf_printf(
			             file, "/%.*s/\n", (int)length,
			             directory)) < 0)
				return error;

			if (*slash == '\0')
				break;

			if ((error = git_filebuf_printf(
			             file, "!/%.*s/*/\n", (int)length,
			             directory)) < 0)
				return error;
		}
	}

	return 0;
}

static int sparse_checkout_path(git_str *path, git_repository *repo)
{
	if (git_repository__item_path(path, repo, GIT_REPOSITORY_ITEM_INFO) < 0)
		return -1;

	return git_str_joinpath(path, path->ptr, "sparse-checkout");
}

static bool sparse_checkout_pattern_is_parent(
        const char *pattern,
        size_t pattern_len,
        const char *next,
        const char *end)
{
	const char *next_end;
	size_t next_len;

	if (!next || next >= end)
		return false;

	next_end = memchr(next, '\n', (size_t)(end - next));
	if (!next_end)
		next_end = end;

	next_len = (size_t)(next_end - next);

	return next_len == pattern_len + 3 && next[0] == '!' &&
	       memcmp(next + 1, pattern, pattern_len - 1) == 0 &&
	       memcmp(next + pattern_len, "/*/", 3) == 0;
}

static bool
sparse_checkout_path_is_in_cone(const char *path, const git_str *patterns)
{
	const char *line = patterns->ptr;
	const char *end = patterns->ptr + patterns->size;

	if (!strchr(path, '/'))
		return true;

	while (line < end) {
		const char *line_end = memchr(line, '\n', (size_t)(end - line));
		const char *next;
		size_t line_len, directory_len;
		bool is_parent;

		if (!line_end)
			line_end = end;

		line_len = (size_t)(line_end - line);
		next = line_end == end ? NULL : line_end + 1;

		if (line_len < 3 || line[0] != '/' ||
		    line[line_len - 1] != '/' || line[1] == '*')
			goto next;

		directory_len = line_len - 2;

		if (strncmp(path, line + 1, directory_len) != 0 ||
		    path[directory_len] != '/')
			goto next;

		is_parent = sparse_checkout_pattern_is_parent(
		        line, line_len, next, end);

		if (!is_parent || !strchr(path + directory_len + 1, '/'))
			return true;

	next:
		line = line_end == end ? end : line_end + 1;
	}

	return false;
}

int git_sparse_checkout_set(
        git_repository *repo,
        const git_strarray *directories)
{
	git_config *config;
	git_filebuf file = GIT_FILEBUF_INIT;
	git_str path = GIT_STR_INIT;
	size_t i;
	int error = 0;
	bool file_open = false;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(directories);

	if (git_repository_is_bare(repo)) {
		git_error_set(
		        GIT_ERROR_REPOSITORY,
		        "cannot configure sparse checkout in a bare repository");
		return GIT_EBAREREPO;
	}

	if (directories->count && !directories->strings)
		return GIT_EINVALID;

	for (i = 0; i < directories->count; i++) {
		if ((error = validate_directory(
		             repo, directories->strings[i])) < 0)
			goto done;
	}

	if ((error = sparse_checkout_path(&path, repo)) < 0 ||
	    (error = git_filebuf_open(
	             &file, path.ptr, GIT_FILEBUF_CREATE_LEADING_DIRS, 0666)) <
	            0)
		goto done;

	file_open = true;

	if ((error = write_cone_patterns(&file, directories)) < 0 ||
	    (error = git_filebuf_commit(&file)) < 0)
		goto done;

	file_open = false;

	if ((error = git_repository_config__weakptr(&config, repo)) < 0 ||
	    (error = git_config_set_bool(config, "core.sparsecheckout", true)) <
	            0 ||
	    (error = git_config_set_bool(
	             config, "core.sparsecheckoutcone", true)) < 0)
		goto done;

done:
	if (file_open)
		git_filebuf_cleanup(&file);

	git_str_dispose(&path);
	return error;
}

int git_sparse_checkout_update_index(git_repository *repo)
{
	git_index *index = NULL;
	git_index_iterator *iterator = NULL;
	const git_index_entry *entry;
	git_str path = GIT_STR_INIT;
	git_str patterns = GIT_STR_INIT;
	int error;

	GIT_ASSERT_ARG(repo);

	if (git_repository_is_bare(repo)) {
		git_error_set(
		        GIT_ERROR_REPOSITORY,
		        "cannot update sparse checkout index in a bare repository");
		return GIT_EBAREREPO;
	}

	if ((error = sparse_checkout_path(&path, repo)) < 0 ||
	    (error = git_futils_readbuffer(&patterns, path.ptr)) < 0 ||
	    (error = git_repository_index(&index, repo)) < 0 ||
	    (error = git_index_iterator_new(&iterator, index)) < 0)
		goto done;

	while ((error = git_index_iterator_next(&entry, iterator)) == 0) {
		git_index_entry updated = *entry;
		bool included =
		        sparse_checkout_path_is_in_cone(entry->path, &patterns);

		if (GIT_INDEX_ENTRY_STAGE(entry) != 0)
			continue;

		if (included)
			updated.flags_extended &=
			        ~GIT_INDEX_ENTRY_SKIP_WORKTREE;
		else
			updated.flags_extended |= GIT_INDEX_ENTRY_SKIP_WORKTREE;

		if (updated.flags_extended != entry->flags_extended &&
		    (error = git_index_add(index, &updated)) < 0)
			goto done;
	}

	if (error == GIT_ITEROVER)
		error = git_index_write(index);

done:
	git_index_iterator_free(iterator);
	git_index_free(index);
	git_str_dispose(&patterns);
	git_str_dispose(&path);
	return error;
}

int git_sparse_checkout_initialize_index(git_repository *repo)
{
	git_index *index = NULL;
	git_tree *tree = NULL;
	int error;

	GIT_ASSERT_ARG(repo);

	if (git_repository_is_bare(repo)) {
		git_error_set(
		        GIT_ERROR_REPOSITORY,
		        "cannot initialize sparse checkout index in a bare repository");
		return GIT_EBAREREPO;
	}

	if ((error = git_repository_index(&index, repo)) < 0 ||
	    (error = git_repository_head_tree(&tree, repo)) < 0)
		goto done;

	if (git_index_entrycount(index) != 0) {
		git_error_set(
		        GIT_ERROR_INDEX,
		        "cannot initialize sparse checkout with a non-empty index");
		error = GIT_EUNCOMMITTED;
		goto done;
	}

	if ((error = git_index_read_tree(index, tree)) < 0 ||
	    (error = git_index_write(index)) < 0)
		goto done;

	error = git_sparse_checkout_update_index(repo);

done:
	git_tree_free(tree);
	git_index_free(index);
	return error;
}

static int
sparse_checkout_collect_paths(git_vector *paths, git_repository *repo)
{
	git_index *index = NULL;
	git_index_iterator *iterator = NULL;
	const git_index_entry *entry;
	int error;

	if ((error = git_repository_index(&index, repo)) < 0 ||
	    (error = git_index_iterator_new(&iterator, index)) < 0)
		goto done;

	while ((error = git_index_iterator_next(&entry, iterator)) == 0) {
		char *path;

		if (GIT_INDEX_ENTRY_STAGE(entry) != 0 ||
		    (entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE) !=
		            0)
			continue;

		if ((path = git__strdup(entry->path)) == NULL ||
		    (error = git_vector_insert(paths, path)) < 0) {
			git__free(path);
			goto done;
		}
	}

	if (error == GIT_ITEROVER)
		error = 0;

done:
	git_index_iterator_free(iterator);
	git_index_free(index);
	return error;
}

int git_sparse_checkout_checkout(
        git_repository *repo,
        const git_checkout_options *opts)
{
	git_checkout_options checkout_opts;
	git_index *index = NULL;
	git_tree *head = NULL;
	git_vector paths = GIT_VECTOR_INIT;
	int error;

	GIT_ASSERT_ARG(repo);
	GIT_ERROR_CHECK_VERSION(
	        opts, GIT_CHECKOUT_OPTIONS_VERSION, "git_checkout_options");

	if (opts)
		checkout_opts = *opts;
	else
		checkout_opts = (git_checkout_options)GIT_CHECKOUT_OPTIONS_INIT;

	if (git_repository_is_bare(repo)) {
		git_error_set(
		        GIT_ERROR_REPOSITORY,
		        "cannot checkout sparse paths in a bare repository");
		return GIT_EBAREREPO;
	}

	if ((error = git_repository_index(&index, repo)) < 0 ||
	    (error = git_repository_head_tree(&head, repo)) < 0)
		goto done;

	if (git_index_entrycount(index) == 0) {
		git_error_set(
		        GIT_ERROR_INDEX,
		        "sparse checkout index has not been initialized");
		error = GIT_EUNCOMMITTED;
		goto done;
	}

	if ((error = git_vector_init(
	             &paths, git_index_entrycount(index), NULL)) < 0 ||
	    (error = sparse_checkout_collect_paths(&paths, repo)) < 0 ||
	    (error = git_repository_head_tree(&head, repo)) < 0)
		goto done;

	checkout_opts.checkout_strategy |= GIT_CHECKOUT_RECREATE_MISSING;
	checkout_opts.paths.strings = (char **)paths.contents;
	checkout_opts.paths.count = paths.length;

	error = git_checkout_tree(repo, (git_object *)head, &checkout_opts);

done:
	git_vector_dispose_deep(&paths);
	git_tree_free(head);
	git_index_free(index);
	return error;
}

static int sparse_checkout_remove_excluded(git_repository *repo)
{
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
	git_diff *diff = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	const char *workdir;
	size_t i;
	int error;

	if ((error = git_repository_head_tree(&tree, repo)) < 0 ||
	    (error = git_repository_index(&index, repo)) < 0)
		goto done;

	diff_opts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;
	if ((error = git_diff_tree_to_workdir(&diff, repo, tree, &diff_opts)) <
	    0)
		goto done;

	workdir = git_repository_workdir(repo);
	for (i = 0; i < git_diff_num_deltas(diff); i++) {
		const git_diff_delta *delta = git_diff_get_delta(diff, i);
		const git_index_entry *entry;

		if (delta->status != GIT_DELTA_UNMODIFIED)
			continue;

		entry = git_index_get_bypath(index, delta->old_file.path, 0);
		if (!entry || (entry->flags_extended &
		               GIT_INDEX_ENTRY_SKIP_WORKTREE) == 0)
			continue;

		if ((error = git_futils_rmdir_r(
		             delta->old_file.path, workdir,
		             GIT_RMDIR_EMPTY_PARENTS |
		                     GIT_RMDIR_REMOVE_FILES)) < 0)
			goto done;
	}

done:
	git_diff_free(diff);
	git_index_free(index);
	git_tree_free(tree);
	return error;
}

static int
sparse_checkout_index_is_clean(git_repository *repo, git_index *index)
{
	git_diff *diff = NULL;
	git_tree *tree = NULL;
	int error;

	if ((error = git_repository_head_tree(&tree, repo)) < 0 ||
	    (error = git_diff_tree_to_index(&diff, repo, tree, index, NULL)) < 0)
		goto done;

	if (git_diff_num_deltas(diff) > 0) {
		git_error_set(
		        GIT_ERROR_INDEX,
		        "cannot apply sparse checkout with staged changes");
		error = GIT_EUNCOMMITTED;
	}

done:
	git_diff_free(diff);
	git_tree_free(tree);
	return error;
}

int git_sparse_checkout_apply(
        git_repository *repo,
        const git_strarray *directories,
        const git_checkout_options *opts)
{
	git_index *index = NULL;
	bool initialize_index;
	int error;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(directories);
	GIT_ERROR_CHECK_VERSION(
	        opts, GIT_CHECKOUT_OPTIONS_VERSION, "git_checkout_options");

	if ((error = git_repository_index(&index, repo)) < 0)
		goto done;

	if (git_index_has_conflicts(index)) {
		git_error_set(
		        GIT_ERROR_INDEX,
		        "cannot apply sparse checkout with unresolved conflicts");
		error = GIT_ECONFLICT;
		goto done;
	}

	initialize_index = git_index_entrycount(index) == 0;
	if (!initialize_index &&
	    (error = sparse_checkout_index_is_clean(repo, index)) < 0)
		goto done;

	git_index_free(index);
	index = NULL;

	if ((error = git_sparse_checkout_set(repo, directories)) < 0)
		goto done;

	if (initialize_index)
		error = git_sparse_checkout_initialize_index(repo);
	else
		error = git_sparse_checkout_update_index(repo);

	if (error < 0)
		goto done;

	if (!initialize_index &&
	    (error = sparse_checkout_remove_excluded(repo)) < 0)
		goto done;

	error = git_sparse_checkout_checkout(repo, opts);

done:
	git_index_free(index);
	return error;
}

int git_sparse_checkout_disable(
        git_repository *repo,
        const git_checkout_options *opts)
{
	git_config *config;
	git_index *index = NULL;
	size_t i;
	int error;

	GIT_ASSERT_ARG(repo);
	GIT_ERROR_CHECK_VERSION(
	        opts, GIT_CHECKOUT_OPTIONS_VERSION, "git_checkout_options");

	if ((error = git_repository_config__weakptr(&config, repo)) < 0 ||
	    (error = git_config_set_bool(
	             config, "core.sparsecheckout", false)) < 0 ||
	    (error = git_config_set_bool(
	             config, "core.sparsecheckoutcone", false)) < 0 ||
	    (error = git_repository_index(&index, repo)) < 0)
		goto done;

	for (i = 0; i < git_index_entrycount(index); i++) {
		const git_index_entry *entry = git_index_get_byindex(index, i);
		git_index_entry updated = *entry;

		if (GIT_INDEX_ENTRY_STAGE(entry) != 0 ||
		    (updated.flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE) ==
		            0)
			continue;

		updated.flags_extended &= ~GIT_INDEX_ENTRY_SKIP_WORKTREE;
		if ((error = git_index_add(index, &updated)) < 0)
			goto done;
	}

	if ((error = git_index_write(index)) < 0)
		goto done;

	error = git_sparse_checkout_checkout(repo, opts);

done:
	git_index_free(index);
	return error;
}

int git_sparse_checkout_list(git_strarray *out, git_repository *repo)
{
	git_str path = GIT_STR_INIT, patterns = GIT_STR_INIT;
	git_vector directories = GIT_VECTOR_INIT;
	const char *line, *end;
	int error = 0;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(repo);
	memset(out, 0, sizeof(*out));

	if ((error = sparse_checkout_path(&path, repo)) < 0 ||
	    (error = git_futils_readbuffer(&patterns, path.ptr)) < 0 ||
	    (error = git_vector_init(&directories, 0, NULL)) < 0)
		goto done;

	line = patterns.ptr;
	end = patterns.ptr + patterns.size;
	while (line < end) {
		const char *line_end = memchr(line, '\n', (size_t)(end - line));
		const char *next;
		size_t line_len;
		char *directory;

		if (!line_end)
			line_end = end;
		line_len = (size_t)(line_end - line);
		next = line_end == end ? NULL : line_end + 1;

		if (line_len >= 3 && line[0] == '/' &&
		    line[line_len - 1] == '/' && line[1] != '*' &&
		    !sparse_checkout_pattern_is_parent(
		            line, line_len, next, end)) {
			if ((directory = git__strndup(
			             line + 1, line_len - 2)) == NULL ||
			    (error = git_vector_insert(
			             &directories, directory)) < 0) {
				git__free(directory);
				goto done;
			}
		}

		line = line_end == end ? end : line_end + 1;
	}

	out->strings =
	        (char **)git_vector_detach(&out->count, NULL, &directories);

done:
	git_vector_dispose_deep(&directories);
	git_str_dispose(&patterns);
	git_str_dispose(&path);
	return error;
}

int git_sparse_checkout_is_enabled(int *out, git_repository *repo)
{
	git_config *config;
	int error;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(repo);
	*out = 0;

	if ((error = git_repository_config__weakptr(&config, repo)) < 0)
		return error;

	error = git_config_get_bool(out, config, "core.sparsecheckout");
	if (error == GIT_ENOTFOUND) {
		git_error_clear();
		return 0;
	}

	return error;
}
