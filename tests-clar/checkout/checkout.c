#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"

#define DO_LOCAL_TEST 0
#define DO_LIVE_NETWORK_TESTS 1
#define LIVE_REPO_URL "http://github.com/libgit2/node-gitteh"


static git_repository *g_repo;

void test_checkout_checkout__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
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
	cl_assert(fd);
	cl_assert_equal_i(p_read(fd, buffer, 1024), strlen(expectedcontents));
	cl_assert_equal_s(expectedcontents, buffer);
	cl_git_pass(p_close(fd));
}


void test_checkout_checkout__bare(void)
{
	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_fail(git_checkout_force(g_repo, NULL));
}

void test_checkout_checkout__default(void)
{
	cl_git_pass(git_checkout_force(g_repo, NULL));
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
	cl_git_pass(git_checkout_force(g_repo, NULL));
	test_file_contents("./testrepo/README", "hey there\n");
	test_file_contents("./testrepo/new.txt", "my new file\n");
	test_file_contents("./testrepo/branch_file.txt", "hi\r\nbye!\r\n");
}

void test_checkout_checkout__stats(void)
{
	/* TODO */
}
