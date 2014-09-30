#include "describe_helpers.h"

void assert_describe(
	const char *expected_output,
	const char *revparse_spec,
	git_repository *repo,
	git_describe_opts *opts,
	bool is_prefix_match)
{
	git_object *object;
	git_buf label;

	cl_git_pass(git_revparse_single(&object, repo, revparse_spec));

	cl_git_pass(git_describe_commit(&label, object, opts));

	if (is_prefix_match)
		cl_assert_equal_i(0, git__prefixcmp(git_buf_cstr(&label), expected_output));
	else
		cl_assert_equal_s(expected_output, label);

	git_object_free(object);
	git_buf_free(&label);
}
