#include "clay_libgit2.h"
#include "posix.h"

void test_repo_open__bare_empty_repo(void)
{
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_bare.git")));
	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git_repository_workdir(repo) == NULL);

	git_repository_free(repo);
}

void test_repo_open__standard_empty_repo(void)
{
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("empty_standard_repo/.gitted")));
	cl_assert(git_repository_path(repo) != NULL);
	cl_assert(git_repository_workdir(repo) != NULL);

	git_repository_free(repo);
}

/* TODO TODO */
#if 0
BEGIN_TEST(open2, "Open a bare repository with a relative path escaping out of the current working directory")
	char current_workdir[GIT_PATH_MAX];
	git_buf new_current_workdir = GIT_BUF_INIT;
	git_buf path_repository = GIT_BUF_INIT;

	const mode_t mode = 0777;
	git_repository* repo;

	/* Setup the repository to open */
	must_pass(p_getcwd(current_workdir, sizeof(current_workdir)));
	must_pass(git_buf_join_n(&path_repository, 3, current_workdir, TEMP_REPO_FOLDER, "a/d/e.git"));
	must_pass(copydir_recurs(REPOSITORY_FOLDER, path_repository.ptr));
	git_buf_free(&path_repository);

	/* Change the current working directory */
	must_pass(git_buf_joinpath(&new_current_workdir, TEMP_REPO_FOLDER, "a/b/c/"));
	must_pass(git_futils_mkdir_r(new_current_workdir.ptr, mode));
	must_pass(chdir(new_current_workdir.ptr));
	git_buf_free(&new_current_workdir);

	must_pass(git_repository_open(&repo, "../../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST
#endif
