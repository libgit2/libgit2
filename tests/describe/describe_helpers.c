#include "describe_helpers.h"

void assert_describe(
	const char *expected_output,
	const char *revparse_spec,
	git_repository *repo,
	git_describe_opts *opts,
	git_describe_format_options *fmt_opts,
	bool is_prefix_match)
{
	git_object *object;
	git_buf label = GIT_BUF_INIT;
	git_describe_result *result;

	cl_git_pass(git_revparse_single(&object, repo, revparse_spec));

	cl_git_pass(git_describe_commit(&result, object, opts));
	cl_git_pass(git_describe_format(&label, result, fmt_opts));

	if (is_prefix_match)
		cl_assert_equal_i(0, git__prefixcmp(git_buf_cstr(&label), expected_output));
	else
		cl_assert_equal_s(expected_output, label);

	git_describe_result_free(result);
	git_object_free(object);
	git_buf_free(&label);
}
