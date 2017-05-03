#include "clar_libgit2.h"

#include "git2/clone.h"
#include "clone.h"
#include "fileops.h"
#include "remote.h"

/**
 * Test to confirm problem fetching when the branch namespace
 * on remote system has changed relative to the set of (now
 * obsolete) branches listed in the local "remote" which cause
 * a directory-vs-file collision during a fetch.
 *
 * This is described in:
 * https://github.com/libgit2/libgit2sharp/issues/846
 *
 * Create a private instance "testrepo" in "repo1" so that we can modify it.
 * Create a top-level branch "fetchconflict846" in "repo1".
 * Clone this modified repo instance into "repo2".
 * [] "repo2" will have its "origin" remote pointing at "repo1".
 * [] "repo2" will also have the remote branch "remotes/origin/fetchconflict846".
 * Delete the branch and create branch "fetchconflict846/foobar" in "repo1".
 *
 * Attempt fetch in "repo2".
 *
 * The FETCH can fail because the (now obsolete) "remotes/origin/fetchconflict846"
 * FILE is in the way of "remotes/origin/fetchconflict846/foobar" branch that
 * FETCH is trying to create.
 *
 */
void test_refs_fetchconflict__846(void)
{
	git_repository *repo1 = NULL;
	git_repository *repo2 = NULL;
	git_reference *ref1_master = NULL;
	git_commit *commit1_master = NULL;
	git_reference *ref1_fc = NULL;
	git_remote *remote_origin = NULL;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

	clone_opts.bare = false;
	cl_git_pass(git_clone(&repo1, cl_fixture("testrepo.git"), "./repo1", &clone_opts));

	cl_git_pass(git_reference_lookup(&ref1_master, repo1, "refs/heads/master"));
	cl_git_pass(git_commit_lookup(&commit1_master, repo1, git_reference_target(ref1_master)));
	cl_git_pass(git_branch_create(&ref1_fc, repo1, "fetchconflict846", commit1_master, true));

	cl_git_pass(git_clone(&repo2, "./repo1", "./repo2", &clone_opts));

	cl_git_pass(git_branch_delete(ref1_fc));
	git_reference_free(ref1_fc);
	cl_git_pass(git_branch_create(&ref1_fc, repo1, "fetchconflict846/foobar", commit1_master, true));

	cl_git_pass(git_remote_lookup(&remote_origin, repo2, GIT_REMOTE_ORIGIN));
	cl_git_pass(git_remote_connect(remote_origin, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(remote_origin, NULL));
	cl_git_pass(git_remote_update_tips(remote_origin, NULL));


	git_reference_free(ref1_master);
	git_commit_free(commit1_master);
	git_reference_free(ref1_fc);
	git_remote_free(remote_origin);
	git_repository_free(repo1);
	git_repository_free(repo2);

	cl_git_pass(git_futils_rmdir_r("./repo1", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r("./repo2", NULL, GIT_RMDIR_REMOVE_FILES));
}
