#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"


static git_repository *g_repo;

void test_checkout_checkout__initialize(void)
{
	const char *attributes = "*.txt text eol=cr\n";

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

	fd = p_open(path, O_RDONLY);
	cl_assert(fd >= 0);

	cl_assert_equal_i(p_read(fd, buffer, 1024), strlen(expectedcontents));
	cl_assert_equal_s(expectedcontents, buffer);
	cl_git_pass(p_close(fd));
}


void test_checkout_checkout__bare(void)
{
	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_fail(git_checkout_index(g_repo, NULL));
}

void test_checkout_checkout__default(void)
{
	cl_git_pass(git_checkout_index(g_repo, NULL));
	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\nbye!\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
}


void test_checkout_checkout__crlf(void)
{
	const char *attributes =
		"branch_file.txt text eol=crlf\n"
		"README text eol=cr\n"
		"new.txt text eol=lf\n";
	cl_git_mkfile("./testrepo/.gitattributes", attributes);
	cl_git_pass(git_checkout_index(g_repo, NULL));
	/* test_file_contents("./testrepo/README", "hey there\n"); */
	/* test_file_contents("./testrepo/new.txt", "my new file\n"); */
	/* test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n"); */
}

void test_checkout_checkout__stats(void)
{
	/* TODO */
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
	cl_git_pass(git_checkout_index(g_repo, NULL));

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
	cl_git_pass(git_checkout_index(g_repo, NULL));

	test_file_contents("./testrepo/link_to_new.txt", "new.txt");
}
