#include "clar_libgit2.h"

#include "futils.h"
#include "fs_path.h"
#include "posix.h"

static git_repository *g_repo;
static git_remote *g_remote;

static int g_progress_calls;
static int g_progress_result;
static git_remote *g_stop_remote;
static bool g_repo_is_sandbox;

void test_fetch_bundle__initialize(void)
{
	g_repo = NULL;
	g_remote = NULL;
	g_progress_calls = 0;
	g_progress_result = 0;
	g_stop_remote = NULL;
	g_repo_is_sandbox = false;
}

void test_fetch_bundle__cleanup(void)
{
	git_remote_free(g_remote);
	g_remote = NULL;

	/*
	 * The sandbox owns its repository, so free ours only when we
	 * opened it, and clean the sandbox only when we made one.
	 */
	if (g_repo_is_sandbox)
		cl_git_sandbox_cleanup();
	else
		git_repository_free(g_repo);

	g_repo = NULL;

	cl_fixture_cleanup("bundle-dest.git");
	cl_fixture_cleanup("truncated.bundle");
}

static void init_dest(void)
{
	cl_git_pass(git_repository_init(&g_repo, "bundle-dest.git", true));
}

static void open_sandbox(const char *fixture)
{
	g_repo = cl_git_sandbox_init(fixture);
	g_repo_is_sandbox = true;
}

static void add_remote(const char *fixture)
{
	cl_git_pass(git_remote_create(&g_remote, g_repo, "origin",
		cl_fixture(fixture)));
}

static int progress_cb(const git_indexer_progress *stats, void *payload)
{
	GIT_UNUSED(stats);
	GIT_UNUSED(payload);

	g_progress_calls++;

	if (g_stop_remote)
		git_remote_stop(g_stop_remote);

	return g_progress_result;
}

static void fetch_opts_init(git_fetch_options *opts)
{
	git_fetch_options tmp = GIT_FETCH_OPTIONS_INIT;

	*opts = tmp;
	opts->callbacks.transfer_progress = progress_cb;
}

static void assert_no_ref(const char *name)
{
	git_reference *ref;

	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, g_repo, name));
}

static void assert_ref_equals(const char *name, const char *oid)
{
	git_reference *ref;
	git_oid expected;

	cl_git_pass(git_reference_lookup(&ref, g_repo, name));
	cl_git_pass(git_oid_from_string(&expected, oid,
		git_repository_oid_type(g_repo)));
	cl_assert(git_oid_equal(&expected, git_reference_target(ref)));

	git_reference_free(ref);
}

void test_fetch_bundle__self_contained(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	cl_git_pass(git_remote_fetch(g_remote, NULL, &opts, NULL));

	assert_ref_equals("refs/remotes/origin/master",
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	assert_ref_equals("refs/remotes/origin/br2",
		"a4a7dce85cf63874e984719f4fdd239f5145052f");

	/* the transfer was reported */
	cl_assert(g_progress_calls > 0);

	/* a bundle fetch never makes the destination shallow */
	cl_assert(!git_fs_path_exists("bundle-dest.git/shallow"));
}

/*
 * The bundle's pack is thin: its single commit deltas against the
 * prerequisite, which the destination already has.
 */
void test_fetch_bundle__incremental_with_prerequisites(void)
{
	git_fetch_options opts;
	git_oid id;
	git_commit *commit;

	open_sandbox("testrepo.git");
	add_remote("bundle/incremental.bundle");
	fetch_opts_init(&opts);

	cl_git_pass(git_remote_fetch(g_remote, NULL, &opts, NULL));

	assert_ref_equals("refs/remotes/origin/master",
		"337b691ad579ebb288c6f0a8e066b4c0c9f3e7a4");

	cl_git_pass(git_oid_from_string(&id,
		"337b691ad579ebb288c6f0a8e066b4c0c9f3e7a4", GIT_OID_SHA1));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &id));
	git_commit_free(commit);
}

void test_fetch_bundle__missing_prerequisite_fails(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/incremental.bundle");
	fetch_opts_init(&opts);

	cl_git_fail(git_remote_fetch(g_remote, NULL, &opts, NULL));

	/* the error names the object we needed */
	cl_assert(strstr(git_error_last()->message,
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750") != NULL);

	/* nothing was ingested and no reference moved */
	cl_assert_equal_i(0, g_progress_calls);
	assert_no_ref("refs/remotes/origin/master");
}

/*
 * When every wanted tip already exists locally the generic fetch path
 * asks the transport for nothing, so no pack is ingested.  Prerequisites
 * are only checked when a pack is actually needed; see the pull request
 * discussion of this deliberate gap.
 */
void test_fetch_bundle__no_pack_fetch_skips_ingestion(void)
{
	git_fetch_options opts;

	open_sandbox("testrepo.git");
	add_remote("bundle/incremental.bundle");
	fetch_opts_init(&opts);

	cl_git_pass(git_remote_fetch(g_remote, NULL, &opts, NULL));
	cl_assert(g_progress_calls > 0);

	/* everything the bundle offers is now present locally */
	g_progress_calls = 0;

	cl_git_pass(git_remote_fetch(g_remote, NULL, &opts, NULL));

	cl_assert_equal_i(0, g_progress_calls);
	assert_ref_equals("refs/remotes/origin/master",
		"337b691ad579ebb288c6f0a8e066b4c0c9f3e7a4");
}

void test_fetch_bundle__rejects_object_format_mismatch(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo_256.bundle");
	fetch_opts_init(&opts);

	cl_git_fail(git_remote_fetch(g_remote, NULL, &opts, NULL));
	cl_assert(strstr(git_error_last()->message, "object ids") != NULL);

	cl_assert_equal_i(0, g_progress_calls);
	assert_no_ref("refs/remotes/origin/master");
}

/* write a bundle whose header is intact but whose pack is cut short */
static void write_truncated_bundle(void)
{
	git_str contents = GIT_STR_INIT;
	const char *sep;
	size_t headerlen;

	cl_git_pass(git_futils_readbuffer(&contents,
		cl_fixture("bundle/testrepo.bundle")));

	sep = strstr(contents.ptr, "\n\n");
	cl_assert(sep != NULL);

	headerlen = (size_t)(sep - contents.ptr) + 2;
	cl_assert(contents.size > headerlen + 64);

	git_str_truncate(&contents, headerlen + 64);

	cl_git_pass(git_futils_writebuffer(&contents, "truncated.bundle",
		O_WRONLY | O_CREAT | O_TRUNC, 0666));

	git_str_dispose(&contents);
}

void test_fetch_bundle__truncated_pack_fails_without_updating_refs(void)
{
	git_fetch_options opts;

	init_dest();
	write_truncated_bundle();

	cl_git_pass(git_remote_create(&g_remote, g_repo, "origin",
		"truncated.bundle"));
	fetch_opts_init(&opts);

	cl_git_fail(git_remote_fetch(g_remote, NULL, &opts, NULL));

	assert_no_ref("refs/remotes/origin/master");
}

/*
 * A bundle's pack cannot be sliced by refspec: the whole pack is
 * ingested, but only the selected references are updated.
 */
void test_fetch_bundle__updates_only_selected_refs(void)
{
	git_fetch_options opts;
	git_strarray refspecs;
	char *spec = "+refs/heads/br2:refs/remotes/origin/br2";

	refspecs.strings = &spec;
	refspecs.count = 1;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	cl_git_pass(git_remote_fetch(g_remote, &refspecs, &opts, NULL));

	assert_ref_equals("refs/remotes/origin/br2",
		"a4a7dce85cf63874e984719f4fdd239f5145052f");
	assert_no_ref("refs/remotes/origin/master");
}

void test_fetch_bundle__propagates_callback_cancellation(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	g_progress_result = -1;

	cl_git_fail(git_remote_fetch(g_remote, NULL, &opts, NULL));

	assert_no_ref("refs/remotes/origin/master");
}

/*
 * Cancelling through git_remote_stop must not poison the transport: a
 * new negotiation resets the flag and the retry succeeds.
 */
void test_fetch_bundle__retry_after_stop(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	g_stop_remote = g_remote;

	cl_git_fail(git_remote_fetch(g_remote, NULL, &opts, NULL));
	assert_no_ref("refs/remotes/origin/master");

	g_stop_remote = NULL;
	g_progress_calls = 0;

	cl_git_pass(git_remote_fetch(g_remote, NULL, &opts, NULL));

	cl_assert(g_progress_calls > 0);
	assert_ref_equals("refs/remotes/origin/master",
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
}

void test_fetch_bundle__rejects_depth(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	opts.depth = 1;

	cl_git_fail_with(GIT_ENOTSUPPORTED,
		git_remote_fetch(g_remote, NULL, &opts, NULL));

	cl_assert_equal_i(0, g_progress_calls);
	assert_no_ref("refs/remotes/origin/master");
}

void test_fetch_bundle__rejects_unshallow(void)
{
	git_fetch_options opts;

	init_dest();
	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	opts.depth = GIT_FETCH_DEPTH_UNSHALLOW;

	cl_git_fail_with(GIT_ENOTSUPPORTED,
		git_remote_fetch(g_remote, NULL, &opts, NULL));

	assert_no_ref("refs/remotes/origin/master");
}

/*
 * A successful download writes the remote's shallow roots, and a bundle
 * has none; that would delete the destination's `shallow` file, so a
 * shallow destination is rejected outright.
 */
void test_fetch_bundle__rejects_shallow_destination(void)
{
	git_fetch_options opts;
	git_str before = GIT_STR_INIT, after = GIT_STR_INIT;

	open_sandbox("shallow.git");

	/* the fixture ships with an origin of its own */
	cl_git_pass(git_remote_delete(g_repo, "origin"));

	add_remote("bundle/testrepo.bundle");
	fetch_opts_init(&opts);

	cl_git_pass(git_futils_readbuffer(&before, "shallow.git/shallow"));

	cl_git_fail_with(GIT_ENOTSUPPORTED,
		git_remote_fetch(g_remote, NULL, &opts, NULL));

	cl_assert_equal_i(0, g_progress_calls);
	assert_no_ref("refs/remotes/origin/master");

	cl_git_pass(git_futils_readbuffer(&after, "shallow.git/shallow"));
	cl_assert_equal_i((int)before.size, (int)after.size);
	cl_assert(memcmp(before.ptr, after.ptr, before.size) == 0);

	git_str_dispose(&before);
	git_str_dispose(&after);
}
