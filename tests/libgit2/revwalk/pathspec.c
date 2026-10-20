#include "clar_libgit2.h"
#include "pathspec_helpers.h"

static git_repository *repo;

void test_revwalk_pathspec__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_revwalk_pathspec__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_revwalk_pathspec__cases(void)
{
	pathspec_run_cases(repo);
}

/**
 * Changing the pathspec mid-walk resets iteration and applies the new
 * filter from scratch, rather than mixing results from both filters.
 */
void test_revwalk_pathspec__change_pathspec_mid_walk(void)
{
	git_revwalk *walk;
	git_pathspec *ps_wildcard = NULL, *ps_exact = NULL;
	git_oid id, expected[2];
	int i, error;
	char *wildcard_path = "*.txt";
	char *exact_path = "README";
	git_strarray wildcard_paths = { NULL, 1 };
	git_strarray exact_paths = { NULL, 1 };
	wildcard_paths.strings = &wildcard_path;
	exact_paths.strings = &exact_path;

	for (i = 0; i < 2; i++)
		git_oid_from_string(&expected[i], pathspec_expected_exact_str[i], GIT_OID_SHA1);

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps_wildcard, &wildcard_paths));
	cl_git_pass(git_pathspec_new(&ps_exact, &exact_paths));

	cl_git_pass(git_revwalk_pathspec(walk, ps_wildcard));
	cl_git_pass(git_revwalk_push_head(walk));
	cl_git_pass(git_revwalk_next(&id, walk));

	cl_git_pass(git_revwalk_pathspec(walk, ps_exact));
	cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected[i], &id);
		i++;
	}

	cl_assert_equal_i(i, 2);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps_wildcard);
	git_pathspec_free(ps_exact);
}

/**
 * git_revwalk_pathspec(walk, NULL) clears a previously set pathspec, so a
 * subsequent walk returns the full, unfiltered history again.
 */
void test_revwalk_pathspec__clear_pathspec(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected[7];
	int i, error;
	char *path = "README";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	for (i = 0; i < 7; i++)
		git_oid_from_string(&expected[i], pathspec_expected_all_str[i], GIT_OID_SHA1);

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));

	cl_git_pass(git_revwalk_pathspec(walk, NULL));
	cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected[i], &id);
		i++;
	}

	cl_assert_equal_i(i, 7);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

/**
 * Verify that we don't error out looking for bloom filter in a repo without
 * commit graph
 */
void test_revwalk_pathspec__no_commit_graph(void)
{
	git_repository *bare_repo;
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	char *path = "README";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	cl_git_pass(git_repository_open(&bare_repo, cl_fixture("empty_bare.git")));

	cl_git_pass(git_revwalk_new(&walk, bare_repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));

	cl_git_pass(git_revwalk_pathspec(walk, ps));

	git_revwalk_free(walk);
	git_pathspec_free(ps);
	git_repository_free(bare_repo);
}
