

#include "clar_libgit2.h"
#include "csvtoyaml.h"
#include "testdata.h"
#include <stdio.h>

static git_repository *g_repo = NULL;
static git_textconv *yaml_filter;
static git_oid id_csv;

void test_textconv_apply__initialize(void)
{
	yaml_filter = create_csv_to_yaml_textconv();
	cl_git_pass(git_textconv_register("csv2yaml", yaml_filter));
	g_repo = cl_git_sandbox_init("empty_standard_repo");
	cl_git_mkfile("empty_standard_repo/test.csv", getTestCSV());
	cl_git_pass(git_blob_create_fromworkdir(&id_csv, g_repo, "test.csv"));
}

void test_textconv_apply__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;

	git_textconv_unregister("csv2yaml");
	git__free(yaml_filter);
}

void test_textconv_apply__blob(void)
{
	git_blob *blob = NULL;
	git_buf out = GIT_BUF_INIT;

	cl_git_pass(git_blob_lookup(&blob, g_repo, &id_csv));
	cl_git_pass(git_filter_textconv_apply_to_blob(&out, NULL, yaml_filter, blob));
	cl_assert_equal_s(getTestYAML(), out.ptr);
	git_blob_free(blob);
	git_buf_free(&out);
}

void test_textconv_apply__file(void)
{
	git_buf out = GIT_BUF_INIT;

	cl_git_pass(git_filter_textconv_apply_to_file(&out, NULL, yaml_filter, g_repo, "test.csv"));
	cl_assert_equal_s(getTestYAML(), out.ptr);
	git_buf_free(&out);
}
