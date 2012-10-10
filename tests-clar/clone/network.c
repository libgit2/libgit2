#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

CL_IN_CATEGORY("network")

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"
#define LIVE_EMPTYREPO_URL "git://github.com/libgit2/TestEmptyRepository"

static git_repository *g_repo;

void test_clone_network__initialize(void)
{
	g_repo = NULL;
}

static void cleanup_repository(void *path)
{
	if (g_repo)
		git_repository_free(g_repo);
	cl_fixture_cleanup((const char *)path);
}


void test_clone_network__network_full(void)
{
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test2");

	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./test2", NULL, NULL, NULL));
	cl_assert(!git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
}


void test_clone_network__network_bare(void)
{
	git_remote *origin;

	cl_set_cleanup(&cleanup_repository, "./test");

	cl_git_pass(git_clone_bare(&g_repo, LIVE_REPO_URL, "./test", NULL));
	cl_assert(git_repository_is_bare(g_repo));
	cl_git_pass(git_remote_load(&origin, g_repo, "origin"));
}

void test_clone_network__cope_with_already_existing_directory(void)
{
	cl_set_cleanup(&cleanup_repository, "./foo");

	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./foo", NULL, NULL, NULL));
	git_repository_free(g_repo); g_repo = NULL;
}

void test_clone_network__empty_repository(void)
{
	git_reference *head;

	cl_set_cleanup(&cleanup_repository, "./empty");

	cl_git_pass(git_clone(&g_repo, LIVE_EMPTYREPO_URL, "./empty", NULL, NULL, NULL));

	cl_assert_equal_i(true, git_repository_is_empty(g_repo));
	cl_assert_equal_i(true, git_repository_head_orphan(g_repo));

	cl_git_pass(git_reference_lookup(&head, g_repo, GIT_HEAD_FILE));
	cl_assert_equal_i(GIT_REF_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s("refs/heads/master", git_reference_target(head));

	git_reference_free(head);
}
