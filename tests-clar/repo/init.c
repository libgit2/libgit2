#include "clar_libgit2.h"
#include "fileops.h"
#include "repository.h"
#include "config.h"
#include "path.h"

enum repo_mode {
	STANDARD_REPOSITORY = 0,
	BARE_REPOSITORY = 1
};

static git_repository *_repo = NULL;

void test_repo_init__initialize(void)
{
	_repo = NULL;
}

static void cleanup_repository(void *path)
{
	git_repository_free(_repo);
	_repo = NULL;

	cl_fixture_cleanup((const char *)path);
}

static void ensure_repository_init(
	const char *working_directory,
	int is_bare,
	const char *expected_path_repository,
	const char *expected_working_directory)
{
	const char *workdir;

	cl_assert(!git_path_isdir(working_directory));

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
		DWORD fattrs = GetFileAttributes(git_repository_path(_repo));
		cl_assert((fattrs & FILE_ATTRIBUTE_HIDDEN) != 0);
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
	cl_repo_set_bool(_repo, "core.filemode", !expected);

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

void test_repo_init__extended_0(void)
{
	git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

	/* without MKDIR this should fail */
	cl_git_fail(git_repository_init_ext(&_repo, "extended", &opts));

	/* make the directory first, then it should succeed */
	cl_git_pass(git_futils_mkdir("extended", NULL, 0775, 0));
	cl_git_pass(git_repository_init_ext(&_repo, "extended", &opts));

	cl_assert(!git__suffixcmp(git_repository_workdir(_repo), "/extended/"));
	cl_assert(!git__suffixcmp(git_repository_path(_repo), "/extended/.git/"));
	cl_assert(!git_repository_is_bare(_repo));
	cl_assert(git_repository_is_empty(_repo));

	cleanup_repository("extended");
}

void test_repo_init__extended_1(void)
{
	git_reference *ref;
	git_remote *remote;
	struct stat st;
	git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

	opts.flags = GIT_REPOSITORY_INIT_MKPATH |
		GIT_REPOSITORY_INIT_NO_DOTGIT_DIR;
	opts.mode = GIT_REPOSITORY_INIT_SHARED_GROUP;
	opts.workdir_path = "../c_wd";
	opts.description = "Awesomest test repository evah";
	opts.initial_head = "development";
	opts.origin_url = "https://github.com/libgit2/libgit2.git";

	cl_git_pass(git_repository_init_ext(&_repo, "root/b/c.git", &opts));

	cl_assert(!git__suffixcmp(git_repository_workdir(_repo), "/c_wd/"));
	cl_assert(!git__suffixcmp(git_repository_path(_repo), "/c.git/"));
	cl_assert(git_path_isfile("root/b/c_wd/.git"));
	cl_assert(!git_repository_is_bare(_repo));
	/* repo will not be counted as empty because we set head to "development" */
	cl_assert(!git_repository_is_empty(_repo));

	cl_git_pass(git_path_lstat(git_repository_path(_repo), &st));
	cl_assert(S_ISDIR(st.st_mode));
	cl_assert((S_ISGID & st.st_mode) == S_ISGID);

	cl_git_pass(git_reference_lookup(&ref, _repo, "HEAD"));
	cl_assert(git_reference_type(ref) == GIT_REF_SYMBOLIC);
	cl_assert_equal_s("refs/heads/development", git_reference_symbolic_target(ref));
	git_reference_free(ref);

	cl_git_pass(git_remote_load(&remote, _repo, "origin"));
	cl_assert_equal_s("origin", git_remote_name(remote));
	cl_assert_equal_s(opts.origin_url, git_remote_url(remote));
	git_remote_free(remote);

	git_repository_free(_repo);
	cl_fixture_cleanup("root");
}

static void assert_hooks_match(
	const char *template_dir,
	const char *repo_dir,
	const char *hook_path,
	bool core_filemode)
{
	git_buf expected = GIT_BUF_INIT;
	git_buf actual = GIT_BUF_INIT;
	struct stat expected_st, st;

	cl_git_pass(git_buf_joinpath(&expected, template_dir, hook_path));
	cl_git_pass(git_path_lstat(expected.ptr, &expected_st));

	cl_git_pass(git_buf_joinpath(&actual, repo_dir, hook_path));
	cl_git_pass(git_path_lstat(actual.ptr, &st));

	cl_assert(expected_st.st_size == st.st_size);

	if (!core_filemode) {
		expected_st.st_mode = expected_st.st_mode & ~0111;
		st.st_mode = st.st_mode & ~0111;
	}

	cl_assert_equal_i((int)expected_st.st_mode, (int)st.st_mode);

	git_buf_free(&expected);
	git_buf_free(&actual);
}

static void assert_mode_seems_okay(
	const char *base, const char *path,
	git_filemode_t expect_mode, bool expect_setgid, bool core_filemode)
{
	git_buf full = GIT_BUF_INIT;
	struct stat st;

	cl_git_pass(git_buf_joinpath(&full, base, path));
	cl_git_pass(git_path_lstat(full.ptr, &st));
	git_buf_free(&full);

	if (!core_filemode) {
		expect_mode = expect_mode & ~0111;
		st.st_mode = st.st_mode & ~0111;
		expect_setgid = false;
	}

	if (S_ISGID != 0) {
		if (expect_setgid)
			cl_assert((st.st_mode & S_ISGID) != 0);
		else
			cl_assert((st.st_mode & S_ISGID) == 0);
	}

	if ((expect_mode & 0111) != 0)
		cl_assert((st.st_mode & 0111) != 0);
	else
		cl_assert((st.st_mode & 0111) == 0);

	cl_assert((expect_mode & 0170000) == (st.st_mode & 0170000));
}

void test_repo_init__extended_with_template(void)
{
	git_buf expected = GIT_BUF_INIT;
	git_buf actual = GIT_BUF_INIT;
	git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;

	cl_set_cleanup(&cleanup_repository, "templated.git");

	opts.flags = GIT_REPOSITORY_INIT_MKPATH | GIT_REPOSITORY_INIT_BARE |
		GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE;
	opts.template_path = cl_fixture("template");

	cl_git_pass(git_repository_init_ext(&_repo, "templated.git", &opts));

	cl_assert(git_repository_is_bare(_repo));

	cl_assert(!git__suffixcmp(git_repository_path(_repo), "/templated.git/"));

	cl_git_pass(git_futils_readbuffer(
		&expected, cl_fixture("template/description")));
	cl_git_pass(git_futils_readbuffer(
		&actual, "templated.git/description"));

	cl_assert_equal_s(expected.ptr, actual.ptr);

	git_buf_free(&expected);
	git_buf_free(&actual);

	assert_hooks_match(
		cl_fixture("template"), git_repository_path(_repo),
		"hooks/update.sample", true);

	assert_hooks_match(
		cl_fixture("template"), git_repository_path(_repo),
		"hooks/link.sample", true);
}

void test_repo_init__extended_with_template_and_shared_mode(void)
{
	git_buf expected = GIT_BUF_INIT;
	git_buf actual = GIT_BUF_INIT;
	git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	git_config *config;
	int filemode = true;
	const char *repo_path = NULL;

	cl_set_cleanup(&cleanup_repository, "init_shared_from_tpl");

	opts.flags = GIT_REPOSITORY_INIT_MKPATH |
		GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE;
	opts.template_path = cl_fixture("template");
	opts.mode = GIT_REPOSITORY_INIT_SHARED_GROUP;

	cl_git_pass(git_repository_init_ext(&_repo, "init_shared_from_tpl", &opts));

	cl_assert(!git_repository_is_bare(_repo));
	cl_assert(!git__suffixcmp(git_repository_path(_repo), "/init_shared_from_tpl/.git/"));

	cl_git_pass(git_repository_config(&config, _repo));
	cl_git_pass(git_config_get_bool(&filemode, config, "core.filemode"));
	git_config_free(config);

	cl_git_pass(git_futils_readbuffer(
		&expected, cl_fixture("template/description")));
	cl_git_pass(git_futils_readbuffer(
		&actual, "init_shared_from_tpl/.git/description"));

	cl_assert_equal_s(expected.ptr, actual.ptr);

	git_buf_free(&expected);
	git_buf_free(&actual);

	repo_path = git_repository_path(_repo);
	assert_mode_seems_okay(repo_path, "hooks",
		GIT_FILEMODE_TREE | GIT_REPOSITORY_INIT_SHARED_GROUP, true, filemode);
	assert_mode_seems_okay(repo_path, "info",
		GIT_FILEMODE_TREE | GIT_REPOSITORY_INIT_SHARED_GROUP, true, filemode);
	assert_mode_seems_okay(repo_path, "description",
		GIT_FILEMODE_BLOB, false, filemode);

	/* for a non-symlinked hook, it should have shared permissions now */
	assert_hooks_match(
		cl_fixture("template"), git_repository_path(_repo),
		"hooks/update.sample", filemode);

	/* for a symlinked hook, the permissions still should match the
	 * source link, not the GIT_REPOSITORY_INIT_SHARED_GROUP value
	 */
	assert_hooks_match(
		cl_fixture("template"), git_repository_path(_repo),
		"hooks/link.sample", filemode);
}

void test_repo_init__can_reinit_an_initialized_repository(void)
{
	git_repository *reinit;

	cl_set_cleanup(&cleanup_repository, "extended");

	cl_git_pass(git_futils_mkdir("extended", NULL, 0775, 0));
	cl_git_pass(git_repository_init(&_repo, "extended", false));

	cl_git_pass(git_repository_init(&reinit, "extended", false));

	cl_assert_equal_s(git_repository_path(_repo), git_repository_path(reinit));

	git_repository_free(reinit);
}
