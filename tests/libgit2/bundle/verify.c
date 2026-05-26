/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "git2/bundle.h"
#include "git2/clone.h"
#include "git2/repository.h"

static git_repository *repo;

void test_bundle_verify__initialize(void)
{
	/*
	 * Clone testrepo into a temp directory so we have a repo whose ODB
	 * contains the commits referenced as prerequisites.
	 */
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	cl_git_pass(git_clone(&repo,
		cl_fixture("testrepo.git"), "./verify_repo", &opts));
}

void test_bundle_verify__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;
	cl_fixture_cleanup("./verify_repo");
}

/* A bundle with no prerequisites is always satisfied */
void test_bundle_verify__no_prerequisites(void)
{
	git_bundle *bundle = NULL;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo.bundle")));

	cl_git_pass(git_bundle_verify(repo, bundle));

	git_bundle_free(bundle);
}

/*
 * Prerequisites bundle: the prerequisite commit IS in our cloned repo,
 * so verify() must succeed.
 */
void test_bundle_verify__satisfied_prerequisites(void)
{
	git_bundle *bundle = NULL;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo_prereq.bundle")));

	cl_git_pass(git_bundle_verify(repo, bundle));

	git_bundle_free(bundle);
}

/*
 * Verify against an empty repo: the prerequisite is missing → GIT_ENOTFOUND.
 */
void test_bundle_verify__missing_prerequisite(void)
{
	git_repository *empty_repo = NULL;
	git_bundle *bundle = NULL;
	int err;

	cl_git_pass(git_repository_init(&empty_repo, "./empty_verify", 0));

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo_prereq.bundle")));

	err = git_bundle_verify(empty_repo, bundle);
	cl_assert_equal_i(GIT_ENOTFOUND, err);

	git_bundle_free(bundle);
	git_repository_free(empty_repo);
	cl_fixture_cleanup("./empty_verify");
}
