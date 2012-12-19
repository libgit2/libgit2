#include "clar_libgit2.h"
#include "buffer.h"
#include "posix.h"
#include "vector.h"
#include "../submodule/submodule_helpers.h"
#include "push_util.h"

CL_IN_CATEGORY("network")

static git_repository *_repo;

static char *_remote_url;
static char *_remote_user;
static char *_remote_pass;

static git_remote *_remote;
static record_callbacks_data _record_cbs_data = {{ 0 }};
static git_remote_callbacks _record_cbs = RECORD_CALLBACKS_INIT(&_record_cbs_data);

static git_oid _oid_b6;
static git_oid _oid_b5;
static git_oid _oid_b4;
static git_oid _oid_b3;
static git_oid _oid_b2;
static git_oid _oid_b1;

/* git_oid *oid, git_repository *repo, (string literal) blob */
#define CREATE_BLOB(oid, repo, blob) git_blob_create_frombuffer(oid, repo, blob, sizeof(blob) - 1)

static int cred_acquire_cb(git_cred **cred, const char *url, unsigned int allowed_types, void *payload)
{
	GIT_UNUSED(url);

	*((bool*)payload) = true;

	if ((GIT_CREDTYPE_USERPASS_PLAINTEXT & allowed_types) == 0 ||
		git_cred_userpass_plaintext_new(cred, _remote_user, _remote_pass) < 0)
		return -1;

	return 0;
}

typedef struct {
	const char *ref;
	const char *msg;
} push_status;

/**
 * git_push_status_foreach callback that records status entries.
 * @param data (git_vector *) of push_status instances
 */
static int record_push_status_cb(const char *ref, const char *msg, void *data)
{
	git_vector *statuses = (git_vector *)data;
	push_status *s;

	cl_assert(s = git__malloc(sizeof(*s)));
	s->ref = ref;
	s->msg = msg;

	git_vector_insert(statuses, s);

	return 0;
}

static void do_verify_push_status(git_push *push, const push_status expected[], const size_t expected_len)
{
	git_vector actual = GIT_VECTOR_INIT;
	push_status *iter;
	bool failed = false;
	size_t i;

	git_push_status_foreach(push, record_push_status_cb, &actual);

	if (expected_len != actual.length)
		failed = true;
	else
		git_vector_foreach(&actual, i, iter)
			if (strcmp(expected[i].ref, iter->ref) ||
				(expected[i].msg && strcmp(expected[i].msg, iter->msg))) {
				failed = true;
				break;
			}

	if (failed) {
		git_buf msg = GIT_BUF_INIT;

		git_buf_puts(&msg, "Expected and actual push statuses differ:\nEXPECTED:\n");

		for(i = 0; i < expected_len; i++) {
			git_buf_printf(&msg, "%s: %s\n",
				expected[i].ref,
				expected[i].msg ? expected[i].msg : "<NULL>");
		}

		git_buf_puts(&msg, "\nACTUAL:\n");

		git_vector_foreach(&actual, i, iter)
			git_buf_printf(&msg, "%s: %s\n", iter->ref, iter->msg);

		cl_fail(git_buf_cstr(&msg));

		git_buf_free(&msg);
	}

	git_vector_foreach(&actual, i, iter)
		git__free(iter);

	git_vector_free(&actual);
}

/**
 * Verifies that after git_push_finish(), refs on a remote have the expected
 * names, oids, and order.
 * 
 * @param remote remote to verify
 * @param expected_refs expected remote refs after push
 * @param expected_refs_len length of expected_refs
 */
static void verify_refs(git_remote *remote, expected_ref expected_refs[], size_t expected_refs_len)
{
	git_vector actual_refs = GIT_VECTOR_INIT;

	git_remote_ls(remote, record_ref_cb, &actual_refs);
	verify_remote_refs(&actual_refs, expected_refs, expected_refs_len);

	git_vector_free(&actual_refs);
}

void test_network_push__initialize(void)
{
	git_vector delete_specs = GIT_VECTOR_INIT;
	size_t i;
	char *curr_del_spec;
	bool cred_acquire_called = false;

	_repo = cl_git_sandbox_init("push_src");

	cl_fixture_sandbox("testrepo.git");
	cl_rename("push_src/submodule/.gitted", "push_src/submodule/.git");

	rewrite_gitmodules(git_repository_workdir(_repo));

	/* git log --format=oneline --decorate --graph
	 * *-.   951bbbb90e2259a4c8950db78946784fb53fcbce (HEAD, b6) merge b3, b4, and b5 to b6
	 * |\ \
	 * | | * fa38b91f199934685819bea316186d8b008c52a2 (b5) added submodule named 'submodule' pointing to '../testrepo.git'
	 * | * | 27b7ce66243eb1403862d05f958c002312df173d (b4) edited fold\b.txt
	 * | |/
	 * * | d9b63a88223d8367516f50bd131a5f7349b7f3e4 (b3) edited a.txt
	 * |/
	 * * a78705c3b2725f931d3ee05348d83cc26700f247 (b2, b1) added fold and fold/b.txt
	 * * 5c0bb3d1b9449d1cc69d7519fd05166f01840915 added a.txt
	 */
	git_oid_fromstr(&_oid_b6, "951bbbb90e2259a4c8950db78946784fb53fcbce");
	git_oid_fromstr(&_oid_b5, "fa38b91f199934685819bea316186d8b008c52a2");
	git_oid_fromstr(&_oid_b4, "27b7ce66243eb1403862d05f958c002312df173d");
	git_oid_fromstr(&_oid_b3, "d9b63a88223d8367516f50bd131a5f7349b7f3e4");
	git_oid_fromstr(&_oid_b2, "a78705c3b2725f931d3ee05348d83cc26700f247");
	git_oid_fromstr(&_oid_b1, "a78705c3b2725f931d3ee05348d83cc26700f247");

	/* Remote URL environment variable must be set.  User and password are optional.  */
	_remote_url = cl_getenv("GITTEST_REMOTE_URL");
	_remote_user = cl_getenv("GITTEST_REMOTE_USER");
	_remote_pass = cl_getenv("GITTEST_REMOTE_PASS");
	_remote = NULL;

	if (_remote_url) {
		cl_git_pass(git_remote_add(&_remote, _repo, "test", _remote_url));

		git_remote_set_cred_acquire_cb(_remote, cred_acquire_cb, &cred_acquire_called);
		record_callbacks_data_clear(&_record_cbs_data);
		git_remote_set_callbacks(_remote, &_record_cbs);

		cl_git_pass(git_remote_connect(_remote, GIT_DIRECTION_PUSH));

		/* Clean up previously pushed branches.  Fails if receive.denyDeletes is
		 * set on the remote.  Also, on Git 1.7.0 and newer, you must run
		 * 'git config receive.denyDeleteCurrent ignore' in the remote repo in
		 * order to delete the remote branch pointed to by HEAD (usually master).
		 * See: https://raw.github.com/git/git/master/Documentation/RelNotes/1.7.0.txt
		 */
		cl_git_pass(git_remote_ls(_remote, delete_ref_cb, &delete_specs));
		if (delete_specs.length) {
			git_push *push;

			cl_git_pass(git_push_new(&push, _remote));

			git_vector_foreach(&delete_specs, i, curr_del_spec) {
				git_push_add_refspec(push, curr_del_spec);
				git__free(curr_del_spec);
			}

			cl_git_pass(git_push_finish(push));
			git_push_free(push);
		}

		git_remote_disconnect(_remote);
		git_vector_free(&delete_specs);

		/* Now that we've deleted everything, fetch from the remote */
		cl_git_pass(git_remote_connect(_remote, GIT_DIRECTION_FETCH));
		cl_git_pass(git_remote_download(_remote, NULL, NULL));
		cl_git_pass(git_remote_update_tips(_remote));
		git_remote_disconnect(_remote);
	} else
		printf("GITTEST_REMOTE_URL unset; skipping push test\n");
}

void test_network_push__cleanup(void)
{
	if (_remote)
		git_remote_free(_remote);
	_remote = NULL;

	/* Freed by cl_git_sandbox_cleanup */
	_repo = NULL;

	record_callbacks_data_clear(&_record_cbs_data);

	cl_fixture_cleanup("testrepo.git");
	cl_git_sandbox_cleanup();
}

/**
 * Calls push and relists refs on remote to verify success.
 * 
 * @param refspecs refspecs to push
 * @param refspecs_len length of refspecs
 * @param expected_refs expected remote refs after push
 * @param expected_refs_len length of expected_refs
 * @param expected_ret expected return value from git_push_finish()
 */
static void do_push(const char *refspecs[], size_t refspecs_len,
	push_status expected_statuses[], size_t expected_statuses_len,
	expected_ref expected_refs[], size_t expected_refs_len, int expected_ret)
{
	git_push *push;
	size_t i;
	int ret;

	if (_remote) {
		cl_git_pass(git_remote_connect(_remote, GIT_DIRECTION_PUSH));

		cl_git_pass(git_push_new(&push, _remote));

		for (i = 0; i < refspecs_len; i++)
			cl_git_pass(git_push_add_refspec(push, refspecs[i]));

		if (expected_ret < 0) {
			cl_git_fail(ret = git_push_finish(push));
			cl_assert_equal_i(0, git_push_unpack_ok(push));
		}
		else {
			cl_git_pass(ret = git_push_finish(push));
			cl_assert_equal_i(1, git_push_unpack_ok(push));
		}

		do_verify_push_status(push, expected_statuses, expected_statuses_len);

		cl_assert_equal_i(expected_ret, ret);

		git_push_free(push);

		verify_refs(_remote, expected_refs, expected_refs_len);

		cl_git_pass(git_remote_update_tips(_remote));

		git_remote_disconnect(_remote);
	}
}

/* Call push_finish() without ever calling git_push_add_refspec() */
void test_network_push__noop(void)
{
	do_push(NULL, 0, NULL, 0, NULL, 0, 0);
}

void test_network_push__b1(void)
{
	const char *specs[] = { "refs/heads/b1:refs/heads/b1" };
	push_status exp_stats[] = { { "refs/heads/b1", NULL } };
	expected_ref exp_refs[] = { { "refs/heads/b1", &_oid_b1 } };
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__b2(void)
{
	const char *specs[] = { "refs/heads/b2:refs/heads/b2" };
	push_status exp_stats[] = { { "refs/heads/b2", NULL } };
	expected_ref exp_refs[] = { { "refs/heads/b2", &_oid_b2 } };
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__b3(void)
{
	const char *specs[] = { "refs/heads/b3:refs/heads/b3" };
	push_status exp_stats[] = { { "refs/heads/b3", NULL } };
	expected_ref exp_refs[] = { { "refs/heads/b3", &_oid_b3 } };
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__b4(void)
{
	const char *specs[] = { "refs/heads/b4:refs/heads/b4" };
	push_status exp_stats[] = { { "refs/heads/b4", NULL } };
	expected_ref exp_refs[] = { { "refs/heads/b4", &_oid_b4 } };
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__b5(void)
{
	const char *specs[] = { "refs/heads/b5:refs/heads/b5" };
	push_status exp_stats[] = { { "refs/heads/b5", NULL } };
	expected_ref exp_refs[] = { { "refs/heads/b5", &_oid_b5 } };
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__multi(void)
{
	const char *specs[] = {
		"refs/heads/b1:refs/heads/b1",
		"refs/heads/b2:refs/heads/b2",
		"refs/heads/b3:refs/heads/b3",
		"refs/heads/b4:refs/heads/b4",
		"refs/heads/b5:refs/heads/b5"
	};
	push_status exp_stats[] = {
		{ "refs/heads/b1", NULL },
		{ "refs/heads/b2", NULL },
		{ "refs/heads/b3", NULL },
		{ "refs/heads/b4", NULL },
		{ "refs/heads/b5", NULL }
	};
	expected_ref exp_refs[] = {
		{ "refs/heads/b1", &_oid_b1 },
		{ "refs/heads/b2", &_oid_b2 },
		{ "refs/heads/b3", &_oid_b3 },
		{ "refs/heads/b4", &_oid_b4 },
		{ "refs/heads/b5", &_oid_b5 }
	};
	do_push(specs, ARRAY_SIZE(specs),
		exp_stats, ARRAY_SIZE(exp_stats),
		exp_refs, ARRAY_SIZE(exp_refs), 0);
}

void test_network_push__implicit_tgt(void)
{
	const char *specs1[] = { "refs/heads/b1:" };
	push_status exp_stats1[] = { { "refs/heads/b1", NULL } };
	expected_ref exp_refs1[] = { { "refs/heads/b1", &_oid_b1 } };

	const char *specs2[] = { "refs/heads/b2:" };
	push_status exp_stats2[] = { { "refs/heads/b2", NULL } };
	expected_ref exp_refs2[] = {
	{ "refs/heads/b1", &_oid_b1 },
	{ "refs/heads/b2", &_oid_b2 }
	};

	do_push(specs1, ARRAY_SIZE(specs1),
		exp_stats1, ARRAY_SIZE(exp_stats1),
		exp_refs1, ARRAY_SIZE(exp_refs1), 0);
	do_push(specs2, ARRAY_SIZE(specs2),
		exp_stats2, ARRAY_SIZE(exp_stats2),
		exp_refs2, ARRAY_SIZE(exp_refs2), 0);
}

void test_network_push__fast_fwd(void)
{
	/* Fast forward b1 in tgt from _oid_b1 to _oid_b6. */

	const char *specs_init[] = { "refs/heads/b1:refs/heads/b1" };
	push_status exp_stats_init[] = { { "refs/heads/b1", NULL } };
	expected_ref exp_refs_init[] = { { "refs/heads/b1", &_oid_b1 } };

	const char *specs_ff[] = { "refs/heads/b6:refs/heads/b1" };
	push_status exp_stats_ff[] = { { "refs/heads/b1", NULL } };
	expected_ref exp_refs_ff[] = { { "refs/heads/b1", &_oid_b6 } };

	/* Do a force push to reset b1 in target back to _oid_b1 */
	const char *specs_reset[] = { "+refs/heads/b1:refs/heads/b1" };
	/* Force should have no effect on a fast forward push */
	const char *specs_ff_force[] = { "+refs/heads/b6:refs/heads/b1" };

	do_push(specs_init, ARRAY_SIZE(specs_init),
		exp_stats_init, ARRAY_SIZE(exp_stats_init),
		exp_refs_init, ARRAY_SIZE(exp_refs_init), 0);

	do_push(specs_ff, ARRAY_SIZE(specs_ff),
		exp_stats_ff, ARRAY_SIZE(exp_stats_ff),
		exp_refs_ff, ARRAY_SIZE(exp_refs_ff), 0);

	do_push(specs_reset, ARRAY_SIZE(specs_reset),
		exp_stats_init, ARRAY_SIZE(exp_stats_init),
		exp_refs_init, ARRAY_SIZE(exp_refs_init), 0);

	do_push(specs_ff_force, ARRAY_SIZE(specs_ff_force),
		exp_stats_ff, ARRAY_SIZE(exp_stats_ff),
		exp_refs_ff, ARRAY_SIZE(exp_refs_ff), 0);
}

void test_network_push__force(void)
{
	const char *specs1[] = {"refs/heads/b3:refs/heads/tgt"};
	push_status exp_stats1[] = { { "refs/heads/tgt", NULL } };
	expected_ref exp_refs1[] = { { "refs/heads/tgt", &_oid_b3 } };

	const char *specs2[] = {"refs/heads/b4:refs/heads/tgt"};

	const char *specs2_force[] = {"+refs/heads/b4:refs/heads/tgt"};
	push_status exp_stats2_force[] = { { "refs/heads/tgt", NULL } };
	expected_ref exp_refs2_force[] = { { "refs/heads/tgt", &_oid_b4 } };

	do_push(specs1, ARRAY_SIZE(specs1),
		exp_stats1, ARRAY_SIZE(exp_stats1),
		exp_refs1, ARRAY_SIZE(exp_refs1), 0);

	do_push(specs2, ARRAY_SIZE(specs2),
		NULL, 0,
		exp_refs1, ARRAY_SIZE(exp_refs1), GIT_ENONFASTFORWARD);

	/* Non-fast-forward update with force should pass. */
	do_push(specs2_force, ARRAY_SIZE(specs2_force),
		exp_stats2_force, ARRAY_SIZE(exp_stats2_force),
		exp_refs2_force, ARRAY_SIZE(exp_refs2_force), 0);
}

void test_network_push__delete(void)
{
	const char *specs1[] = {
		"refs/heads/b1:refs/heads/tgt1",
		"refs/heads/b1:refs/heads/tgt2"
	};
	push_status exp_stats1[] = {
		{ "refs/heads/tgt1", NULL },
		{ "refs/heads/tgt2", NULL }
	};
	expected_ref exp_refs1[] = {
		{ "refs/heads/tgt1", &_oid_b1 },
		{ "refs/heads/tgt2", &_oid_b1 }
	};

	const char *specs_del_fake[] = { ":refs/heads/fake" };
	/* Force has no effect for delete. */
	const char *specs_del_fake_force[] = { "+:refs/heads/fake" };

	const char *specs_delete[] = { ":refs/heads/tgt1" };
	push_status exp_stats_delete[] = { { "refs/heads/tgt1", NULL } };
	expected_ref exp_refs_delete[] = { { "refs/heads/tgt2", &_oid_b1 } };
	/* Force has no effect for delete. */
	const char *specs_delete_force[] = { "+:refs/heads/tgt1" };

	do_push(specs1, ARRAY_SIZE(specs1),
		exp_stats1, ARRAY_SIZE(exp_stats1),
		exp_refs1, ARRAY_SIZE(exp_refs1), 0);

	/* Deleting a non-existent branch should fail before the request is sent to
	 * the server because the client cannot find the old oid for the ref.
	 */
	do_push(specs_del_fake, ARRAY_SIZE(specs_del_fake),
		NULL, 0,
		exp_refs1, ARRAY_SIZE(exp_refs1), -1);
	do_push(specs_del_fake_force, ARRAY_SIZE(specs_del_fake_force),
		NULL, 0,
		exp_refs1, ARRAY_SIZE(exp_refs1), -1);

	/* Delete one of the pushed branches. */
	do_push(specs_delete, ARRAY_SIZE(specs_delete),
		exp_stats_delete, ARRAY_SIZE(exp_stats_delete),
		exp_refs_delete, ARRAY_SIZE(exp_refs_delete), 0);

	/* Re-push branches and retry delete with force. */
	do_push(specs1, ARRAY_SIZE(specs1),
		exp_stats1, ARRAY_SIZE(exp_stats1),
		exp_refs1, ARRAY_SIZE(exp_refs1), 0);
	do_push(specs_delete_force, ARRAY_SIZE(specs_delete_force),
		exp_stats_delete, ARRAY_SIZE(exp_stats_delete),
		exp_refs_delete, ARRAY_SIZE(exp_refs_delete), 0);
}

void test_network_push__bad_refspecs(void)
{
	/* All classes of refspecs that should be rejected by
	 * git_push_add_refspec() should go in this test.
	 */
	git_push *push;

	if (_remote) {
//		cl_git_pass(git_remote_connect(_remote, GIT_DIRECTION_PUSH));
		cl_git_pass(git_push_new(&push, _remote));

		/* Unexpanded branch names not supported */
		cl_git_fail(git_push_add_refspec(push, "b6:b6"));

		git_push_free(push);
	}
}

void test_network_push__expressions(void)
{
	/* TODO: Expressions in refspecs doesn't actually work yet */
	const char *specs_left_expr[] = { "refs/heads/b2~1:refs/heads/b2" };

	const char *specs_right_expr[] = { "refs/heads/b2:refs/heads/b2~1" };
	push_status exp_stats_right_expr[] = { { "refs/heads/b2~1", "funny refname" } };

	/* TODO: Find a more precise way of checking errors than a exit code of -1. */
	do_push(specs_left_expr, ARRAY_SIZE(specs_left_expr),
		NULL, 0,
		NULL, 0, -1);

	do_push(specs_right_expr, ARRAY_SIZE(specs_right_expr),
		exp_stats_right_expr, ARRAY_SIZE(exp_stats_right_expr),
		NULL, 0, 0);
}
