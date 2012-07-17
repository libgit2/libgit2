#include "clar_libgit2.h"
#include "fileops.h"
#include "repository.h"
#include "config.h"

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

void test_repo_init__bare_repo_escaping_current_workdir(void)
{
	git_buf path_repository = GIT_BUF_INIT;
	git_buf path_current_workdir = GIT_BUF_INIT;

	cl_git_pass(git_path_prettify_dir(&path_current_workdir, ".", NULL));
	
	cl_git_pass(git_buf_joinpath(&path_repository, git_buf_cstr(&path_current_workdir), "a/b/c"));
	cl_git_pass(git_futils_mkdir_r(git_buf_cstr(&path_repository), NULL, GIT_DIR_MODE));

	/* Change the current working directory */
	cl_git_pass(chdir(git_buf_cstr(&path_repository)));

	/* Initialize a bare repo with a relative path escaping out of the current working directory */
	cl_git_pass(git_repository_init(&_repo, "../d/e.git", 1));
	cl_git_pass(git__suffixcmp(git_repository_path(_repo), "/a/b/d/e.git/"));

	git_repository_free(_repo);

	/* Open a bare repo with a relative path escaping out of the current working directory */
	cl_git_pass(git_repository_open(&_repo, "../d/e.git"));

	cl_git_pass(chdir(git_buf_cstr(&path_current_workdir)));

	git_buf_free(&path_current_workdir);
	git_buf_free(&path_repository);

	cleanup_repository("a");
}

void test_repo_init__reinit_bare_repo(void)
{
	cl_set_cleanup(&cleanup_repository, "reinit.git");

	/* Initialize the repository */
	cl_git_pass(git_repository_init(&_repo, "reinit.git", 1));
	git_repository_free(_repo);

	/* Reinitialize the repository */
	cl_git_pass(git_repository_init(&_repo, "reinit.git", 1));
}

void test_repo_init__reinit_too_recent_bare_repo(void)
{
	git_config *config;

	/* Initialize the repository */
	cl_git_pass(git_repository_init(&_repo, "reinit.git", 1));
	git_repository_config(&config, _repo);

	/*
	 * Hack the config of the repository to make it look like it has
	 * been created by a recenter version of git/libgit2
	 */
	cl_git_pass(git_config_set_int32(config, "core.repositoryformatversion", 42));

	git_config_free(config);
	git_repository_free(_repo);

	/* Try to reinitialize the repository */
	cl_git_fail(git_repository_init(&_repo, "reinit.git", 1));

	cl_fixture_cleanup("reinit.git");
}

void test_repo_init__additional_templates(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_set_cleanup(&cleanup_repository, "tester");

	ensure_repository_init("tester", 0, "tester/.git/", "tester/");

	cl_git_pass(
		git_buf_joinpath(&path, git_repository_path(_repo), "description"));
	cl_assert(git_path_isfile(git_buf_cstr(&path)));

	cl_git_pass(
		git_buf_joinpath(&path, git_repository_path(_repo), "info/exclude"));
	cl_assert(git_path_isfile(git_buf_cstr(&path)));

	cl_git_pass(
		git_buf_joinpath(&path, git_repository_path(_repo), "hooks"));
	cl_assert(git_path_isdir(git_buf_cstr(&path)));
	/* won't confirm specific contents of hooks dir since it may vary */

	git_buf_free(&path);
}

static void assert_config_entry_on_init_bytype(const char *config_key, int expected_value, bool is_bare)
{
	git_config *config;
	int current_value;
	git_buf repo_path = GIT_BUF_INIT;

	cl_set_cleanup(&cleanup_repository, "config_entry");

	cl_git_pass(git_buf_puts(&repo_path, "config_entry/test."));

	if (!is_bare)
		cl_git_pass(git_buf_puts(&repo_path, "non."));

	cl_git_pass(git_buf_puts(&repo_path, "bare.git"));

	cl_git_pass(git_repository_init(&_repo, git_buf_cstr(&repo_path), is_bare));

	git_buf_free(&repo_path);

	git_repository_config(&config, _repo);

	if (expected_value >= 0) {
		cl_git_pass(git_config_get_bool(&current_value, config, config_key));

		cl_assert_equal_i(expected_value, current_value);
	} else {
		int error = git_config_get_bool(&current_value, config, config_key);

		cl_assert_equal_i(expected_value, error);
	}

	git_config_free(config);
}

static void assert_config_entry_on_init(const char *config_key, int expected_value)
{
	assert_config_entry_on_init_bytype(config_key, expected_value, true);
	git_repository_free(_repo);

	assert_config_entry_on_init_bytype(config_key, expected_value, false);
}

void test_repo_init__detect_filemode(void)
{
#ifdef GIT_WIN32
	assert_config_entry_on_init("core.filemode", false);
#else
	assert_config_entry_on_init("core.filemode", true);
#endif
}

#define CASE_INSENSITIVE_FILESYSTEM (defined GIT_WIN32 || defined __APPLE__)

void test_repo_init__detect_ignorecase(void)
{
#if CASE_INSENSITIVE_FILESYSTEM
	assert_config_entry_on_init("core.ignorecase", true);
#else
	assert_config_entry_on_init("core.ignorecase", GIT_ENOTFOUND);
#endif
}

void test_repo_init__reinit_doesnot_overwrite_ignorecase(void)
{
	git_config *config;
	int current_value;

	/* Init a new repo */
	cl_set_cleanup(&cleanup_repository, "not.overwrite.git");
	cl_git_pass(git_repository_init(&_repo, "not.overwrite.git", 1));

	/* Change the "core.ignorecase" config value to something unlikely */
	git_repository_config(&config, _repo);
	git_config_set_int32(config, "core.ignorecase", 42);
	git_config_free(config);
	git_repository_free(_repo);
	_repo = NULL;

	/* Reinit the repository */
	cl_git_pass(git_repository_init(&_repo, "not.overwrite.git", 1));
	git_repository_config(&config, _repo);

	/* Ensure the "core.ignorecase" config value hasn't been updated */
	cl_git_pass(git_config_get_int32(&current_value, config, "core.ignorecase"));
	cl_assert_equal_i(42, current_value);

	git_config_free(config);
}

void test_repo_init__reinit_overwrites_filemode(void)
{
    git_config *config;
    int expected, current_value;

#ifdef GIT_WIN32
    expected = false;
#else
    expected = true;
#endif

    /* Init a new repo */
    cl_set_cleanup(&cleanup_repository, "overwrite.git");
    cl_git_pass(git_repository_init(&_repo, "overwrite.git", 1));


    /* Change the "core.filemode" config value to something unlikely */
    git_repository_config(&config, _repo);
    git_config_set_bool(config, "core.filemode", !expected);
    git_config_free(config);
    git_repository_free(_repo);
    _repo = NULL;

    /* Reinit the repository */
    cl_git_pass(git_repository_init(&_repo, "overwrite.git", 1));
    git_repository_config(&config, _repo);

    /* Ensure the "core.filemode" config value has been reset */
    cl_git_pass(git_config_get_bool(&current_value, config, "core.filemode"));
    cl_assert_equal_i(expected, current_value);

    git_config_free(config);
}

void test_repo_init__sets_logAllRefUpdates_according_to_type_of_repository(void)
{
	assert_config_entry_on_init_bytype("core.logallrefupdates", GIT_ENOTFOUND, true);
	git_repository_free(_repo);
	assert_config_entry_on_init_bytype("core.logallrefupdates", true, false);
}
