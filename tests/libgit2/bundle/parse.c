/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "git2/bundle.h"
#include "git2/net.h"

/* -------------------------------------------------------------------------
 * git_bundle_is_valid
 * ---------------------------------------------------------------------- */

void test_bundle_parse__is_valid_true(void)
{
	cl_assert_equal_i(1,
		git_bundle_is_valid(cl_fixture("bundle/testrepo.bundle")));
}

void test_bundle_parse__is_valid_v3(void)
{
	cl_assert_equal_i(1,
		git_bundle_is_valid(cl_fixture("bundle/v3.bundle")));
}

void test_bundle_parse__is_valid_nonexistent(void)
{
	/* missing file → 0, not an error */
	cl_assert_equal_i(0,
		git_bundle_is_valid("/does/not/exist/foo.bundle"));
}

void test_bundle_parse__is_valid_not_a_bundle(void)
{
	/* a regular git object file is not a bundle */
	cl_assert_equal_i(0,
		git_bundle_is_valid(cl_fixture("testrepo.git/HEAD")));
}

/* -------------------------------------------------------------------------
 * git_bundle_open – valid inputs
 * ---------------------------------------------------------------------- */

void test_bundle_parse__open_v2_full(void)
{
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	size_t count;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo.bundle")));

	cl_assert(bundle != NULL);

	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert(count > 0);

	git_bundle_free(bundle);
}

void test_bundle_parse__open_v2_minimal(void)
{
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	git_oidarray prereqs;
	size_t count;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/v2.bundle")));

	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert_equal_sz(1, count);
	cl_assert_equal_s("refs/heads/main", heads[0]->name);

	cl_git_pass(git_bundle_prerequisites(&prereqs, bundle));
	cl_assert_equal_sz(0, prereqs.count);
	git_oidarray_dispose(&prereqs);

	git_bundle_free(bundle);
}

void test_bundle_parse__open_v3(void)
{
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	size_t count;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/v3.bundle")));

	/* v3 bundle: should have exactly one ref */
	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert_equal_sz(1, count);

	git_bundle_free(bundle);
}

void test_bundle_parse__open_with_prerequisites(void)
{
	git_bundle *bundle = NULL;
	git_oidarray prereqs;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/v2_prereq.bundle")));

	cl_git_pass(git_bundle_prerequisites(&prereqs, bundle));
	cl_assert_equal_sz(1, prereqs.count);

	git_oidarray_dispose(&prereqs);
	git_bundle_free(bundle);
}

void test_bundle_parse__testrepo_bundle_refs(void)
{
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	size_t count;
	bool found_master = false;
	size_t i;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo.bundle")));

	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert(count > 0);

	for (i = 0; i < count; i++) {
		if (strcmp(heads[i]->name, "refs/heads/master") == 0) {
			found_master = true;
			break;
		}
	}
	cl_assert(found_master);

	git_bundle_free(bundle);
}

void test_bundle_parse__testrepo_prereq_bundle(void)
{
	git_bundle *bundle = NULL;
	git_oidarray prereqs;

	cl_git_pass(git_bundle_open(
		&bundle, cl_fixture("bundle/testrepo_prereq.bundle")));

	cl_git_pass(git_bundle_prerequisites(&prereqs, bundle));
	cl_assert_equal_sz(1, prereqs.count);

	git_oidarray_dispose(&prereqs);
	git_bundle_free(bundle);
}

/* -------------------------------------------------------------------------
 * git_bundle_open – error cases
 * ---------------------------------------------------------------------- */

void test_bundle_parse__open_nonexistent_file(void)
{
	git_bundle *bundle = NULL;
	int err;

	err = git_bundle_open(&bundle, "/no/such/file.bundle");
	cl_assert_equal_i(GIT_ENOTFOUND, err);
	cl_assert(bundle == NULL);
}

void test_bundle_parse__open_not_a_bundle(void)
{
	git_bundle *bundle = NULL;
	int err;

	err = git_bundle_open(&bundle,
		cl_fixture("testrepo.git/HEAD"));
	cl_assert(err < 0);
	cl_assert(bundle == NULL);
}

void test_bundle_parse__free_null(void)
{
	/* git_bundle_free(NULL) must not crash */
	git_bundle_free(NULL);
}
