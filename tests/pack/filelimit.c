#include "clar_libgit2.h"
#include <git2.h>

void test_pack_filelimit__open_repo_with_1025_packfiles(void)
{
	git_repository *repo;
	git_revwalk *walk;
	git_oid id;
	int i;

	/*
	 * This repository contains 1025 packfiles, each with one commit, one tree,
	 * and two blobs. The first blob (README.md) has the same content in all
	 * commits, but the second one (file.txt) has a different content in each
	 * commit.
	 */
	cl_git_pass(git_repository_open(&repo, cl_fixture("1025.git")));
	cl_git_pass(git_revwalk_new(&walk, repo));

	cl_git_pass(git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/heads/master"));

	/*
	 * Walking the tree requires opening each of the 1025 packfiles. This should
	 * work in all platforms, including those where the default limit of open
	 * file descriptors is small (e.g. 256 in macOS).
	 */
	i = 0;
	while (git_revwalk_next(&id, walk) == 0)
		++i;
	cl_assert_equal_i(1025, i);

	git_revwalk_free(walk);
	git_repository_free(repo);
}

