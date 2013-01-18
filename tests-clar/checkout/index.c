#include "clar_libgit2.h"
#include "checkout_helpers.h"

#include "git2/checkout.h"
#include "repository.h"

static git_repository *g_repo;

void test_checkout_index__initialize(void)
{
	git_tree *tree;

	g_repo = cl_git_sandbox_init("testrepo");

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	reset_index_to_treeish((git_object *)tree);
	git_tree_free(tree);

	cl_git_rewritefile(
		"./testrepo/.gitattributes",
		"* text eol=lf\n");
}

void test_checkout_index__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_index__cannot_checkout_a_bare_repository(void)
{
	test_checkout_index__cleanup();

	g_repo = cl_git_sandbox_init("testrepo.git");

	cl_git_fail(git_checkout_index(g_repo, NULL, NULL));
}

void test_checkout_index__can_create_missing_files(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__can_remove_untracked_files(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	git_futils_mkdir("./testrepo/dir/subdir/subsubdir", NULL, 0755, GIT_MKDIR_PATH);
	cl_git_mkfile("./testrepo/dir/one", "one\n");
	cl_git_mkfile("./testrepo/dir/subdir/two", "two\n");

	cl_assert_equal_i(true, git_path_isdir("./testrepo/dir/subdir/subsubdir"));

	opts.checkout_strategy =
		GIT_CHECKOUT_SAFE_CREATE | GIT_CHECKOUT_REMOVE_UNTRACKED;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/dir"));
}

void test_checkout_index__honor_the_specified_pathspecs(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	char *entries[] = { "*.txt" };

	opts.paths.strings = entries;
	opts.paths.count = 1;

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

static void set_config_entry_to(const char *entry_name, bool value)
{
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, entry_name, value));

	git_config_free(cfg);
}

static void set_core_autocrlf_to(bool value)
{
	set_config_entry_to("core.autocrlf", value);
}

void test_checkout_index__honor_the_gitattributes_directives(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	const char *attributes =
		"branch_file.txt text eol=crlf\n"
		"new.txt text eol=lf\n";

	cl_git_mkfile("./testrepo/.gitattributes", attributes);
	set_core_autocrlf_to(false);

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n");
}

void test_checkout_index__honor_coreautocrlf_setting_set_to_true(void)
{
#ifdef GIT_WIN32
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	const char *expected_readme_text = "hey there\r\n";

	cl_git_pass(p_unlink("./testrepo/.gitattributes"));
	set_core_autocrlf_to(true);

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/README", expected_readme_text);
#endif
}

static void set_repo_symlink_handling_cap_to(bool value)
{
	set_config_entry_to("core.symlinks", value);
}

void test_checkout_index__honor_coresymlinks_setting_set_to_true(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	set_repo_symlink_handling_cap_to(true);

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

#ifdef GIT_WIN32
	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
#else
	{
		char link_data[1024];
		size_t link_size = 1024;

		link_size = p_readlink("./testrepo/link_to_new.txt", link_data, link_size);
		link_data[link_size] = '\0';
		cl_assert_equal_i(link_size, strlen("new.txt"));
		cl_assert_equal_s(link_data, "new.txt");
		test_file_contents("./testrepo/link_to_new.txt", "my new file\n");
	}
#endif
}

void test_checkout_index__honor_coresymlinks_setting_set_to_false(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	set_repo_symlink_handling_cap_to(false);

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
}

void test_checkout_index__donot_overwrite_modified_file_by_default(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	/* set this up to not return an error code on conflicts, but it
	 * still will not have permission to overwrite anything...
	 */
	opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/new.txt", "This isn't what's stored!");
}

void test_checkout_index__can_overwrite_modified_file(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__options_disable_filters(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_git_mkfile("./testrepo/.gitattributes", "*.txt text eol=crlf\n");

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	opts.disable_filters = false;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/new.txt", "my new file\r\n");

	p_unlink("./testrepo/new.txt");

	opts.disable_filters = true;
	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__options_dir_modes(void)
{
#ifndef GIT_WIN32
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	struct stat st;
	git_oid oid;
	git_commit *commit;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/dir"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));

	reset_index_to_treeish((git_object *)commit);

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	opts.dir_mode = 0701;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	cl_git_pass(p_stat("./testrepo/a", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0701);

	/* File-mode test, since we're on the 'dir' branch */
	cl_git_pass(p_stat("./testrepo/a/b.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0755);

	git_commit_free(commit);
#endif
}

void test_checkout_index__options_override_file_modes(void)
{
#ifndef GIT_WIN32
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	struct stat st;

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	opts.file_mode = 0700;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	cl_git_pass(p_stat("./testrepo/new.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0700);
#endif
}

void test_checkout_index__options_open_flags(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_git_mkfile("./testrepo/new.txt", "hi\n");

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	opts.file_open_flags = O_CREAT | O_RDWR | O_APPEND;

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	test_file_contents("./testrepo/new.txt", "hi\nmy new file\n");
}

struct notify_data {
	const char *file;
	const char *sha;
};

static int test_checkout_notify_cb(
	git_checkout_notify_t why,
	const char *path,
	const git_diff_file *baseline,
	const git_diff_file *target,
	const git_diff_file *workdir,
	void *payload)
{
	struct notify_data *expectations = (struct notify_data *)payload;

	GIT_UNUSED(workdir);

	cl_assert_equal_i(GIT_CHECKOUT_NOTIFY_CONFLICT, why);
	cl_assert_equal_s(expectations->file, path);
	cl_assert_equal_i(0, git_oid_streq(&baseline->oid, expectations->sha));
	cl_assert_equal_i(0, git_oid_streq(&target->oid, expectations->sha));

	return 0;
}

void test_checkout_index__can_notify_of_skipped_files(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	struct notify_data data;

	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	/*
	 * $ git ls-tree HEAD
	 * 100644 blob a8233120f6ad708f843d861ce2b7228ec4e3dec6    README
	 * 100644 blob 3697d64be941a53d4ae8f6a271e4e3fa56b022cc    branch_file.txt
	 * 100644 blob a71586c1dfe8a71c6cbf6c129f404c5642ff31bd    new.txt
	 */
	data.file = "new.txt";
	data.sha = "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd";

	opts.checkout_strategy =
		GIT_CHECKOUT_SAFE_CREATE | GIT_CHECKOUT_ALLOW_CONFLICTS;
	opts.notify_flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
	opts.notify_cb = test_checkout_notify_cb;
	opts.notify_payload = &data;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));
}

static int dont_notify_cb(
	git_checkout_notify_t why,
	const char *path,
	const git_diff_file *baseline,
	const git_diff_file *target,
	const git_diff_file *workdir,
	void *payload)
{
	GIT_UNUSED(why);
	GIT_UNUSED(path);
	GIT_UNUSED(baseline);
	GIT_UNUSED(target);
	GIT_UNUSED(workdir);
	GIT_UNUSED(payload);

	cl_assert(false);

	return 0;
}

void test_checkout_index__wont_notify_of_expected_line_ending_changes(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_git_pass(p_unlink("./testrepo/.gitattributes"));
	set_core_autocrlf_to(true);

	cl_git_mkfile("./testrepo/new.txt", "my new file\r\n");

	opts.checkout_strategy =
		GIT_CHECKOUT_SAFE_CREATE | GIT_CHECKOUT_ALLOW_CONFLICTS;
	opts.notify_flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
	opts.notify_cb = dont_notify_cb;
	opts.notify_payload = NULL;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));
}

static void checkout_progress_counter(
	const char *path, size_t cur, size_t tot, void *payload)
{
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	(*(int *)payload)++;
}

void test_checkout_index__calls_progress_callback(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	int calls = 0;

	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	opts.progress_cb = checkout_progress_counter;
	opts.progress_payload = &calls;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));
	cl_assert(calls > 0);
}

void test_checkout_index__can_overcome_name_clashes(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	git_index *index;

	cl_git_pass(git_repository_index(&index, g_repo));
	git_index_clear(index);

	cl_git_mkfile("./testrepo/path0", "content\r\n");
	cl_git_pass(p_mkdir("./testrepo/path1", 0777));
	cl_git_mkfile("./testrepo/path1/file1", "content\r\n");

	cl_git_pass(git_index_add_bypath(index, "path0"));
	cl_git_pass(git_index_add_bypath(index, "path1/file1"));

	cl_git_pass(p_unlink("./testrepo/path0"));
	cl_git_pass(git_futils_rmdir_r(
		"./testrepo/path1", NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_mkfile("./testrepo/path1", "content\r\n");
	cl_git_pass(p_mkdir("./testrepo/path0", 0777));
	cl_git_mkfile("./testrepo/path0/file0", "content\r\n");

	cl_assert(git_path_isfile("./testrepo/path1"));
	cl_assert(git_path_isfile("./testrepo/path0/file0"));

	opts.checkout_strategy =
		GIT_CHECKOUT_SAFE_CREATE | GIT_CHECKOUT_ALLOW_CONFLICTS;
	cl_git_pass(git_checkout_index(g_repo, index, &opts));

	cl_assert(git_path_isfile("./testrepo/path1"));
	cl_assert(git_path_isfile("./testrepo/path0/file0"));

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_checkout_index(g_repo, index, &opts));

	cl_assert(git_path_isfile("./testrepo/path0"));
	cl_assert(git_path_isfile("./testrepo/path1/file1"));

	git_index_free(index);
}

void test_checkout_index__validates_struct_version(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	const git_error *err;

	opts.version = 1024;
	cl_git_fail(git_checkout_index(g_repo, NULL, &opts));

	err = giterr_last();
	cl_assert_equal_i(err->klass, GITERR_INVALID);

	opts.version = 0;
	giterr_clear();
	cl_git_fail(git_checkout_index(g_repo, NULL, &opts));

	err = giterr_last();
	cl_assert_equal_i(err->klass, GITERR_INVALID);
}

void test_checkout_index__can_update_prefixed_files(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	cl_git_mkfile("./testrepo/READ", "content\n");
	cl_git_mkfile("./testrepo/README.after", "content\n");
	cl_git_pass(p_mkdir("./testrepo/branch_file", 0777));
	cl_git_pass(p_mkdir("./testrepo/branch_file/contained_dir", 0777));
	cl_git_mkfile("./testrepo/branch_file/contained_file", "content\n");
	cl_git_pass(p_mkdir("./testrepo/branch_file.txt.after", 0777));

	opts.checkout_strategy =
		GIT_CHECKOUT_SAFE_CREATE | GIT_CHECKOUT_REMOVE_UNTRACKED;

	cl_git_pass(git_checkout_index(g_repo, NULL, &opts));

	/* remove untracked will remove the .gitattributes file before the blobs
	 * were created, so they will have had crlf filtering applied on Windows
	 */
	test_file_contents_nocr("./testrepo/README", "hey there\n");
	test_file_contents_nocr("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents_nocr("./testrepo/new.txt", "my new file\n");

	cl_assert(!git_path_exists("testrepo/READ"));
	cl_assert(!git_path_exists("testrepo/README.after"));
	cl_assert(!git_path_exists("testrepo/branch_file"));
	cl_assert(!git_path_exists("testrepo/branch_file.txt.after"));
}

void test_checkout_index__can_checkout_a_newly_initialized_repository(void)
{
	test_checkout_index__cleanup();

	g_repo = cl_git_sandbox_init("empty_standard_repo");
	cl_git_remove_placeholders(git_repository_path(g_repo), "dummy-marker.txt");

	cl_git_pass(git_checkout_index(g_repo, NULL, NULL));
}
