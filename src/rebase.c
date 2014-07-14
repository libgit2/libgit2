/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "buffer.h"
#include "repository.h"
#include "posix.h"
#include "filebuf.h"
#include "merge.h"
#include "array.h"

#include <git2/types.h>
#include <git2/rebase.h>
#include <git2/commit.h>
#include <git2/reset.h>
#include <git2/revwalk.h>

#define REBASE_APPLY_DIR	"rebase-apply"
#define REBASE_MERGE_DIR	"rebase-merge"

#define HEAD_NAME_FILE		"head-name"
#define ORIG_HEAD_FILE		"orig-head"
#define HEAD_FILE			"head"
#define ONTO_FILE			"onto"
#define ONTO_NAME_FILE		"onto_name"
#define QUIET_FILE			"quiet"

#define MSGNUM_FILE			"msgnum"
#define END_FILE			"end"
#define CMT_FILE_FMT		"cmt.%d"

#define ORIG_DETACHED_HEAD	"detached HEAD"

#define REBASE_DIR_MODE		0777
#define REBASE_FILE_MODE	0666

typedef enum {
	GIT_REBASE_TYPE_NONE = 0,
	GIT_REBASE_TYPE_APPLY = 1,
	GIT_REBASE_TYPE_MERGE = 2,
} git_rebase_type_t;

static int rebase_state_type(
	git_rebase_type_t *type_out,
	char **path_out,
	git_repository *repo)
{
	git_buf path = GIT_BUF_INIT;
	git_rebase_type_t type = GIT_REBASE_TYPE_NONE;

	if (git_buf_joinpath(&path, repo->path_repository, REBASE_APPLY_DIR) < 0)
		return -1;

	if (git_path_isdir(git_buf_cstr(&path))) {
		type = GIT_REBASE_TYPE_APPLY;
		goto done;
	}

	git_buf_clear(&path);
	if (git_buf_joinpath(&path, repo->path_repository, REBASE_MERGE_DIR) < 0)
		return -1;

	if (git_path_isdir(git_buf_cstr(&path))) {
		type = GIT_REBASE_TYPE_MERGE;
		goto done;
	}

done:
	*type_out = type;

	if (type != GIT_REBASE_TYPE_NONE && path_out)
		*path_out = git_buf_detach(&path);

	git_buf_free(&path);

	return 0;
}

static int rebase_setupfile(git_repository *repo, const char *filename, const char *fmt, ...)
{
	git_buf path = GIT_BUF_INIT,
		contents = GIT_BUF_INIT;
	va_list ap;
	int error;

	va_start(ap, fmt);
	git_buf_vprintf(&contents, fmt, ap);
	va_end(ap);

	if ((error = git_buf_joinpath(&path, repo->path_repository, REBASE_MERGE_DIR)) == 0 &&
		(error = git_buf_joinpath(&path, path.ptr, filename)) == 0)
		error = git_futils_writebuffer(&contents, path.ptr, O_RDWR|O_CREAT, REBASE_FILE_MODE);

	git_buf_free(&path);
	git_buf_free(&contents);

	return error;
}

/* TODO: git.git actually uses the literal argv here, this is an attempt
 * to emulate that.
 */
static const char *rebase_onto_name(const git_merge_head *onto)
{
	if (onto->ref_name && git__strncmp(onto->ref_name, "refs/heads/", 11) == 0)
		return onto->ref_name + 11;
	else if (onto->ref_name)
		return onto->ref_name;
	else
		return onto->oid_str;
}

static int rebase_setup_merge(
	git_repository *repo,
	const git_merge_head *branch,
	const git_merge_head *upstream,
	const git_merge_head *onto,
	const git_rebase_options *opts)
{
	git_revwalk *revwalk = NULL;
	git_commit *commit;
	git_buf commit_filename = GIT_BUF_INIT;
	git_oid id;
	char id_str[GIT_OID_HEXSZ];
	bool merge;
	int commit_cnt = 0, error;

	GIT_UNUSED(opts);

	if (!upstream)
		upstream = onto;

	if ((error = git_revwalk_new(&revwalk, repo)) < 0 ||
		(error = git_revwalk_push(revwalk, &branch->oid)) < 0 ||
		(error = git_revwalk_hide(revwalk, &upstream->oid)) < 0)
		goto done;

	git_revwalk_sorting(revwalk, GIT_SORT_REVERSE | GIT_SORT_TIME);

	while ((error = git_revwalk_next(&id, revwalk)) == 0) {
		if ((error = git_commit_lookup(&commit, repo, &id)) < 0)
			goto done;

		merge = (git_commit_parentcount(commit) > 1);
		git_commit_free(commit);

		if (merge)
			continue;

		commit_cnt++;

		git_buf_clear(&commit_filename);
		git_buf_printf(&commit_filename, CMT_FILE_FMT, commit_cnt);

		git_oid_fmt(id_str, &id);
		if ((error = rebase_setupfile(repo, commit_filename.ptr,
				"%.*s\n", GIT_OID_HEXSZ, id_str)) < 0)
			goto done;
	}

	if (error != GIT_ITEROVER ||
		(error = rebase_setupfile(repo, END_FILE, "%d\n", commit_cnt)) < 0)
		goto done;

	error = rebase_setupfile(repo, ONTO_NAME_FILE, "%s\n",
		rebase_onto_name(onto));

done:
	git_revwalk_free(revwalk);
	git_buf_free(&commit_filename);

	return error;
}

static int rebase_setup(
	git_repository *repo,
	const git_merge_head *branch,
	const git_merge_head *upstream,
	const git_merge_head *onto,
	const git_rebase_options *opts)
{
	git_buf state_path = GIT_BUF_INIT;
	const char *orig_head_name;
	int error;

	if (git_buf_joinpath(&state_path, repo->path_repository, REBASE_MERGE_DIR) < 0)
		return -1;

	if ((error = p_mkdir(state_path.ptr, REBASE_DIR_MODE)) < 0) {
		giterr_set(GITERR_OS, "Failed to create rebase directory '%s'",
			state_path.ptr);
		goto done;
	}

	if ((error = git_repository__set_orig_head(repo, &branch->oid)) < 0)
		goto done;

	orig_head_name = branch->ref_name ? branch->ref_name : ORIG_DETACHED_HEAD;

	if ((error = rebase_setupfile(repo, HEAD_NAME_FILE, "%s\n", orig_head_name)) < 0 ||
		(error = rebase_setupfile(repo, ONTO_FILE, "%s\n", onto->oid_str)) < 0 ||
		(error = rebase_setupfile(repo, ORIG_HEAD_FILE, "%s\n", branch->oid_str)) < 0 ||
		(error = rebase_setupfile(repo, QUIET_FILE, opts->quiet ? "t\n" : "\n")) < 0)
		goto done;

	error = rebase_setup_merge(repo, branch, upstream, onto, opts);

done:
	if (error < 0)
		git_repository__cleanup_files(repo, (const char **)&state_path.ptr, 1);

	git_buf_free(&state_path);

	return error;
}

int git_rebase_init_options(git_rebase_options *opts, unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(
		opts, version, git_rebase_options, GIT_REBASE_OPTIONS_INIT);
	return 0;
}

static void rebase_normalize_options(
	git_rebase_options *opts,
	const git_rebase_options *given_opts)
{
	if (given_opts)
		memcpy(&opts, given_opts, sizeof(git_rebase_options));
}

static int rebase_ensure_not_in_progress(git_repository *repo)
{
	int error;
	git_rebase_type_t type;

	if ((error = rebase_state_type(&type, NULL, repo)) < 0)
		return error;

	if (type != GIT_REBASE_TYPE_NONE) {
		giterr_set(GITERR_REBASE, "There is an existing rebase in progress");
		return -1;
	}

	return 0;
}

static int rebase_ensure_not_dirty(git_repository *repo)
{
	git_tree *head = NULL;
	git_index *index = NULL;
	git_diff *diff = NULL;
	int error;

	if ((error = git_repository_head_tree(&head, repo)) < 0 ||
		(error = git_repository_index(&index, repo)) < 0 ||
		(error = git_diff_tree_to_index(&diff, repo, head, index, NULL)) < 0)
		goto done;

	if (git_diff_num_deltas(diff) > 0) {
		giterr_set(GITERR_REBASE, "Uncommitted changes exist in index");
		error = -1;
		goto done;
	}

	git_diff_free(diff);
	diff = NULL;

	if ((error = git_diff_index_to_workdir(&diff, repo, index, NULL)) < 0)
		goto done;

	if (git_diff_num_deltas(diff) > 0) {
		giterr_set(GITERR_REBASE, "Unstaged changes exist in workdir");
		error = -1;
	}

done:
	git_diff_free(diff);
	git_index_free(index);
	git_tree_free(head);

	return error;
}

int git_rebase(
	git_repository *repo,
	const git_merge_head *branch,
	const git_merge_head *upstream,
	const git_merge_head *onto,
	const git_signature *signature,
	const git_rebase_options *given_opts)
{
	git_rebase_options opts = GIT_REBASE_OPTIONS_INIT;
	git_reference *head_ref = NULL;
	git_buf reflog = GIT_BUF_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	int error;

	assert(repo && branch && (upstream || onto));

	GITERR_CHECK_VERSION(given_opts, GIT_MERGE_OPTIONS_VERSION, "git_merge_options");
	rebase_normalize_options(&opts, given_opts);

	if ((error = git_repository__ensure_not_bare(repo, "rebase")) < 0 ||
		(error = rebase_ensure_not_in_progress(repo)) < 0 ||
		(error = rebase_ensure_not_dirty(repo)) < 0)
		goto done;

	if (!onto)
		onto = upstream;

	if ((error = rebase_setup(repo, branch, upstream, onto, &opts)) < 0)
		goto done;

	if ((error = git_buf_printf(&reflog,
			"rebase: checkout %s", rebase_onto_name(onto))) < 0 ||
		(error = git_reference_create(&head_ref, repo, GIT_HEAD_FILE,
			&onto->oid, 1, signature, reflog.ptr)) < 0)
		goto done;

	checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	error = git_checkout_head(repo, &checkout_opts);

done:
	git_reference_free(head_ref);
	git_buf_free(&reflog);
	return error;
}
