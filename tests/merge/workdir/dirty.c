#include "clar_libgit2.h"
#include "git2/merge.h"
#include "buffer.h"
#include "merge.h"
#include "../merge_helpers.h"

#define TEST_REPO_PATH "merge-resolve"
#define MERGE_BRANCH_OID "7cb63eed597130ba4abb87b3e544b85021905520"

static git_repository *repo;
static git_index *repo_index;

static char *unaffected[][4] = {
	{ "added-in-master.txt", NULL },
	{ "changed-in-master.txt", NULL },
	{ "unchanged.txt", NULL },
	{ "added-in-master.txt", "changed-in-master.txt", NULL },
	{ "added-in-master.txt", "unchanged.txt", NULL },
	{ "changed-in-master.txt", "unchanged.txt", NULL },
	{ "added-in-master.txt", "changed-in-master.txt", "unchanged.txt", NULL },
	{ "new_file.txt", NULL },
	{ "new_file.txt", "unchanged.txt", NULL },
	{ NULL },
};

static char *affected[][5] = {
	{ "automergeable.txt", NULL },
	{ "changed-in-branch.txt", NULL },
	{ "conflicting.txt", NULL },
	{ "removed-in-branch.txt", NULL },
	{ "automergeable.txt", "changed-in-branch.txt", NULL },
	{ "automergeable.txt", "conflicting.txt", NULL },
	{ "automergeable.txt", "removed-in-branch.txt", NULL },
	{ "changed-in-branch.txt", "conflicting.txt", NULL },
	{ "changed-in-branch.txt", "removed-in-branch.txt", NULL },
	{ "conflicting.txt", "removed-in-branch.txt", NULL },
	{ "automergeable.txt", "changed-in-branch.txt", "conflicting.txt", NULL },
	{ "automergeable.txt", "changed-in-branch.txt", "removed-in-branch.txt", NULL },
	{ "automergeable.txt", "conflicting.txt", "removed-in-branch.txt", NULL },
	{ "changed-in-branch.txt", "conflicting.txt", "removed-in-branch.txt", NULL },
	{ "automergeable.txt", "changed-in-branch.txt", "conflicting.txt", "removed-in-branch.txt", NULL },
	{ NULL },
};

void test_merge_workdir_dirty__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&repo_index, repo);
}

void test_merge_workdir_dirty__cleanup(void)
{
	git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

static void set_core_autocrlf_to(git_repository *repo, bool value)
{
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_bool(cfg, "core.autocrlf", value));

	git_config_free(cfg);
}

static int merge_branch(git_merge_result **result, int merge_file_favor, int checkout_strategy)
{
	git_oid their_oids[1];
	git_merge_head *their_heads[1];
	git_merge_opts opts = GIT_MERGE_OPTS_INIT;
	int error;

	cl_git_pass(git_oid_fromstr(&their_oids[0], MERGE_BRANCH_OID));
	cl_git_pass(git_merge_head_from_id(&their_heads[0], repo, &their_oids[0]));

	opts.merge_tree_opts.file_favor = merge_file_favor;
	opts.checkout_opts.checkout_strategy = checkout_strategy;
	error = git_merge(result, repo, (const git_merge_head **)their_heads, 1, &opts);

	git_merge_head_free(their_heads[0]);

	return error;
}

static void write_files(char *files[])
{
	char *filename;
	git_buf path = GIT_BUF_INIT, content = GIT_BUF_INIT;
	size_t i;

	for (i = 0, filename = files[i]; filename; filename = files[++i]) {
		git_buf_clear(&path);
		git_buf_clear(&content);

		git_buf_printf(&path, "%s/%s", TEST_REPO_PATH, filename);
		git_buf_printf(&content, "This is a dirty file in the working directory!\n\n"
			"It will not be staged!  Its filename is %s.\n", filename);

		cl_git_mkfile(path.ptr, content.ptr);
	}

	git_buf_free(&path);
	git_buf_free(&content);
}

static void stage_random_files(char *files[])
{
	char *filename;
	size_t i;

	write_files(files);

	for (i = 0, filename = files[i]; filename; filename = files[++i])
		cl_git_pass(git_index_add_bypath(repo_index, filename));
}

static int merge_dirty_files(char *dirty_files[])
{
	git_reference *head;
	git_object *head_object;
	git_merge_result *result = NULL;
	int error;

	cl_git_pass(git_repository_head(&head, repo));
	cl_git_pass(git_reference_peel(&head_object, head, GIT_OBJ_COMMIT));
	cl_git_pass(git_reset(repo, head_object, GIT_RESET_HARD));

	write_files(dirty_files);

	error = merge_branch(&result, 0, 0);

	git_merge_result_free(result);
	git_object_free(head_object);
	git_reference_free(head);

	return error;
}

static int merge_staged_files(char *staged_files[])
{
	git_merge_result *result = NULL;
	int error;
	
	stage_random_files(staged_files);

	error = merge_branch(&result, 0, 0);

	git_merge_result_free(result);

	return error;
}

void test_merge_workdir_dirty__unaffected_dirty_files_allowed(void)
{
	char **files;
	size_t i;

	for (i = 0, files = unaffected[i]; files[0]; files = unaffected[++i])
		cl_git_pass(merge_dirty_files(files));
}

void test_merge_workdir_dirty__affected_dirty_files_disallowed(void)
{
	char **files;
	size_t i;

	for (i = 0, files = affected[i]; files[0]; files = affected[++i])
		cl_git_fail(merge_dirty_files(files));
}

void test_merge_workdir_dirty__staged_files_in_index_disallowed(void)
{
	char **files;
	size_t i;

	for (i = 0, files = unaffected[i]; files[0]; files = unaffected[++i])
		cl_git_fail(merge_staged_files(files));

	for (i = 0, files = affected[i]; files[0]; files = affected[++i])
		cl_git_fail(merge_staged_files(files));
}
