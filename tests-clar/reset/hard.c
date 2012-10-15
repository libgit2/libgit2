#include "clar_libgit2.h"
#include "posix.h"
#include "reset_helpers.h"
#include "path.h"
#include "fileops.h"

static git_repository *repo;
static git_object *target;

void test_reset_hard__initialize(void)
{
	repo = cl_git_sandbox_init("status");
	target = NULL;
}

void test_reset_hard__cleanup(void)
{
	git_object_free(target);
	cl_git_sandbox_cleanup();
}

void test_reset_hard__resetting_culls_empty_directories(void)
{
	git_buf subdir_path = GIT_BUF_INIT;
	git_buf subfile_path = GIT_BUF_INIT;
	git_buf newdir_path = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&newdir_path, git_repository_workdir(repo), "newdir/"));

	cl_git_pass(git_buf_joinpath(&subfile_path, git_buf_cstr(&newdir_path), "with/nested/file.txt"));
	cl_git_pass(git_futils_mkpath2file(git_buf_cstr(&subfile_path), 0755));
	cl_git_mkfile(git_buf_cstr(&subfile_path), "all anew...\n");

	cl_git_pass(git_buf_joinpath(&subdir_path, git_repository_workdir(repo), "subdir/"));
	cl_assert(git_path_isdir(git_buf_cstr(&subdir_path)) == true);

	retrieve_target_from_oid(&target, repo, "0017bd4ab1ec30440b17bae1680cff124ab5f1f6");
	cl_git_pass(git_reset(repo, target, GIT_RESET_HARD));

	cl_assert(git_path_isdir(git_buf_cstr(&subdir_path)) == false);
	cl_assert(git_path_isdir(git_buf_cstr(&newdir_path)) == false);

	git_buf_free(&subdir_path);
	git_buf_free(&subfile_path);
	git_buf_free(&newdir_path);
}

void test_reset_hard__cannot_reset_in_a_bare_repository(void)
{
	git_repository *bare;

	cl_git_pass(git_repository_open(&bare, cl_fixture("testrepo.git")));
	cl_assert(git_repository_is_bare(bare) == true);

	retrieve_target_from_oid(&target, bare, KNOWN_COMMIT_IN_BARE_REPO);

	cl_assert_equal_i(GIT_EBAREREPO, git_reset(bare, target, GIT_RESET_HARD));

	git_repository_free(bare);
}
