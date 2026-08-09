#include "clar_libgit2.h"

#include "git2/clone.h"
#include "git2/transport.h"
#include "git2/sys/transport.h"
#include "futils.h"
#include "fs_path.h"
#include "vector.h"
#include "repo/repo_helpers.h"

void test_clone_transport__cleanup(void)
{
	cl_fixture_cleanup("tmp_global_path");
}

static int custom_transport(
	git_transport **out,
	git_remote *owner,
	void *payload)
{
	*((int*)payload) = 1;

	return git_transport_local(out, owner, payload);
}

static int custom_transport_remote_create(
	git_remote **out,
	git_repository *repo,
	const char *name,
	const char *url,
	void *payload)
{
	int error;

	GIT_UNUSED(payload);

	if ((error = git_remote_create(out, repo, name, url)) < 0)
		return error;

	return 0;
}

void test_clone_transport__custom_transport(void)
{
	git_repository *repo;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	int custom_transport_used = 0;

	clone_opts.remote_cb = custom_transport_remote_create;
	clone_opts.fetch_opts.callbacks.transport = custom_transport;
	clone_opts.fetch_opts.callbacks.payload = &custom_transport_used;

	cl_git_pass(git_clone(&repo, cl_fixture("testrepo.git"), "./custom_transport.git", &clone_opts));
	git_repository_free(repo);

	cl_git_pass(git_futils_rmdir_r("./custom_transport.git", NULL, GIT_RMDIR_REMOVE_FILES));

	cl_assert(custom_transport_used == 1);
}

/*
 * A transport that wraps the local transport but never advertises HEAD,
 * the way a bundle without a recorded HEAD does.
 */

#define NOHEAD_MAX_REFS 64

typedef struct {
	git_transport parent;
	git_transport *wrapped;
	const git_remote_head *refs[NOHEAD_MAX_REFS];
} nohead_transport;

static int nohead_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->connect(t->wrapped, url, direction, connect_opts);
}

static int nohead_set_connect_opts(
	git_transport *transport,
	const git_remote_connect_options *connect_opts)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->set_connect_opts(t->wrapped, connect_opts);
}

static int nohead_capabilities(unsigned int *capabilities, git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->capabilities(capabilities, t->wrapped);
}

static int nohead_oid_type(git_oid_t *out, git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->oid_type(out, t->wrapped);
}

static int nohead_ls(const git_remote_head ***out, size_t *size, git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	const git_remote_head **refs;
	size_t refs_len, i, len = 0;
	int error;

	if ((error = t->wrapped->ls(&refs, &refs_len, t->wrapped)) < 0)
		return error;

	for (i = 0; i < refs_len; i++) {
		if (strcmp(refs[i]->name, "HEAD") == 0)
			continue;

		cl_assert(len < NOHEAD_MAX_REFS);
		t->refs[len++] = refs[i];
	}

	*out = t->refs;
	*size = len;

	return 0;
}

static int nohead_push(git_transport *transport, git_push *push)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->push(t->wrapped, push);
}

static int nohead_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->negotiate_fetch(t->wrapped, repo, wants);
}

static int nohead_shallow_roots(git_oidarray *out, git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->shallow_roots(out, t->wrapped);
}

static int nohead_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->download_pack(t->wrapped, repo, stats);
}

static int nohead_is_connected(git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->is_connected(t->wrapped);
}

static void nohead_cancel(git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	t->wrapped->cancel(t->wrapped);
}

static int nohead_close(git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;
	return t->wrapped->close(t->wrapped);
}

static void nohead_free(git_transport *transport)
{
	nohead_transport *t = (nohead_transport *)transport;

	t->wrapped->free(t->wrapped);
	git__free(t);
}

static int nohead_transport_cb(
	git_transport **out,
	git_remote *owner,
	void *payload)
{
	nohead_transport *t;
	int error;

	GIT_UNUSED(payload);

	t = git__calloc(1, sizeof(nohead_transport));
	cl_assert(t);

	if ((error = git_transport_local(&t->wrapped, owner, NULL)) < 0) {
		git__free(t);
		return error;
	}

	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.connect = nohead_connect;
	t->parent.set_connect_opts = nohead_set_connect_opts;
	t->parent.capabilities = nohead_capabilities;
	t->parent.oid_type = nohead_oid_type;
	t->parent.ls = nohead_ls;
	t->parent.push = nohead_push;
	t->parent.negotiate_fetch = nohead_negotiate_fetch;
	t->parent.shallow_roots = nohead_shallow_roots;
	t->parent.download_pack = nohead_download_pack;
	t->parent.is_connected = nohead_is_connected;
	t->parent.cancel = nohead_cancel;
	t->parent.close = nohead_close;
	t->parent.free = nohead_free;

	*out = &t->parent;

	return 0;
}

static void nohead_clone(git_repository **out, const char *url, const char *path)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.callbacks.transport = nohead_transport_cb;

	cl_git_pass(git_clone(out, url, path, &opts));
}

static void assert_on_branch(git_repository *repo, const char *branch)
{
	git_reference *head;
	git_buf upstream = GIT_BUF_INIT;
	git_str name = GIT_STR_INIT;

	cl_git_pass(git_str_printf(&name, "refs/heads/%s", branch));

	cl_git_pass(git_reference_lookup(&head, repo, "HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(head));
	cl_assert_equal_s(name.ptr, git_reference_symbolic_target(head));
	git_reference_free(head);

	cl_git_pass(git_reference_lookup(&head, repo, name.ptr));
	git_reference_free(head);

	cl_git_pass(git_branch_upstream_name(&upstream, repo, name.ptr));
	cl_assert_equal_s("refs/remotes/origin/master", upstream.ptr);

	git_buf_dispose(&upstream);
	git_str_dispose(&name);
}

/*
 * The remote advertised no HEAD; the configured initial branch was
 * advertised and fetched, so clone selects and checks it out.
 */
void test_clone_transport__no_head_selects_configured_initial_branch(void)
{
	git_repository *repo;
	git_reference *ref;

	nohead_clone(&repo, cl_fixture("testrepo.git"), "./nohead");

	assert_on_branch(repo, "master");

	/* the remote did not advertise HEAD, so we must not invent one */
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, repo, "refs/remotes/origin/HEAD"));

	cl_assert_equal_i(0, git_repository_head_unborn(repo));
	cl_assert(git_fs_path_exists("./nohead/README"));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./nohead", NULL, GIT_RMDIR_REMOVE_FILES));
}

/*
 * Selection is by exact name, not by advertisement order: "test" is
 * advertised after several other branches.
 */
void test_clone_transport__no_head_selects_initial_branch_regardless_of_order(void)
{
	git_repository *repo;
	git_reference *head;

	create_tmp_global_config("tmp_global_path", "init.defaultbranch", "test");

	nohead_clone(&repo, cl_fixture("testrepo.git"), "./nohead");

	cl_git_pass(git_reference_lookup(&head, repo, "HEAD"));
	cl_assert_equal_s("refs/heads/test", git_reference_symbolic_target(head));
	git_reference_free(head);

	cl_assert_equal_i(0, git_repository_head_unborn(repo));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./nohead", NULL, GIT_RMDIR_REMOVE_FILES));
}

/*
 * The configured initial branch was not advertised: Git leaves it
 * unborn instead of picking some other branch.
 */
void test_clone_transport__no_head_leaves_absent_initial_branch_unborn(void)
{
	git_repository *repo;
	git_reference *ref;
	git_buf upstream = GIT_BUF_INIT;

	create_tmp_global_config("tmp_global_path", "init.defaultbranch", "trunk");

	nohead_clone(&repo, cl_fixture("testrepo.git"), "./nohead");

	cl_assert_equal_i(1, git_repository_head_unborn(repo));

	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, repo, "refs/heads/trunk"));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, repo, "refs/heads/master"));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, repo, "refs/remotes/origin/HEAD"));

	/* the tracking configuration is still written, as for an empty remote */
	cl_git_pass(git_branch_upstream_name(&upstream, repo, "refs/heads/trunk"));
	cl_assert_equal_s("refs/remotes/origin/trunk", upstream.ptr);
	git_buf_dispose(&upstream);

	/* nothing was checked out */
	cl_assert(!git_fs_path_exists("./nohead/README"));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./nohead", NULL, GIT_RMDIR_REMOVE_FILES));
}

/*
 * Even when exactly one branch is advertised, an absent configured
 * initial branch is not replaced by that branch.
 */
void test_clone_transport__no_head_does_not_select_the_only_branch(void)
{
	git_repository *sandbox, *repo;
	git_reference *ref;
	git_branch_iterator *iter;
	git_vector doomed = GIT_VECTOR_INIT;
	git_branch_t type;
	size_t i;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&sandbox, "testrepo.git"));

	/* reduce the remote to a single branch */
	cl_git_pass(git_branch_iterator_new(&iter, sandbox, GIT_BRANCH_LOCAL));

	while (git_branch_next(&ref, &type, iter) == 0) {
		if (strcmp(git_reference_name(ref), "refs/heads/master") == 0)
			git_reference_free(ref);
		else
			cl_git_pass(git_vector_insert(&doomed, ref));
	}

	git_branch_iterator_free(iter);

	git_vector_foreach(&doomed, i, ref) {
		cl_git_pass(git_branch_delete(ref));
		git_reference_free(ref);
	}

	git_vector_dispose(&doomed);

	create_tmp_global_config("tmp_global_path", "init.defaultbranch", "trunk");

	nohead_clone(&repo, "testrepo.git", "./nohead");

	cl_assert_equal_i(1, git_repository_head_unborn(repo));
	cl_git_fail_with(GIT_ENOTFOUND,
		git_reference_lookup(&ref, repo, "refs/heads/master"));

	git_repository_free(repo);
	git_repository_free(sandbox);
	cl_git_pass(git_futils_rmdir_r("./nohead", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_fixture_cleanup("testrepo.git");
}
