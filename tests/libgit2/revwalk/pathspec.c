#include "clar_libgit2.h"

static git_repository *repo;

void test_revwalk_pathspec__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_revwalk_pathspec__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

/**
 * $ git log -- README
 * Lists commits 
 * 4a202b346bb0fb0db7eff3cffeb3c70babbd2045
 * 8496071c1b46c854b31185ea97743be6a8774479
 */
static const char *expected_exact_str[] = {
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045",
	"8496071c1b46c854b31185ea97743be6a8774479",
};

void test_revwalk_pathspec__exact_file(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected[2];
	int i, error;
	char *path = "README";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	for (i = 0; i < 2; i++) {
		git_oid_from_string(&expected[i], expected_exact_str[i], GIT_OID_SHA1);
	}

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
	cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected[i], &id);
		i++;
	}

	cl_assert_equal_i(i, 2);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

/**
 * $ git log -- *.txt
 * Lists commits 
 * a65fedf39aefe402d3bb6e24df4d4f5fe4547750
 * be3563ae3f795b2b4353bcce3a527ad0a4f7f644
 * c47800c7266a2be04c571c04d5a6614691ea99bd
 * 9fd738e8f7967c078dceed8190330fc8648ee56a
 * 5b5b025afb0b4c913b4c338a42934a3863bf3644
 */
static const char *expected_wildcard_str[] = {
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750",
	"be3563ae3f795b2b4353bcce3a527ad0a4f7f644",
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
	"9fd738e8f7967c078dceed8190330fc8648ee56a",
	"5b5b025afb0b4c913b4c338a42934a3863bf3644",
};

void test_revwalk_pathspec__wildcard(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected[5];
	int i, error;
	char *path = "*.txt";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	for (i = 0; i < 5; i++) {
		git_oid_from_string(&expected[i], expected_wildcard_str[i], GIT_OID_SHA1);
	}

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
	cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected[i], &id);
		i++;
	}

	cl_assert_equal_i(i, 5);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

/**
 * $ git log --oneline HEAD -- does-not-exist.xyz
 * (empty)
 */
void test_revwalk_pathspec__no_match(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id;
	int i, error;
	char *path = "does-not-exist.xyz";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
	cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0)
		i++;

	cl_assert_equal_i(i, 0);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

/**
 * $ git log --oneline subtrees -- ab
 * Lists commit
 * 763d71aadf09a7951596c9746c024e7eece7c7af
 */
void test_revwalk_pathspec__directory(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected;
	int i, error;
	char *path = "ab";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	git_oid_from_string(&expected, "763d71aadf09a7951596c9746c024e7eece7c7af", GIT_OID_SHA1);

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/heads/subtrees"));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected, &id);
		i++;
	}

	cl_assert_equal_i(i, 1);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

/**
 * $ git log --oneline HEAD -- README '*.txt'
 * Lists the union of the exact_file and wildcard commits
 */
static const char *expected_multiple_str[] = {
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750",
	"be3563ae3f795b2b4353bcce3a527ad0a4f7f644",
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
	"9fd738e8f7967c078dceed8190330fc8648ee56a",
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045",
	"5b5b025afb0b4c913b4c338a42934a3863bf3644",
	"8496071c1b46c854b31185ea97743be6a8774479",
};

void test_revwalk_pathspec__multiple_patterns(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected[7];
	int i, error;
	char *paths_arr[2] = { "README", "*.txt" };
	git_strarray paths = { NULL, 2 };
	paths.strings = paths_arr;

	for (i = 0; i < 7; i++)
		git_oid_from_string(&expected[i], expected_multiple_str[i], GIT_OID_SHA1);

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
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
 * be3563a "Merge branch 'br2'" only touches branch_file.txt relative to one
 * of its two parents (it is unchanged versus c47800c). It must not show up
 * when walking history for that path, since a merge only counts as a change
 * when it differs from *every* parent.
 *
 * $ git log --oneline refs/remotes/test/master -- branch_file.txt
 * Lists commit
 * c47800c7266a2be04c571c04d5a6614691ea99bd
 */
void test_revwalk_pathspec__merge_commit_single_parent_touches(void)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected;
	int i, error;
	char *path = "branch_file.txt";
	git_strarray paths = { NULL, 1 };
	paths.strings = &path;

	git_oid_from_string(&expected, "c47800c7266a2be04c571c04d5a6614691ea99bd", GIT_OID_SHA1);

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/remotes/test/master"));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		cl_assert_equal_oid(&expected, &id);
		i++;
	}

	cl_assert_equal_i(i, 1);
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
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
		git_oid_from_string(&expected[i], expected_exact_str[i], GIT_OID_SHA1);

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
 *
 * $ git rev-list --count HEAD
 * 7
 */
static const char *expected_all_str[] = {
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750",
	"be3563ae3f795b2b4353bcce3a527ad0a4f7f644",
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
	"9fd738e8f7967c078dceed8190330fc8648ee56a",
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045",
	"5b5b025afb0b4c913b4c338a42934a3863bf3644",
	"8496071c1b46c854b31185ea97743be6a8774479",
};

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
		git_oid_from_string(&expected[i], expected_all_str[i], GIT_OID_SHA1);

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
