

#include "clar_libgit2.h"
#include "csvtoyaml.h"
#include <stdio.h>

static git_repository *g_repo = NULL;
static git_textconv *yaml_filter;

void test_textconv_registry__initialize(void)
{
	yaml_filter = create_csv_to_yaml_textconv();
	cl_git_pass(git_textconv_register("csv2yaml", yaml_filter));
	g_repo = cl_git_sandbox_init("empty_standard_repo");
}

void test_textconv_registry__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;

	git_textconv_unregister("csv2yaml");
	git__free(yaml_filter);
}

void test_textconv_registry__lookup(void)
{
	cl_assert_equal_p(git_textconv_lookup("csv2yaml"), yaml_filter);
}
