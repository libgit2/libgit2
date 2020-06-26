#include "clar_libgit2.h"
#include "mwindow.h"
#include "global.h"
#include <git2.h>

extern git_mwindow_ctl git_mwindow__mem_ctl;
size_t mwindow_file_limit = 0;
size_t original_mwindow_file_limit = 0;

void test_pack_filelimit__initialize_tiny(void)
{
	mwindow_file_limit = 1;
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_MWINDOW_FILE_LIMIT, &original_mwindow_file_limit));
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_MWINDOW_FILE_LIMIT, mwindow_file_limit));
}

void test_pack_filelimit__initialize_medium(void)
{
	mwindow_file_limit = 100;
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_MWINDOW_FILE_LIMIT, &original_mwindow_file_limit));
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_MWINDOW_FILE_LIMIT, mwindow_file_limit));
}

void test_pack_filelimit__cleanup(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_MWINDOW_FILE_LIMIT, original_mwindow_file_limit));
}

void test_pack_filelimit__open_repo_with_1025_packfiles(void)
{
	git_mwindow_ctl *ctl = &git_mwindow__mem_ctl;
	git_repository *repo;
	git_revwalk *walk;
	git_oid id;
	int i;
	unsigned int open_windows;

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

	cl_git_pass(git_mutex_lock(&git__mwindow_mutex));
	/*
	 * Adding an assert while holding a lock will cause the whole process to
	 * deadlock. Copy the value and do the assert after releasing the lock.
	 */
	open_windows = ctl->open_windows;
	cl_git_pass(git_mutex_unlock(&git__mwindow_mutex));

	cl_assert_equal_i(mwindow_file_limit, open_windows);

	git_revwalk_free(walk);
	git_repository_free(repo);
}
