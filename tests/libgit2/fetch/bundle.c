/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "futils.h"
#include "git2/bundle.h"
#include "git2/clone.h"
#include "git2/object.h"
#include "git2/remote.h"
#include "git2/repository.h"

static git_repository *repo;

void test_fetch_bundle__initialize(void)
{
	/*
	 * We need a repo that already contains the prerequisite commit
	 * so we can test incremental fetches too.  Clone testrepo.
	 */
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	cl_git_pass(git_clone(
		&repo, cl_fixture("testrepo.git"), "./fetch_bundle_repo", &opts));
}

void test_fetch_bundle__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;
	cl_fixture_cleanup("./fetch_bundle_repo");
	cl_fixture_cleanup("./fetch_bundle_empty");
}

/* -------------------------------------------------------------------------
 * Fetch all refs from a full bundle into a fresh empty repo
 * ---------------------------------------------------------------------- */
void test_fetch_bundle__full_bundle_into_empty(void)
{
	git_repository *empty = NULL;
	git_remote *remote = NULL;
	git_object *obj = NULL;

	cl_git_pass(git_repository_init(&empty, "./fetch_bundle_empty", 0));

	cl_git_pass(git_remote_create(
		&remote, empty, "origin",
		cl_fixture("bundle/testrepo.bundle")));

	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));

	/* master must now be present */
	cl_git_pass(git_revparse_single(
		&obj, empty, "refs/remotes/origin/master"));
	cl_assert(obj != NULL);

	git_object_free(obj);
	git_remote_free(remote);
	git_repository_free(empty);
}

/* -------------------------------------------------------------------------
 * Fetch from the prerequisites bundle: prerequisites are satisfied
 * because our repo already contains the root commit.
 * ---------------------------------------------------------------------- */
void test_fetch_bundle__incremental_satisfied(void)
{
	git_remote *remote = NULL;
	git_object *obj = NULL;

	cl_git_pass(git_remote_create(
		&remote, repo, "bundle-incr",
		cl_fixture("bundle/testrepo_prereq.bundle")));

	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));

	/* refs that were in the bundle must now be reachable */
	cl_git_pass(git_revparse_single(
		&obj, repo, "refs/remotes/bundle-incr/master"));
	cl_assert(obj != NULL);

	git_object_free(obj);
	git_remote_free(remote);
}

/* -------------------------------------------------------------------------
 * Fetch from the prerequisites bundle into an EMPTY repo: should fail
 * because the prerequisite commit is missing.
 * ---------------------------------------------------------------------- */
void test_fetch_bundle__incremental_unmet_prerequisites(void)
{
	git_repository *empty = NULL;
	git_remote *remote = NULL;
	int err;

	cl_git_pass(git_repository_init(&empty, "./fetch_bundle_empty", 0));

	cl_git_pass(git_remote_create(
		&remote, empty, "origin",
		cl_fixture("bundle/testrepo_prereq.bundle")));

	err = git_remote_fetch(remote, NULL, NULL, NULL);
	cl_assert_equal_i(GIT_ENOTFOUND, err);

	git_remote_free(remote);
	git_repository_free(empty);
}
