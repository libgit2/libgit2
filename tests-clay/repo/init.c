#include "clay_libgit2.h"
#include "fileops.h"

enum repo_mode {
	STANDARD_REPOSITORY = 0,
	BARE_REPOSITORY = 1
};

static git_repository *_repo;

void test_repo_init__initialize(void)
{
	_repo = NULL;
}

static void cleanup_repository(void *path)
{
	git_repository_free(_repo);
	cl_fixture_cleanup((const char *)path);
}

static void ensure_repository_init(
	const char *working_directory,
	int is_bare,
	const char *expected_path_repository,
	const char *expected_working_directory)
{
	const char *workdir;

	cl_git_pass(git_repository_init(&_repo, working_directory, is_bare));

	workdir = git_repository_workdir(_repo);
	if (workdir != NULL || expected_working_directory != NULL) {
		cl_assert(
			git__suffixcmp(workdir, expected_working_directory) == 0
		);
	}

	cl_assert(
		git__suffixcmp(git_repository_path(_repo), expected_path_repository) == 0
	);

	cl_assert(git_repository_is_bare(_repo) == is_bare);

#ifdef GIT_WIN32
	if (!is_bare) {
		cl_assert((GetFileAttributes(git_repository_path(_repo)) & FILE_ATTRIBUTE_HIDDEN) != 0);
	}
#endif

	cl_assert(git_repository_is_empty(_repo));
}

void test_repo_init__standard_repo(void)
{
	cl_set_cleanup(&cleanup_repository, "testrepo");
	ensure_repository_init("testrepo/", 0, "testrepo/.git/", "testrepo/");
}

void test_repo_init__standard_repo_noslash(void)
{
	cl_set_cleanup(&cleanup_repository, "testrepo");
	ensure_repository_init("testrepo", 0, "testrepo/.git/", "testrepo/");
}

void test_repo_init__bare_repo(void)
{
	cl_set_cleanup(&cleanup_repository, "testrepo.git");
	ensure_repository_init("testrepo.git/", 1, "testrepo.git/", NULL);
}

void test_repo_init__bare_repo_noslash(void)
{
	cl_set_cleanup(&cleanup_repository, "testrepo.git");
	ensure_repository_init("testrepo.git", 1, "testrepo.git/", NULL);
}

#if 0
BEGIN_TEST(init2, "Initialize and open a bare repo with a relative path escaping out of the current working directory")
	git_buf path_repository = GIT_BUF_INIT;
	char current_workdir[GIT_PATH_MAX];
	const mode_t mode = 0777;
	git_repository* repo;

	must_pass(p_getcwd(current_workdir, sizeof(current_workdir)));

	must_pass(git_buf_joinpath(&path_repository, TEMP_REPO_FOLDER, "a/b/c/"));
	must_pass(git_futils_mkdir_r(path_repository.ptr, mode));

	must_pass(chdir(path_repository.ptr));

	git_buf_free(&path_repository);

	must_pass(git_repository_init(&repo, "../d/e.git", 1));
	must_pass(git__suffixcmp(git_repository_path(_repo), "/a/b/d/e.git/"));

	git_repository_free(repo);

	must_pass(git_repository_open(&repo, "../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST
#endif
