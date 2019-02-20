#include "clar_libgit2.h"

#include "attr_file.h"
#include "git2/sys/repository.h"

static git_repository *repo;

void test_attr_macro__initialize(void)
{
	cl_git_pass(git_repository_new(&repo));
}

void test_attr_macro__cleanup(void)
{
	git_repository_free(repo);
}

static void assert_attr_expands(const char *attr, const char *expected)
{
	git_attr_file_entry entry = { { NULL }, ".gitattributes" };
	git_buf attrbuf = GIT_BUF_INIT;
	git_attr_file *attrs;
	const char *value;

	cl_git_pass(git_buf_printf(&attrbuf, "example.txt %s\n", attr));
	cl_git_pass(git_attr_file__new(&attrs, &entry, GIT_ATTR_FILE__IN_MEMORY));
	cl_git_pass(git_attr_file__parse_buffer(repo, attrs, attrbuf.ptr));

	cl_git_pass(git_attr_get(&value, repo, 0, "example.txt", attr));
	cl_assert_equal_s(value, expected);

	git_attr_file__free(attrs);
	git_buf_dispose(&attrbuf);
}

void test_attr_macro__adding_macro_succeeds(void)
{
	cl_git_pass(git_attr_add_macro(repo, "foo", "test"));
	assert_attr_expands("foo", "test");
}

void test_attr_macro__adding_negative_macro_succeeds(void)
{
	cl_git_pass(git_attr_add_macro(repo, "foo", "-test"));
	assert_attr_expands("foo", "-text");
}

void test_attr_macro__redefining_macro_succeeds(void)
{
	cl_git_pass(git_attr_add_macro(repo, "foo", "foo"));
	cl_git_pass(git_attr_add_macro(repo, "foo", "bar"));
}

void test_attr_macro__recursive_macro_resolves(void)
{
	cl_git_pass(git_attr_add_macro(repo, "foo", "bar"));
	cl_git_pass(git_attr_add_macro(repo, "bar", "-text"));
}
