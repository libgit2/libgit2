#include "clar_libgit2.h"
#include "git2/sys/hook.h"

static git_repository *g_repo = NULL;
static git_signature *signature;

void test_hook_call__initialize(void)
{
	g_repo = cl_git_sandbox_init("rebase");

	cl_git_pass(git_signature_new(&signature,
		"Rebaser", "rebaser@rebaser.rb", 1405694510, 0));
}

void test_hook_call__cleanup(void)
{
	cl_git_sandbox_cleanup();
	git_signature_free(signature);
}

static void make_dummy_hook(const char *hook_name)
{
	git_buf hook_path = GIT_BUF_INIT;

	cl_git_pass(git_hook_dir(&hook_path, g_repo));

	cl_must_pass(p_mkdir(git_buf_cstr(&hook_path), 0777));

	git_buf_joinpath(&hook_path, hook_path.ptr, hook_name);

	cl_git_mkfile(git_buf_cstr(&hook_path), NULL);
	cl_must_pass(p_chmod(git_buf_cstr(&hook_path), 0776));

	git_buf_dispose(&hook_path);
}

int hook_exec__pre_rebase(git_hook_env *env, void *payload)
{
	GIT_UNUSED(payload);

	if (strstr(env->args.strings[0], "master") != NULL) {
		return -1;
	}

	return 0;
}

void test_hook_call__pre_rebase_hook(void)
{
	git_rebase *rebase;
	git_reference *branch_ref, *upstream_ref;
	git_annotated_commit *branch_head, *upstream_head;

	make_dummy_hook("pre-rebase");

	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__pre_rebase, NULL, NULL));

	cl_git_pass(git_reference_lookup(&branch_ref, g_repo, "refs/heads/beef"));
	cl_git_pass(git_reference_lookup(&upstream_ref, g_repo, "refs/heads/master"));

	cl_git_pass(git_annotated_commit_from_ref(&branch_head, g_repo, branch_ref));
	cl_git_pass(git_annotated_commit_from_ref(&upstream_head, g_repo, upstream_ref));

	cl_git_fail_with(git_rebase_init(&rebase, g_repo, branch_head, upstream_head, NULL, NULL), -1);

	git_annotated_commit_free(branch_head);
	git_annotated_commit_free(upstream_head);
	git_reference_free(branch_ref);
	git_reference_free(upstream_ref);
	git_rebase_free(rebase);
}

struct hook_data {
	int calls;
	git_strarray args;
};

static void cl_git_hook_check_(git_hook_env *env, struct hook_data *data)
{
	size_t arg_id;

	(data->calls)++;

	cl_assert_equal_i(env->args.count, data->args.count);

	for (arg_id = 0; arg_id < data->args.count; arg_id++)
		cl_assert_equal_s(data->args.strings[arg_id], env->args.strings[arg_id]);
}

#define cl_git_hook_check(env, data) cl_git_hook_check_(env, data)

int hook_exec__check(git_hook_env *env, void *payload)
{
	cl_git_hook_check(env, payload);
	return 0;
}

void test_hook_call__pre_commit(void)
{
	struct hook_data data = {0, {NULL, 0}};

	make_dummy_hook("pre-commit");
	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__check, NULL, &data));

	cl_git_pass(git_hook_call_pre_commit(g_repo));
	cl_assert_equal_i(data.calls, 1);
}

void test_hook_call__prepare_commit_message(void)
{
	git_buf path = GIT_BUF_INIT;
	char *args_str[2];
	struct hook_data data = {0, {args_str, 2}};

	git_buf_sets(&path, git_repository_path(g_repo));
	git_buf_joinpath(&path, path.ptr, "COMMIT_MSG");

	args_str[0] = path.ptr;
	args_str[1] = "message";

	make_dummy_hook("prepare-commit-msg");
	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__check, NULL, &data));

	cl_git_pass(git_hook_call_prepare_commit_message(g_repo, GIT_HOOK_PREPARE_COMMIT_MSG_MESSAGE, "nice message"));
	cl_assert_equal_i(data.calls, 1);

	git_buf_dispose(&path);
}

void test_hook_call__prepare_commit_template(void)
{
	git_buf path = GIT_BUF_INIT;
	char *args_str[2];
	struct hook_data data = {0, {args_str, 2}};

	git_buf_sets(&path, git_repository_path(g_repo));
	git_buf_joinpath(&path, path.ptr, "COMMIT_MSG");

	args_str[0] = path.ptr;
	args_str[1] = "template";

	make_dummy_hook("prepare-commit-msg");
	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__check, NULL, &data));

	cl_git_pass(git_hook_call_prepare_commit_message(g_repo, GIT_HOOK_PREPARE_COMMIT_MSG_TEMPLATE, "tmpl-file.txt"));
	cl_assert_equal_i(data.calls, 1);

	git_buf_dispose(&path);
}

void test_hook_call__commit_msg(void)
{
	git_buf path = GIT_BUF_INIT;
	char *args_str[1];
	struct hook_data data = {0, {args_str, 1}};

	git_buf_sets(&path, git_repository_path(g_repo));
	git_buf_joinpath(&path, path.ptr, "COMMIT_MSG");

	args_str[0] = path.ptr;

	make_dummy_hook("commit-msg");
	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__check, NULL, &data));

	cl_git_pass(git_hook_call_commit_msg(g_repo, "my message"));
	cl_assert_equal_i(data.calls, 1);

	git_buf_dispose(&path);
}

void test_hook_call__post_commit(void)
{
	struct hook_data data = {0, {NULL, 0}};

	make_dummy_hook("post-commit");
	cl_git_pass(git_hook_register_callback(g_repo, hook_exec__check, NULL, &data));

	cl_git_pass(git_hook_call_post_commit(g_repo));
	cl_assert_equal_i(data.calls, 1);
}
