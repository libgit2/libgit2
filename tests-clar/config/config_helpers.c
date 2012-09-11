#include "clar_libgit2.h"
#include "config_helpers.h"
#include "repository.h"

void assert_config_entry_existence(
	git_repository *repo,
	const char *name,
	bool is_supposed_to_exist)
{
	git_config *config;
	const char *out;
	int result;

	cl_git_pass(git_repository_config__weakptr(&config, repo));
	
	result = git_config_get_string(&out, config, name);

	if (is_supposed_to_exist)
		cl_git_pass(result);
	else
		cl_assert_equal_i(GIT_ENOTFOUND, result);
}
