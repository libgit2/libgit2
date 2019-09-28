#include "clar_libgit2.h"
#include "fileops.h"

void test_clone_shallow__initialize(void)
{

}

void test_clone_shallow__cleanup(void)
{
	cl_git_sandbox_cleanup();
}


#define CLONE_DEPTH 5

void test_clone_shallow__clone_depth(void)
{
	git_buf path = GIT_BUF_INIT;
	git_repository *repo;
	git_revwalk *walk;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_oid oid;
	git_oidarray roots;
	size_t depth = 0;
	int error = 0;

	clone_opts.fetch_opts.depth = CLONE_DEPTH;

	git_buf_joinpath(&path, clar_sandbox_path(), "shallowclone");

	cl_git_pass(git_clone(&repo, "https://github.com/libgit2/TestGitRepository", git_buf_cstr(&path), &clone_opts));

	cl_assert_equal_b(true, git_repository_is_shallow(repo));

	cl_git_pass(git_repository_shallow_roots(&roots, repo));
	cl_assert_equal_i(1, roots.count);
	cl_assert_equal_s("83834a7afdaa1a1260568567f6ad90020389f664", git_oid_tostr_s(&roots.ids[0]));

	git_revwalk_new(&walk, repo);

	git_revwalk_push_head(walk);

	while ((error = git_revwalk_next(&oid, walk)) == GIT_OK) {
		if (depth + 1 > CLONE_DEPTH)
			cl_fail("expected depth mismatch");
		depth++;
	}

	cl_git_pass(error);

	git_buf_dispose(&path);
	git_revwalk_free(walk);
	git_repository_free(repo);
}
