#include "clar_libgit2.h"
//#include "git2/hook.h"

static git_repository *g_repo = NULL;

void test_hook_saveload__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_hook_saveload__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_hook_saveload__load_empty_hook(void)
{
	git_buf contents = GIT_BUF_INIT;

	cl_git_pass(git_hook_load(&contents, g_repo, "commit-msg"));
}

void test_hook_saveload__save_hook(void)
{
	git_buf contents = GIT_BUF_INIT;
	const char * content_str;
	const char *repo_path;
	char *hook_path;
	struct stat hook_stat;

	content_str =
	"#!/bin/sh\n"
	"\n"
	"echo 'Hello world !'\n"
	;

	git_buf_puts(&contents, content_str);

	cl_git_pass(git_hook_save(&contents, g_repo, "commit-msg"));

	repo_path = git_repository_path(g_repo);
	hook_path = malloc(strlen(repo_path) + 17); /* hooks/commit-msg */

	strcat(hook_path, repo_path);
	strcat(hook_path, "hooks/commit-msg");

	cl_must_pass(p_stat(hook_path, &hook_stat));

	cl_must_pass(hook_stat.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));

	git_buf_clear(&contents);

	cl_git_pass(git_hook_load(&contents, g_repo, "commit-msg"));

	cl_assert_equal_s(git_buf_cstr(&contents), content_str);
}