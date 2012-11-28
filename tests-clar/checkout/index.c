#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"

static git_repository *g_repo;
static git_checkout_opts g_opts;

static void reset_index_to_treeish(git_object *treeish)
{
	git_object *tree;
	git_index *index;

	cl_git_pass(git_object_peel(&tree, treeish, GIT_OBJ_TREE));

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_read_tree(index, (git_tree *)tree));
	cl_git_pass(git_index_write(index));

	git_object_free(tree);
	git_index_free(index);
}

void test_checkout_index__initialize(void)
{
	git_tree *tree;

	memset(&g_opts, 0, sizeof(g_opts));
	g_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

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

static void test_file_contents(const char *path, const char *expectedcontents)
{
	int fd;
	char buffer[1024] = {0};
	size_t expectedlen, actuallen;

	fd = p_open(path, O_RDONLY);
	cl_assert(fd >= 0);

	expectedlen = strlen(expectedcontents);
	actuallen = p_read(fd, buffer, 1024);
	cl_git_pass(p_close(fd));

	cl_assert_equal_sz(actuallen, expectedlen);
	cl_assert_equal_s(buffer, expectedcontents);
}

void test_checkout_index__cannot_checkout_a_bare_repository(void)
{
	test_checkout_index__cleanup();

	memset(&g_opts, 0, sizeof(g_opts));
	g_repo = cl_git_sandbox_init("testrepo.git");

	cl_git_fail(git_checkout_index(g_repo, NULL, NULL));
}

void test_checkout_index__can_create_missing_files(void)
{
	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__can_remove_untracked_files(void)
{
	git_futils_mkdir("./testrepo/dir/subdir/subsubdir", NULL, 0755, GIT_MKDIR_PATH);
	cl_git_mkfile("./testrepo/dir/one", "one\n");
	cl_git_mkfile("./testrepo/dir/subdir/two", "two\n");

	cl_assert_equal_i(true, git_path_isdir("./testrepo/dir/subdir/subsubdir"));

	g_opts.checkout_strategy |= GIT_CHECKOUT_REMOVE_UNTRACKED;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/dir"));
}

void test_checkout_index__honor_the_specified_pathspecs(void)
{
	char *entries[] = { "*.txt" };

	g_opts.paths.strings = entries;
	g_opts.paths.count = 1;

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

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
	const char *attributes =
		"branch_file.txt text eol=crlf\n"
		"new.txt text eol=lf\n";

	cl_git_mkfile("./testrepo/.gitattributes", attributes);
	set_core_autocrlf_to(false);

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n");
}

void test_checkout_index__honor_coreautocrlf_setting_set_to_true(void)
{
#ifdef GIT_WIN32
	const char *expected_readme_text = "hey there\r\n";

	cl_git_pass(p_unlink("./testrepo/.gitattributes"));
	set_core_autocrlf_to(true);

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/README", expected_readme_text);
#endif
}

static void set_repo_symlink_handling_cap_to(bool value)
{
	set_config_entry_to("core.symlinks", value);
}

void test_checkout_index__honor_coresymlinks_setting_set_to_true(void)
{
	set_repo_symlink_handling_cap_to(true);

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

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
	set_repo_symlink_handling_cap_to(false);

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
}

void test_checkout_index__donot_overwrite_modified_file_by_default(void)
{
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	/* set this up to not return an error code on conflicts, but it
	 * still will not have permission to overwrite anything...
	 */
	g_opts.checkout_strategy = GIT_CHECKOUT_ALLOW_CONFLICTS;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/new.txt", "This isn't what's stored!");
}

void test_checkout_index__can_overwrite_modified_file(void)
{
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	g_opts.checkout_strategy |= GIT_CHECKOUT_UPDATE_MODIFIED;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__options_disable_filters(void)
{
	cl_git_mkfile("./testrepo/.gitattributes", "*.txt text eol=crlf\n");

	g_opts.disable_filters = false;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/new.txt", "my new file\r\n");

	p_unlink("./testrepo/new.txt");

	g_opts.disable_filters = true;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_index__options_dir_modes(void)
{
#ifndef GIT_WIN32
	struct stat st;
	git_oid oid;
	git_commit *commit;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/dir"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));

	reset_index_to_treeish((git_object *)commit);

	g_opts.dir_mode = 0701;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

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
	struct stat st;

	g_opts.file_mode = 0700;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	cl_git_pass(p_stat("./testrepo/new.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0700);
#endif
}

void test_checkout_index__options_open_flags(void)
{
	cl_git_mkfile("./testrepo/new.txt", "hi\n");

	g_opts.file_open_flags = O_CREAT | O_RDWR | O_APPEND;

	g_opts.checkout_strategy |= GIT_CHECKOUT_UPDATE_MODIFIED;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	test_file_contents("./testrepo/new.txt", "hi\nmy new file\n");
}

struct conflict_data {
	const char *file;
	const char *sha;
};

static int conflict_cb(
	const char *conflict_file,
	const git_oid *blob_oid,
	unsigned int index_mode,
	unsigned int wd_mode,
	void *payload)
{
	struct conflict_data *expectations = (struct conflict_data *)payload;

	GIT_UNUSED(index_mode);
	GIT_UNUSED(wd_mode);

	cl_assert_equal_s(expectations->file, conflict_file);
	cl_assert_equal_i(0, git_oid_streq(blob_oid, expectations->sha));

	return 0;
}

void test_checkout_index__can_notify_of_skipped_files(void)
{
	struct conflict_data data;

	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");

	/*
	 * $ git ls-tree HEAD
	 * 100644 blob a8233120f6ad708f843d861ce2b7228ec4e3dec6    README
	 * 100644 blob 3697d64be941a53d4ae8f6a271e4e3fa56b022cc    branch_file.txt
	 * 100644 blob a71586c1dfe8a71c6cbf6c129f404c5642ff31bd    new.txt
	 */
	data.file = "new.txt";
	data.sha = "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd";

	g_opts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;
	g_opts.conflict_cb = conflict_cb;
	g_opts.conflict_payload = &data;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));
}

static int dont_conflict_cb(
	const char *conflict_file,
	const git_oid *blob_oid,
	unsigned int index_mode,
	unsigned int wd_mode,
	void *payload)
{
	GIT_UNUSED(conflict_file);
	GIT_UNUSED(blob_oid);
	GIT_UNUSED(index_mode);
	GIT_UNUSED(wd_mode);
	GIT_UNUSED(payload);

	cl_assert(false);

	return 0;
}

void test_checkout_index__wont_notify_of_expected_line_ending_changes(void)
{
	cl_git_pass(p_unlink("./testrepo/.gitattributes"));
	set_core_autocrlf_to(true);

	cl_git_mkfile("./testrepo/new.txt", "my new file\r\n");

	g_opts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;
	g_opts.conflict_cb = dont_conflict_cb;
	g_opts.conflict_payload = NULL;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));
}

static void progress(const char *path, size_t cur, size_t tot, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	*was_called = true;
}

void test_checkout_index__calls_progress_callback(void)
{
	bool was_called = 0;
	g_opts.progress_cb = progress;
	g_opts.progress_payload = &was_called;

	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));
	cl_assert_equal_i(was_called, true);
}

void test_checkout_index__can_overcome_name_clashes(void)
{
	git_index *index;

	cl_git_pass(git_repository_index(&index, g_repo));
	git_index_clear(index);

	cl_git_mkfile("./testrepo/path0", "content\r\n");
	cl_git_pass(p_mkdir("./testrepo/path1", 0777));
	cl_git_mkfile("./testrepo/path1/file1", "content\r\n");

	cl_git_pass(git_index_add_from_workdir(index, "path0"));
	cl_git_pass(git_index_add_from_workdir(index, "path1/file1"));


	cl_git_pass(p_unlink("./testrepo/path0"));
	cl_git_pass(git_futils_rmdir_r(
		"./testrepo/path1", NULL, GIT_RMDIR_REMOVE_FILES));

	cl_git_mkfile("./testrepo/path1", "content\r\n");
	cl_git_pass(p_mkdir("./testrepo/path0", 0777));
	cl_git_mkfile("./testrepo/path0/file0", "content\r\n");

	cl_assert(git_path_isfile("./testrepo/path1"));
	cl_assert(git_path_isfile("./testrepo/path0/file0"));

	g_opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	cl_assert(git_path_isfile("./testrepo/path1"));
	cl_assert(git_path_isfile("./testrepo/path0/file0"));

	g_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_checkout_index(g_repo, NULL, &g_opts));

	cl_assert(git_path_isfile("./testrepo/path0"));
	cl_assert(git_path_isfile("./testrepo/path1/file1"));

	git_index_free(index);
}
