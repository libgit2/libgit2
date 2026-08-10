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
