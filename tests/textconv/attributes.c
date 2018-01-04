

#include "clar_libgit2.h"
#include "csvtoyaml.h"
#include "testdata.h"
#include <stdio.h>

static git_repository *g_repo = NULL;
static git_textconv *yaml_filter;
static git_oid id_csv;

void test_textconv_attributes__initialize(void)
{
	yaml_filter = create_csv_to_yaml_textconv();
	cl_git_pass(git_textconv_register("csv2yaml", yaml_filter));
	g_repo = cl_git_sandbox_init("textconv");
	cl_git_mkfile("textconv/test.csv", getTestCSV());
	cl_git_pass(git_blob_create_fromworkdir(&id_csv, g_repo, "test.csv"));
}

void test_textconv_attributes__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
	git_textconv_unregister("csv2yaml");
	git__free(yaml_filter);
}

void test_textconv_attributes__check(void)
{
	git_diff_driver* driver;
	git_textconv* tc;

	cl_check_pass(git_diff_driver_lookup(&driver, g_repo, "test.csv"));
	cl_check_pass(git_textconv_load(&tc, driver));
	cl_assert_equal_p(yaml_filter,tc);
	cl_check_pass(git_diff_driver_lookup(&driver, g_repo, "test.dat"));
	cl_check_pass(git_textconv_load(&tc, driver));
	cl_assert_equal_p(yaml_filter,tc);
	cl_check_pass(git_diff_driver_lookup(&driver, g_repo, "abc.txt"));
	cl_assert_equal_i(GIT_PASSTHROUGH, git_textconv_load(&tc, driver));
	cl_assert_equal_p(NULL,tc);
}

