#include "clar.h"
#include "clar_libgit2.h"

#include "git2/errors.h"
#include "git2/index.h"

static git_repository *g_repo = NULL;
static git_index *g_index = NULL;

static const char test_ext_signature[4] = {'T', 'E', 'S', 'T'};
static const char test_ext_data1[] = "This data is for testing purposes ONLY.";
static const char test_ext_data2[] = "This data has been overwritten.";

void test_index_extension__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
	cl_git_pass(git_repository_index(&g_index, g_repo));

}

void test_index_extension__cleanup(void)
{
	git_index_free(g_index);
	cl_git_sandbox_cleanup();
	g_repo = NULL;

	cl_git_pass(git_libgit2_opts(GIT_OPT_ENABLE_STRICT_OBJECT_CREATION, 1));
}

void test_index_extension__nonexistant(void)
{
	git_buf ext;

	cl_git_fail_with(GIT_ENOTFOUND, git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_git_fail_with(GIT_ENOTFOUND, git_index_extension_remove(g_index, test_ext_signature));
}

void test_index_extension__add(void)
{
	git_buf ext;

	cl_git_pass(git_index_extension_add(g_index, test_ext_signature, test_ext_data1, sizeof(test_ext_data1), false));
	cl_git_pass(git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_assert_equal_strn(test_ext_data1, ext.ptr, sizeof(test_ext_data1));
}

void test_index_extension__overwrite(void)
{
	git_buf ext;

	cl_git_pass(git_index_extension_add(g_index, test_ext_signature, test_ext_data1, sizeof(test_ext_data1), false));
	cl_git_pass(git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_assert_equal_strn(test_ext_data1, ext.ptr, sizeof(test_ext_data1));

	cl_git_fail_with(GIT_EEXISTS, git_index_extension_add(g_index, test_ext_signature, test_ext_data2, sizeof(test_ext_data2), false));

	cl_git_pass(git_index_extension_add(g_index, test_ext_signature, test_ext_data2, sizeof(test_ext_data2), true));
	cl_git_pass(git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_assert_equal_strn(test_ext_data2, ext.ptr, sizeof(test_ext_data2));
}

void test_index_extension__remove(void)
{
	git_buf ext;

	cl_git_pass(git_index_extension_add(g_index, test_ext_signature, test_ext_data1, sizeof(test_ext_data1), false));
	cl_git_pass(git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_assert_equal_strn(test_ext_data1, ext.ptr, sizeof(test_ext_data1));

	cl_git_pass(git_index_extension_remove(g_index, test_ext_signature));
	cl_git_fail_with(GIT_ENOTFOUND, git_index_extension_get(&ext, g_index, test_ext_signature));
}

void test_index_extension__write_read(void)
{
	git_buf ext;

	cl_git_pass(git_index_extension_add(g_index, test_ext_signature, test_ext_data1, sizeof(test_ext_data1), false));

	cl_git_pass(git_index_write(g_index));
	git_index_clear(g_index);

	cl_git_pass(git_index_read(g_index, true));

	cl_git_pass(git_index_extension_get(&ext, g_index, test_ext_signature));
	cl_assert_equal_strn(test_ext_data1, ext.ptr, sizeof(test_ext_data1));
}
