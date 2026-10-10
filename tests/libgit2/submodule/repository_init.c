#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "submodule_helpers.h"
#include "config/config_helpers.h"
#include "futils.h"

static git_repository *g_repo = NULL;

void test_submodule_repository_init__basic(void)
{
	git_submodule *sm;
	git_repository *repo;
	git_str dot_git_content = GIT_STR_INIT;

	g_repo = setup_fixture_submod2();

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_gitmodules_only"));
	cl_git_pass(git_submodule_init(sm, 0));
	cl_git_pass(git_submodule_repo_init(&repo, sm, 1));

	/* Verify worktree */
	assert_config_entry_value(repo, "core.worktree", "../../../sm_gitmodules_only/");

	/* Verify gitlink */
	cl_git_pass(git_futils_readbuffer(&dot_git_content, "submod2/" "sm_gitmodules_only" "/.git"));
	cl_assert_equal_s("gitdir: ../.git/modules/sm_gitmodules_only/\n", dot_git_content.ptr);

	cl_assert(git_fs_path_isfile("submod2/" "sm_gitmodules_only" "/.git"));

	cl_assert(git_fs_path_isdir("submod2/.git/modules"));
	cl_assert(git_fs_path_isdir("submod2/.git/modules/" "sm_gitmodules_only"));
	cl_assert(git_fs_path_isfile("submod2/.git/modules/" "sm_gitmodules_only" "/HEAD"));

	git_submodule_free(sm);
	git_repository_free(repo);
	git_str_dispose(&dot_git_content);
}

void test_submodule_repository_init__name_differs_from_path(void)
{
	git_submodule *sm;
	git_repository *repo;
	git_str dot_git_content = GIT_STR_INIT;

	g_repo = setup_fixture_submodule_with_path();

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_git_pass(git_submodule_init(sm, 0));
	cl_git_pass(git_submodule_repo_init(&repo, sm, 1));

	assert_config_entry_value(repo, "core.worktree", "../../../lib/testrepo/");
	cl_git_pass(git_futils_readbuffer(&dot_git_content, "submodule_with_path/lib/testrepo/.git"));
	cl_assert_equal_s("gitdir: ../../.git/modules/testrepo/\n", dot_git_content.ptr);
	cl_assert(git_fs_path_isfile("submodule_with_path/.git/modules/testrepo/HEAD"));
	cl_assert(!git_fs_path_exists("submodule_with_path/.git/modules/lib/testrepo"));

	git_submodule_free(sm);
	git_repository_free(repo);
	git_str_dispose(&dot_git_content);
}

void test_submodule_repository_init__existing_path_layout(void)
{
	git_submodule *sm;
	git_repository *legacy, *repo = NULL;
	git_repository_init_options options = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	git_str workdir = GIT_STR_INIT, dot_git_content = GIT_STR_INIT;

	g_repo = setup_fixture_submodule_with_path();

	cl_git_pass(git_str_joinpath(&workdir, git_repository_workdir(g_repo), "lib/testrepo"));
	options.flags = GIT_REPOSITORY_INIT_MKPATH | GIT_REPOSITORY_INIT_RELATIVE_GITLINK;
	options.workdir_path = workdir.ptr;
	cl_git_pass(git_repository_init_ext(&legacy,
		"submodule_with_path/.git/modules/lib/testrepo", &options));

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_git_pass(git_submodule_init(sm, 0));
	cl_git_fail_with(GIT_EEXISTS, git_submodule_repo_init(&repo, sm, 1));

	cl_git_pass(git_futils_readbuffer(&dot_git_content, "submodule_with_path/lib/testrepo/.git"));
	cl_assert_equal_s("gitdir: ../../.git/modules/lib/testrepo/\n", dot_git_content.ptr);
	cl_assert(!git_fs_path_exists("submodule_with_path/.git/modules/testrepo"));

	git_submodule_free(sm);
	git_repository_free(legacy);
	git_repository_free(repo);
	git_str_dispose(&workdir);
	git_str_dispose(&dot_git_content);
}
