#include "clar_libgit2.h"
#include "buffer.h"

extern void assert_describe(
	const char *expected_output,
	const char *revparse_spec,
	git_repository *repo,
	git_describe_opts *opts,
	git_describe_format_options *fmt_opts,
	bool is_prefix_match);

extern void assert_describe_workdir(
	const char *expected_output,
	const char *expected_suffix,
	git_repository *repo,
	git_describe_opts *opts,
	git_describe_format_options *fmt_opts,
	bool is_prefix_match);
