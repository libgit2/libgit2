#include "clar_libgit2.h"

#include "futils.h"
#include "posix.h"
#include "refs.h"
#include "remote.h"

static git_repository *g_repo;
static bool g_sandboxed;

void test_network_remote_bundle__initialize(void)
{
	g_sandboxed = false;
	cl_git_pass(git_repository_init(&g_repo, "bundle-dest.git", true));
}

void test_network_remote_bundle__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;

	/* only clean the sandbox when this test made one */
	if (g_sandboxed)
		cl_git_sandbox_cleanup();

	cl_fixture_cleanup("bundle-dest.git");
	cl_fixture_cleanup("copied.bundle");
	cl_fixture_cleanup("noextension");
	cl_fixture_cleanup("arbitrary.txt");
	cl_fixture_cleanup("unreadable.bundle");
	cl_fixture_cleanup("host:like.bundle");
	cl_fixture_cleanup("subdir");
}

static void copy_fixture(const char *fixture, const char *to)
{
	cl_git_pass(git_futils_cp(cl_fixture(fixture), to, 0666));
}

static void connect_to(git_remote **out, const char *url)
{
	cl_git_pass(git_remote_create_anonymous(out, g_repo, url));
	cl_git_pass(git_remote_connect(*out, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
}

static void assert_is_bundle_transport(git_remote *remote)
{
	unsigned int caps;

	cl_git_pass(git_remote_capabilities(&caps, remote));

	/*
	 * A bundle can serve the tips it advertises, but not an arbitrary
	 * reachable object that was never packed into it.
	 */
	cl_assert_equal_i(GIT_REMOTE_CAPABILITY_TIP_OID, caps);
}

void test_network_remote_bundle__selects_bundle_by_content(void)
{
	git_remote *remote;
	const git_remote_head **refs;
	size_t len;

	/* the extension is irrelevant; only the contents matter */
	copy_fixture("bundle/testrepo.bundle", "noextension");

	connect_to(&remote, "noextension");
	assert_is_bundle_transport(remote);

	cl_git_pass(git_remote_ls(&refs, &len, remote));
	cl_assert(len > 0);

	git_remote_free(remote);

	copy_fixture("bundle/testrepo.bundle", "arbitrary.txt");

	connect_to(&remote, "arbitrary.txt");
	assert_is_bundle_transport(remote);

	git_remote_free(remote);
}

/* the fixture path is absolute, and drive-rooted on Windows */
void test_network_remote_bundle__selects_bundle_by_absolute_path(void)
{
	git_remote *remote;

	connect_to(&remote, cl_fixture("bundle/testrepo.bundle"));
	assert_is_bundle_transport(remote);

	git_remote_free(remote);
}

void test_network_remote_bundle__leaves_directories_to_the_local_transport(void)
{
	git_remote *remote;
	unsigned int caps;

	cl_git_sandbox_init("testrepo.git");
	g_sandboxed = true;

	connect_to(&remote, "testrepo.git");

	cl_git_pass(git_remote_capabilities(&caps, remote));
	cl_assert(caps & GIT_REMOTE_CAPABILITY_REACHABLE_OID);

	git_remote_free(remote);
}

static void assert_unsupported_url(const char *url)
{
	git_remote *remote;

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo, url));
	cl_git_fail(git_remote_connect(remote, GIT_DIRECTION_FETCH,
		NULL, NULL, NULL));
	cl_assert(strstr(git_error_last()->message, "unsupported URL") != NULL);

	git_remote_free(remote);
}

void test_network_remote_bundle__nonexistent_path_is_unsupported(void)
{
	assert_unsupported_url("./does-not-exist-at-all");
}

void test_network_remote_bundle__arbitrary_file_is_not_a_bundle(void)
{
	cl_git_mkfile("arbitrary.txt", "just some text\nand more of it\n");
	assert_unsupported_url("arbitrary.txt");
}

void test_network_remote_bundle__malformed_header_is_not_a_bundle(void)
{
	cl_git_mkfile("arbitrary.txt",
		"# v2 git bundle\nthis is not a reference line\n\n");
	assert_unsupported_url("arbitrary.txt");
}

/*
 * A well-formed bundle we cannot use still selects the bundle
 * transport, so that connect can say why rather than falling through
 * to "unsupported URL protocol".
 */
static void assert_unsupported_bundle(const char *contents, const char *expected)
{
	git_remote *remote;

	cl_git_mkfile("copied.bundle", contents);

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo, "copied.bundle"));
	cl_git_fail_with(GIT_ENOTSUPPORTED,
		git_remote_connect(remote, GIT_DIRECTION_FETCH, NULL, NULL, NULL));
	cl_assert(strstr(git_error_last()->message, expected) != NULL);

	git_remote_free(remote);
}

void test_network_remote_bundle__filtered_bundle_reports_its_error(void)
{
	assert_unsupported_bundle(
		"# v3 git bundle\n"
		"@filter=blob:none\n"
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/heads/master\n"
		"\n",
		"filtered bundles");
}

void test_network_remote_bundle__unknown_capability_reports_its_error(void)
{
	assert_unsupported_bundle(
		"# v3 git bundle\n"
		"@nonsense\n"
		"a65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/heads/master\n"
		"\n",
		"unsupported bundle capability");
}

/* an operational failure must not be cleared as a format miss */
void test_network_remote_bundle__read_failure_propagates(void)
{
	git_remote *remote;

	if (!cl_is_chmod_supported() || geteuid() == 0)
		cl_skip();

	copy_fixture("bundle/testrepo.bundle", "unreadable.bundle");
	cl_must_pass(p_chmod("unreadable.bundle", 0000));

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo, "unreadable.bundle"));
	cl_git_fail(git_remote_connect(remote, GIT_DIRECTION_FETCH,
		NULL, NULL, NULL));

	cl_assert_equal_i(GIT_ERROR_OS, git_error_last()->klass);
	cl_assert(strstr(git_error_last()->message, "unsupported URL") == NULL);

	git_remote_free(remote);
	cl_must_pass(p_chmod("unreadable.bundle", 0666));
}

/*
 * A colon before any path separator is an scp-style remote, so it is
 * never probed; a colon after one is just a local path.
 */
void test_network_remote_bundle__distinguishes_scp_style_paths(void)
{
	git_remote *remote;

	copy_fixture("bundle/testrepo.bundle", "host:like.bundle");

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo, "host:like.bundle"));
	cl_git_fail(git_remote_connect(remote, GIT_DIRECTION_FETCH,
		NULL, NULL, NULL));
	git_remote_free(remote);

	cl_must_pass(p_mkdir("subdir", 0777));
	copy_fixture("bundle/testrepo.bundle", "subdir/host:like.bundle");

	connect_to(&remote, "./subdir/host:like.bundle");
	assert_is_bundle_transport(remote);
	git_remote_free(remote);
}

void test_network_remote_bundle__advertises_all_refs_with_head_first(void)
{
	git_remote *remote;
	const git_remote_head **refs;
	size_t len, i;

	connect_to(&remote, cl_fixture("bundle/testrepo.bundle"));

	cl_git_pass(git_remote_ls(&refs, &len, remote));

	/* the fixture records HEAD last; it is advertised first */
	cl_assert(len > 1);
	cl_assert_equal_s("HEAD", refs[0]->name);
	cl_assert_equal_s("refs/blobs/annotated_tag_to_blob", refs[1]->name);
	cl_assert_equal_s("refs/heads/br2", refs[2]->name);

	/* the bundle format carries no symref information */
	for (i = 0; i < len; i++)
		cl_assert(refs[i]->symref_target == NULL);

	git_remote_free(remote);
}

void test_network_remote_bundle__does_not_synthesize_head(void)
{
	git_remote *remote;
	const git_remote_head **refs;
	size_t len, i;

	connect_to(&remote, cl_fixture("bundle/nohead.bundle"));

	cl_git_pass(git_remote_ls(&refs, &len, remote));
	cl_assert_equal_i(1, (int)len);

	for (i = 0; i < len; i++)
		cl_assert(strcmp(refs[i]->name, "HEAD") != 0);

	git_remote_free(remote);
}

/*
 * The transport reports only what the bundle recorded; resolving a
 * recorded HEAD by its object id is the existing default-branch logic,
 * which prefers the configured initial branch among the candidates.
 */
void test_network_remote_bundle__default_branch_prefers_initial_branch(void)
{
	git_remote *remote;
	git_buf branch = GIT_BUF_INIT;

	connect_to(&remote, cl_fixture("bundle/testrepo.bundle"));

	/* refs/heads/master and refs/heads/not-good share HEAD's id */
	cl_git_pass(git_remote_default_branch(&branch, remote));
	cl_assert_equal_s("refs/heads/master", branch.ptr);

	git_buf_dispose(&branch);
	git_remote_free(remote);
}

void test_network_remote_bundle__default_branch_absent_without_head(void)
{
	git_remote *remote;
	git_buf branch = GIT_BUF_INIT;

	connect_to(&remote, cl_fixture("bundle/nohead.bundle"));

	cl_git_fail_with(GIT_ENOTFOUND,
		git_remote_default_branch(&branch, remote));

	git_buf_dispose(&branch);
	git_remote_free(remote);
}

void test_network_remote_bundle__refs_remain_available_after_disconnect(void)
{
	git_remote *remote;
	const git_remote_head **refs;
	size_t len, connected_len;

	connect_to(&remote, cl_fixture("bundle/testrepo.bundle"));

	cl_git_pass(git_remote_ls(&refs, &connected_len, remote));
	cl_git_pass(git_remote_disconnect(remote));

	cl_git_pass(git_remote_ls(&refs, &len, remote));
	cl_assert_equal_i((int)connected_len, (int)len);
	cl_assert_equal_s("HEAD", refs[0]->name);

	git_remote_free(remote);
}

void test_network_remote_bundle__reports_sha256_object_type(void)
{
	git_remote *remote;
	git_oid_t oid_type;

	connect_to(&remote, cl_fixture("bundle/testrepo_256.bundle"));

	cl_git_pass(git_remote_oid_type(&oid_type, remote));
	cl_assert_equal_i(GIT_OID_SHA256, oid_type);

	git_remote_free(remote);
}

void test_network_remote_bundle__rejects_push(void)
{
	git_remote *remote;

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo,
		cl_fixture("bundle/testrepo.bundle")));

	cl_git_fail_with(GIT_ENOTSUPPORTED,
		git_remote_connect(remote, GIT_DIRECTION_PUSH, NULL, NULL, NULL));

	git_remote_free(remote);
}
