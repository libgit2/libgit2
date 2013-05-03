#include "clar_libgit2.h"

#include "tag.h"

static git_repository *g_repo;

void test_object_tag_lookup_byname__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tag_lookup_byname__cleanup(void)
{
	cl_git_sandbox_cleanup();
}


const char* existing_tags[] = {
	"e90810b",
	"foo/bar",
	"foo/foo/bar",
	"test",
	NULL};

void test_object_tag_lookup_byname__lookup_existing(void)
{
	const char** tag;
	
	for (tag = existing_tags; *tag; ++tag)
	{
		git_reference* out;

		cl_git_pass(git_tag_lookup_byname(&out, g_repo, *tag));

		git_reference_free(out);
	}
}

struct tag_name_expected_t
{
	const char* name;
	const int expected_ret_value;
};

static const struct tag_name_expected_t tags_with_errors[] = {
	/* Normal non existing tags */
	{ "non_existing_tag", GIT_ENOTFOUND },
	{ "bar", GIT_ENOTFOUND },
	{ "{}", GIT_ENOTFOUND },
	{ "---", GIT_ENOTFOUND },
	{ "HEAD", GIT_ENOTFOUND },
	{ "a///b", GIT_ENOTFOUND },
	/* Invalid tag names*/
	{ "", GIT_EINVALIDSPEC },
	{ "^", GIT_EINVALIDSPEC },
	{ "/", GIT_EINVALIDSPEC },
	{ "a///b/", GIT_EINVALIDSPEC },
	/* Generates other errors */
	{ "foo/foo", GIT_ERROR },
	{ "foo", GIT_ERROR },
	{ NULL }};


void test_object_tag_lookup_byname__lookup_non_existing(void)
{
	size_t i;

	for (i = 0; tags_with_errors[i].name; i++)
	{
		git_reference* out;
		int error = git_tag_lookup_byname(&out, g_repo, tags_with_errors[i].name);
		
		if (error != tags_with_errors[i].expected_ret_value)
		{
			cl_git_report_failure(error, __FILE__, __LINE__, tags_with_errors[i].name);
		}
		if (error == 0)
		{
			git_reference_free(out);
		}
	}
}
