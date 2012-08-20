#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"


static git_repository *g_repo;
static git_object *g_treeish;
static git_checkout_opts g_opts;

void test_checkout_tree__initialize(void)
{
	memset(&g_opts, 0, sizeof(g_opts));

	g_repo = cl_git_sandbox_init("testrepo");
	
	cl_git_rewritefile(
		"./testrepo/.gitattributes",
		"* text eol=lf\n");

	cl_git_pass(git_repository_head_tree((git_tree **)&g_treeish, g_repo));
}

void test_checkout_tree__cleanup(void)
{
	git_object_free(g_treeish);
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

void test_checkout_tree__cannot_checkout_a_bare_repository(void)
{
	test_checkout_tree__cleanup();

	memset(&g_opts, 0, sizeof(g_opts));
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_head_tree((git_tree **)&g_treeish, g_repo));

	cl_git_fail(git_checkout_tree(g_repo, g_treeish, NULL, NULL));
}

void test_checkout_tree__update_the_content_of_workdir_with_missing_files(void)
{
	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, NULL, NULL));

	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_tree__honor_the_specified_pathspecs(void)
{
	git_strarray paths;
	char *entries[] = { "*.txt" };

	paths.strings = entries;
	paths.count = 1;
	g_opts.paths = &paths;

	cl_assert_equal_i(false, git_path_isfile("./testrepo/README"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/branch_file.txt"));
	cl_assert_equal_i(false, git_path_isfile("./testrepo/new.txt"));

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

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

void test_checkout_tree__honor_the_gitattributes_directives(void)
{
	const char *attributes =
		"branch_file.txt text eol=crlf\n"
		"new.txt text eol=lf\n";

	cl_git_mkfile("./testrepo/.gitattributes", attributes);
	set_core_autocrlf_to(false);

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, NULL, NULL));

	test_file_contents("./testrepo/README", "hey there\n"); 
	test_file_contents("./testrepo/new.txt", "my new file\n"); 
	test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n"); 
}

void test_checkout_tree__honor_coreautocrlf_setting_set_to_true(void)
{
#ifdef GIT_WIN32
	const char *expected_readme_text = "hey there\r\n";

	cl_git_pass(p_unlink("./testrepo/.gitattributes"));
	set_core_autocrlf_to(true);

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, NULL, NULL));

	test_file_contents("./testrepo/README", expected_readme_text); 
#endif
}

static void set_repo_symlink_handling_cap_to(bool value)
{
	set_config_entry_to("core.symlinks", value);
}

void test_checkout_tree__honor_coresymlinks_setting_set_to_true(void)
{
	set_repo_symlink_handling_cap_to(true);

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, NULL, NULL));

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

void test_checkout_tree__honor_coresymlinks_setting_set_to_false(void)
{
	set_repo_symlink_handling_cap_to(false);

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, NULL, NULL));

	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
}

void test_checkout_tree__options_skip_existing_file(void)
{
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");
	g_opts.existing_file_action = GIT_CHECKOUT_SKIP_EXISTING;

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	test_file_contents("./testrepo/new.txt", "This isn't what's stored!");
}

void test_checkout_tree__options_overwrite_existing_file(void)
{
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");
	g_opts.existing_file_action = GIT_CHECKOUT_OVERWRITE_EXISTING;

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_tree__options_disable_filters(void)
{
	cl_git_mkfile("./testrepo/.gitattributes", "*.txt text eol=crlf\n");

	g_opts.disable_filters = false;
	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	test_file_contents("./testrepo/new.txt", "my new file\r\n");

	p_unlink("./testrepo/new.txt");

	g_opts.disable_filters = true;
	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_tree__options_dir_modes(void)
{
#ifndef GIT_WIN32
	struct stat st;
	git_oid oid;
	git_commit *commit;

	cl_git_pass(git_reference_name_to_oid(&oid, g_repo, "refs/heads/dir"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));

	g_opts.dir_mode = 0701;
	cl_git_pass(git_checkout_tree(g_repo, (git_object *)commit, &g_opts, NULL));

	cl_git_pass(p_stat("./testrepo/a", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0701);

	/* File-mode test, since we're on the 'dir' branch */
	cl_git_pass(p_stat("./testrepo/a/b.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0755);

	git_commit_free(commit);
#endif
}

void test_checkout_tree__options_override_file_modes(void)
{
#ifndef GIT_WIN32
	struct stat st;

	g_opts.file_mode = 0700;

	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	cl_git_pass(p_stat("./testrepo/new.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0700);
#endif
}

void test_checkout_tree__options_open_flags(void)
{
	cl_git_mkfile("./testrepo/new.txt", "hi\n");

	g_opts.file_open_flags = O_CREAT | O_RDWR | O_APPEND;
	cl_git_pass(git_checkout_tree(g_repo, g_treeish, &g_opts, NULL));

	test_file_contents("./testrepo/new.txt", "hi\nmy new file\n");
}

void test_checkout_tree__cannot_checkout_a_non_treeish(void)
{
	git_oid oid;
	git_blob *blob;

	cl_git_pass(git_oid_fromstr(&oid, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));
	cl_git_pass(git_blob_lookup(&blob, g_repo, &oid));

	cl_git_fail(git_checkout_tree(g_repo, (git_object *)blob, NULL, NULL));

	git_blob_free(blob);
}
