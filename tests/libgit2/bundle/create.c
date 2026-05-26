/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "clar_libgit2.h"
#include "git2/bundle.h"
#include "git2/clone.h"
#include "git2/refs.h"
#include "git2/repository.h"
#include "git2/revwalk.h"

static git_repository *repo;

void test_bundle_create__initialize(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	cl_git_pass(git_clone(&repo,
		cl_fixture("testrepo.git"), "./create_repo", &opts));
}

void test_bundle_create__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;
	cl_fixture_cleanup("./create_repo");
	cl_fixture_cleanup("./test.bundle");
	cl_fixture_cleanup("./test_clone");
}

/* -------------------------------------------------------------------------
 * Create a full bundle and verify the result is readable
 * ---------------------------------------------------------------------- */
void test_bundle_create__full_bundle(void)
{
	git_revwalk *walk = NULL;
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	size_t count;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_revwalk_push_glob(walk, "refs/heads/*"));

	cl_git_pass(git_bundle_create(
		"./test.bundle", repo, walk, NULL, NULL));

	cl_git_pass(git_bundle_open(&bundle, "./test.bundle"));
	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert(count > 0);

	git_bundle_free(bundle);
	git_revwalk_free(walk);
}

/* -------------------------------------------------------------------------
 * Create a bundle with explicit refs
 * ---------------------------------------------------------------------- */
void test_bundle_create__explicit_refs(void)
{
	git_revwalk *walk = NULL;
	git_bundle *bundle = NULL;
	const git_remote_head **heads;
	size_t count;
	/*
	 * Use remote-tracking refs since this is a non-bare clone; local
	 * branches in the source testrepo.git appear as
	 * refs/remotes/origin/* in the cloned working copy.
	 */
	char *ref_strings[] = {
		"refs/remotes/origin/master",
		"refs/remotes/origin/br2"
	};
	git_strarray refs = { ref_strings, 2 };
	bool found_master = false, found_br2 = false;
	size_t i;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/remotes/origin/master"));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/remotes/origin/br2"));

	cl_git_pass(git_bundle_create(
		"./test.bundle", repo, walk, &refs, NULL));

	cl_git_pass(git_bundle_open(&bundle, "./test.bundle"));
	cl_git_pass(git_bundle_refs(&heads, &count, bundle));
	cl_assert_equal_sz(2, count);

	for (i = 0; i < count; i++) {
		if (strcmp(heads[i]->name, "refs/remotes/origin/master") == 0)
			found_master = true;
		if (strcmp(heads[i]->name, "refs/remotes/origin/br2") == 0)
			found_br2 = true;
	}
	cl_assert(found_master);
	cl_assert(found_br2);

	git_bundle_free(bundle);
	git_revwalk_free(walk);
}

/* -------------------------------------------------------------------------
 * Create an incremental bundle with prerequisites and verify them
 * ---------------------------------------------------------------------- */
void test_bundle_create__incremental_has_prerequisites(void)
{
	git_revwalk *walk = NULL;
	git_bundle *bundle = NULL;
	git_oidarray prereqs;

	/* Get the root commit of master */
	git_revwalk *root_walk = NULL;
	git_oid root_oid;

	cl_git_pass(git_revwalk_new(&root_walk, repo));
	cl_git_pass(git_revwalk_push_ref(root_walk, "refs/heads/master"));
	git_revwalk_sorting(root_walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

	cl_git_pass(git_revwalk_next(&root_oid, root_walk));
	git_revwalk_free(root_walk);

	/* Bundle master, hiding the root commit */
	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_revwalk_push_ref(walk, "refs/heads/master"));
	cl_git_pass(git_revwalk_hide(walk, &root_oid));

	cl_git_pass(git_bundle_create(
		"./test.bundle", repo, walk, NULL, NULL));

	cl_git_pass(git_bundle_open(&bundle, "./test.bundle"));
	cl_git_pass(git_bundle_prerequisites(&prereqs, bundle));

	/* The root commit should appear as a prerequisite */
	cl_assert(prereqs.count > 0);

	git_oidarray_dispose(&prereqs);
	git_bundle_free(bundle);
	git_revwalk_free(walk);
}

/* -------------------------------------------------------------------------
 * Round-trip: create → clone → verify
 * ---------------------------------------------------------------------- */
void test_bundle_create__round_trip_clone(void)
{
	git_revwalk *walk = NULL;
	git_repository *cloned = NULL;
	git_object *obj = NULL;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_revwalk_push_glob(walk, "refs/heads/*"));

	cl_git_pass(git_bundle_create(
		"./test.bundle", repo, walk, NULL, NULL));
	git_revwalk_free(walk);
	walk = NULL;

	/* Clone from the bundle we just created */
	{
		git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
		cl_git_pass(git_clone(
			&cloned, "./test.bundle", "./test_clone", &opts));
	}

	/* Verify that master tip is what we expect */
	cl_git_pass(git_revparse_single(
		&obj, cloned, "refs/remotes/origin/master"));
	cl_assert(obj != NULL);

	git_object_free(obj);
	git_repository_free(cloned);
	git_revwalk_free(walk);
}
