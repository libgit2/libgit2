#include <git2/strarray.h>
#include <util.h>
#include "git2/sparse.h"

int git_sparse_checkout_set_default(git_repository *repo) {

	int error = 0;

	char *default_pattern__strings[] = { "/*", "!/*/" };
	git_strarray default_patterns = {default_pattern__strings, ARRAY_SIZE(default_pattern__strings) };

	error = git_sparse_checkout_set(&default_patterns, repo);
	return error;
}