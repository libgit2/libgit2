/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "git2/bundle.h"
#include "git2/clone.h"
#include "git2/object.h"
#include "git2/repository.h"

static git_repository *cloned;

void test_clone_bundle__initialize(void)
{
	cloned = NULL;
}

void test_clone_bundle__cleanup(void)
{
	git_repository_free(cloned);
	cloned = NULL;
	cl_fixture_cleanup("./bundle_clone");
}

/* -------------------------------------------------------------------------
 * Clone from a .bundle file given as a plain filesystem path
 * ---------------------------------------------------------------------- */
void test_clone_bundle__plain_path(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_object *obj = NULL;

	cl_git_pass(git_clone(
		&cloned,
		cl_fixture("bundle/testrepo.bundle"),
		"./bundle_clone",
		&opts));

	cl_assert(cloned != NULL);

	/* master tip must be present */
	cl_git_pass(git_revparse_single(
		&obj, cloned, "refs/remotes/origin/master"));
	cl_assert(obj != NULL);

	git_object_free(obj);
}

/* -------------------------------------------------------------------------
 * Clone using an explicit bundle:// URL
 * ---------------------------------------------------------------------- */
void test_clone_bundle__bundle_scheme_url(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_str url = GIT_STR_INIT;
	git_object *obj = NULL;

	cl_git_pass(git_str_printf(
		&url, "bundle://%s", cl_fixture("bundle/testrepo.bundle")));

	cl_git_pass(git_clone(
		&cloned, url.ptr, "./bundle_clone", &opts));

	cl_assert(cloned != NULL);

	cl_git_pass(git_revparse_single(
		&obj, cloned, "refs/remotes/origin/master"));
	cl_assert(obj != NULL);

	git_object_free(obj);
	git_str_dispose(&url);
}

/* -------------------------------------------------------------------------
 * Attempting to push to a bundle must fail with GIT_ENOTSUPPORTED
 * ---------------------------------------------------------------------- */
void test_clone_bundle__push_not_supported(void)
{
	git_remote *remote = NULL;
	git_repository *temp = NULL;
	int err;

	/* create a bare temp repo to push from */
	cl_git_pass(git_repository_init(&temp, "./bundle_clone", 0));

	cl_git_pass(git_remote_create(
		&remote, temp, "origin",
		cl_fixture("bundle/testrepo.bundle")));

	err = git_remote_push(remote, NULL, NULL);
	cl_assert(err == GIT_ENOTSUPPORTED || err < 0);

	git_remote_free(remote);
	git_repository_free(temp);
}
