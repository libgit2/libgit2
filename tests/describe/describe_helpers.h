#include "clar_libgit2.h"
#include "buffer.h"

extern void assert_describe(
	const char *expected_output,
	const char *revparse_spec,
	git_repository *repo,
	git_describe_opts *opts,
	bool is_prefix_match);
