#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"


static git_repository *g_repo;

void test_checkout_checkout__initialize(void)
{
	const char *attributes = "* text eol=lf\n";

	g_repo = cl_git_sandbox_init("testrepo");
	cl_git_mkfile("./testrepo/.gitattributes", attributes);
}

void test_checkout_checkout__cleanup(void)
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


void test_checkout_checkout__bare(void)
{
	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_fail(git_checkout_head(g_repo, NULL, NULL));
}

void test_checkout_checkout__default(void)
{
	cl_git_pass(git_checkout_head(g_repo, NULL, NULL));
	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}


void test_checkout_checkout__crlf(void)
{
	const char *attributes =
		"branch_file.txt text eol=crlf\n"
		"new.txt text eol=lf\n";
	git_config *cfg;

	cl_git_pass(git_repository_config__weakptr(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "core.autocrlf", false));
	cl_git_mkfile("./testrepo/.gitattributes", attributes);

	cl_git_pass(git_checkout_head(g_repo, NULL, NULL));
	test_file_contents("./testrepo/README", "hey there\n"); 
	test_file_contents("./testrepo/new.txt", "my new file\n"); 
	test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n"); 
}


void test_checkout_checkout__win32_autocrlf(void)
{
#ifdef GIT_WIN32
	git_config *cfg;
	const char *expected_readme_text = "hey there\r\n";

	cl_must_pass(p_unlink("./testrepo/.gitattributes"));
	cl_git_pass(git_repository_config__weakptr(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "core.autocrlf", true));

	cl_git_pass(git_checkout_head(g_repo, NULL, NULL));
	test_file_contents("./testrepo/README", expected_readme_text); 
#endif
}


static void enable_symlinks(bool enable)
{
	git_config *cfg;
	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "core.symlinks", enable));
	git_config_free(cfg);
}

void test_checkout_checkout__symlinks(void)
{
	/* First try with symlinks forced on */
	enable_symlinks(true);
	cl_git_pass(git_checkout_head(g_repo, NULL, NULL));

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

	/* Now with symlinks forced off */
	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("testrepo");
	enable_symlinks(false);
	cl_git_pass(git_checkout_head(g_repo, NULL, NULL));

	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
}

void test_checkout_checkout__existing_file_skip(void)
{
	git_checkout_opts opts = {0};
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");
	opts.existing_file_action = GIT_CHECKOUT_SKIP_EXISTING;
	cl_git_pass(git_checkout_head(g_repo, &opts, NULL));
	test_file_contents("./testrepo/new.txt", "This isn't what's stored!");
}

void test_checkout_checkout__existing_file_overwrite(void)
{
	git_checkout_opts opts = {0};
	cl_git_mkfile("./testrepo/new.txt", "This isn't what's stored!");
	opts.existing_file_action = GIT_CHECKOUT_OVERWRITE_EXISTING;
	cl_git_pass(git_checkout_head(g_repo, &opts, NULL));
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_checkout__disable_filters(void)
{
	git_checkout_opts opts = {0};
	cl_git_mkfile("./testrepo/.gitattributes", "*.txt text eol=crlf\n");
	/* TODO cl_git_pass(git_checkout_head(g_repo, &opts, NULL));*/
	/* TODO test_file_contents("./testrepo/new.txt", "my new file\r\n");*/
	opts.disable_filters = true;
	cl_git_pass(git_checkout_head(g_repo, &opts, NULL));
	test_file_contents("./testrepo/new.txt", "my new file\n");
}

void test_checkout_checkout__dir_modes(void)
{
#ifndef GIT_WIN32
	git_checkout_opts opts = {0};
	struct stat st;
	git_reference *ref;

	cl_git_pass(git_reference_lookup(&ref, g_repo, "refs/heads/dir"));

	opts.dir_mode = 0701;
	cl_git_pass(git_checkout_reference(ref, &opts, NULL));
	cl_git_pass(p_stat("./testrepo/a", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0701);

	/* File-mode test, since we're on the 'dir' branch */
	cl_git_pass(p_stat("./testrepo/a/b.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0755);

	git_reference_free(ref);
#endif
}

void test_checkout_checkout__override_file_modes(void)
{
#ifndef GIT_WIN32
	git_checkout_opts opts = {0};
	struct stat st;

	opts.file_mode = 0700;
	cl_git_pass(git_checkout_head(g_repo, &opts, NULL));
	cl_git_pass(p_stat("./testrepo/new.txt", &st));
	cl_assert_equal_i(st.st_mode & 0777, 0700);
#endif
}

void test_checkout_checkout__open_flags(void)
{
	git_checkout_opts opts = {0};

	cl_git_mkfile("./testrepo/new.txt", "hi\n");
	opts.file_open_flags = O_CREAT | O_RDWR | O_APPEND;
	cl_git_pass(git_checkout_head(g_repo, &opts, NULL));
	test_file_contents("./testrepo/new.txt", "hi\nmy new file\n");
}

void test_checkout_checkout__detached_head(void)
{
	/* TODO: write this when git_checkout_commit is implemented. */
}
