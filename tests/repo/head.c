#include "clar_libgit2.h"
#include "refs.h"
#include "repo_helpers.h"
#include "posix.h"

static git_repository *repo;

void test_repo_head__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_head__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void check_last_reflog_entry(const char *email, const char *message)
{
	git_reflog *log;
	const git_reflog_entry *entry;

	cl_git_pass(git_reflog_read(&log, repo, GIT_HEAD_FILE));
	cl_assert(git_reflog_entrycount(log) > 0);
	entry = git_reflog_entry_byindex(log, 0);
	if (email)
		cl_assert_equal_s(email, git_reflog_entry_committer(entry)->email);
	if (message)
		cl_assert_equal_s(message, git_reflog_entry_message(entry));
	git_reflog_free(log);
}

void test_repo_head__head_detached(void)
{
	git_reference *ref;
	git_signature *sig;

	cl_git_pass(git_signature_now(&sig, "Foo Bar", "foo@example.com"));

	cl_assert_equal_i(false, git_repository_head_detached(repo));

	cl_git_pass(git_repository_detach_head(repo, sig, "CABLE DETACHED"));
	check_last_reflog_entry(sig->email, "CABLE DETACHED");
	cl_assert_equal_i(true, git_repository_head_detached(repo));

	/* take the repo back to it's original state */
	cl_git_pass(git_reference_symbolic_create(&ref, repo, "HEAD", "refs/heads/master",
				true, sig, "REATTACH"));
	git_reference_free(ref);

	check_last_reflog_entry(sig->email, "REATTACH");
	cl_assert_equal_i(false, git_repository_head_detached(repo));
	git_signature_free(sig);
}

void test_repo_head__unborn_head(void)
{
	git_reference *ref;

	cl_git_pass(git_repository_head_detached(repo));

	make_head_unborn(repo, NON_EXISTING_HEAD);

	cl_assert(git_repository_head_unborn(repo) == 1);


	/* take the repo back to it's original state */
	cl_git_pass(git_reference_symbolic_create(&ref, repo, "HEAD", "refs/heads/master", 1, NULL, NULL));
	cl_assert(git_repository_head_unborn(repo) == 0);

	git_reference_free(ref);
}

void test_repo_head__set_head_Attaches_HEAD_to_un_unborn_branch_when_the_branch_doesnt_exist(void)
{
	git_reference *head;

	cl_git_pass(git_repository_set_head(repo, "refs/heads/doesnt/exist/yet", NULL, NULL));

	cl_assert_equal_i(false, git_repository_head_detached(repo));

	cl_assert_equal_i(GIT_EUNBORNBRANCH, git_repository_head(&head, repo));
}

void test_repo_head__set_head_Returns_ENOTFOUND_when_the_reference_doesnt_exist(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_set_head(repo, "refs/tags/doesnt/exist/yet", NULL, NULL));
}

void test_repo_head__set_head_Fails_when_the_reference_points_to_a_non_commitish(void)
{
	cl_git_fail(git_repository_set_head(repo, "refs/tags/point_to_blob", NULL, NULL));
}

void test_repo_head__set_head_Attaches_HEAD_when_the_reference_points_to_a_branch(void)
{
	git_reference *head;

	cl_git_pass(git_repository_set_head(repo, "refs/heads/br2", NULL, NULL));

	cl_assert_equal_i(false, git_repository_head_detached(repo));

	cl_git_pass(git_repository_head(&head, repo));
	cl_assert_equal_s("refs/heads/br2", git_reference_name(head));

	git_reference_free(head);
}

static void assert_head_is_correctly_detached(void)
{
	git_reference *head;
	git_object *commit;

	cl_assert_equal_i(true, git_repository_head_detached(repo));

	cl_git_pass(git_repository_head(&head, repo));

	cl_git_pass(git_object_lookup(&commit, repo, git_reference_target(head), GIT_OBJ_COMMIT));

	git_object_free(commit);
	git_reference_free(head);
}

void test_repo_head__set_head_Detaches_HEAD_when_the_reference_doesnt_point_to_a_branch(void)
{
	cl_git_pass(git_repository_set_head(repo, "refs/tags/test", NULL, NULL));

	cl_assert_equal_i(true, git_repository_head_detached(repo));

	assert_head_is_correctly_detached();
}

void test_repo_head__set_head_detached_Return_ENOTFOUND_when_the_object_doesnt_exist(void)
{
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_set_head_detached(repo, &oid, NULL, NULL));
}

void test_repo_head__set_head_detached_Fails_when_the_object_isnt_a_commitish(void)
{
	git_object *blob;

	cl_git_pass(git_revparse_single(&blob, repo, "point_to_blob"));

	cl_git_fail(git_repository_set_head_detached(repo, git_object_id(blob), NULL, NULL));

	git_object_free(blob);
}

void test_repo_head__set_head_detached_Detaches_HEAD_and_make_it_point_to_the_peeled_commit(void)
{
	git_object *tag;

	cl_git_pass(git_revparse_single(&tag, repo, "tags/test"));
	cl_assert_equal_i(GIT_OBJ_TAG, git_object_type(tag));

	cl_git_pass(git_repository_set_head_detached(repo, git_object_id(tag), NULL, NULL));

	assert_head_is_correctly_detached();

	git_object_free(tag);
}

void test_repo_head__detach_head_Detaches_HEAD_and_make_it_point_to_the_peeled_commit(void)
{
	cl_assert_equal_i(false, git_repository_head_detached(repo));

	cl_git_pass(git_repository_detach_head(repo, NULL, NULL));

	assert_head_is_correctly_detached();
}

void test_repo_head__detach_head_Fails_if_HEAD_and_point_to_a_non_commitish(void)
{
	git_reference *head;

	cl_git_pass(git_reference_symbolic_create(&head, repo, GIT_HEAD_FILE, "refs/tags/point_to_blob", 1, NULL, NULL));

	cl_git_fail(git_repository_detach_head(repo, NULL, NULL));

	git_reference_free(head);
}

void test_repo_head__detaching_an_unborn_branch_returns_GIT_EUNBORNBRANCH(void)
{
	make_head_unborn(repo, NON_EXISTING_HEAD);

	cl_assert_equal_i(GIT_EUNBORNBRANCH, git_repository_detach_head(repo, NULL, NULL));
}

void test_repo_head__retrieving_an_unborn_branch_returns_GIT_EUNBORNBRANCH(void)
{
	git_reference *head;

	make_head_unborn(repo, NON_EXISTING_HEAD);

	cl_assert_equal_i(GIT_EUNBORNBRANCH, git_repository_head(&head, repo));
}

void test_repo_head__retrieving_a_missing_head_returns_GIT_ENOTFOUND(void)
{
	git_reference *head;

	delete_head(repo);

	cl_assert_equal_i(GIT_ENOTFOUND, git_repository_head(&head, repo));
}

void test_repo_head__can_tell_if_an_unborn_head_is_detached(void)
{
	make_head_unborn(repo, NON_EXISTING_HEAD);

	cl_assert_equal_i(false, git_repository_head_detached(repo));
}

static void test_reflog(git_repository *repo, size_t idx,
		const char *old_spec, const char *new_spec,
		const char *email, const char *message)
{
	git_reflog *log;
	const git_reflog_entry *entry;

	cl_git_pass(git_reflog_read(&log, repo, "HEAD"));
	entry = git_reflog_entry_byindex(log, idx);

	if (old_spec) {
		git_object *obj;
		cl_git_pass(git_revparse_single(&obj, repo, old_spec));
		cl_assert_equal_i(0, git_oid_cmp(git_object_id(obj), git_reflog_entry_id_old(entry)));
		git_object_free(obj);
	}
	if (new_spec) {
		git_object *obj;
		cl_git_pass(git_revparse_single(&obj, repo, new_spec));
		cl_assert_equal_i(0, git_oid_cmp(git_object_id(obj), git_reflog_entry_id_new(entry)));
		git_object_free(obj);
	}

	if (email) {
		cl_assert_equal_s(email, git_reflog_entry_committer(entry)->email);
	}
	if (message) {
		cl_assert_equal_s(message, git_reflog_entry_message(entry));
	}

	git_reflog_free(log);
}

void test_repo_head__setting_head_updates_reflog(void)
{
	git_object *tag;
	git_signature *sig;

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_repository_set_head(repo, "refs/heads/haacked", sig, "message1"));
	cl_git_pass(git_repository_set_head(repo, "refs/heads/unborn", sig, "message2"));
	cl_git_pass(git_revparse_single(&tag, repo, "tags/test"));
	cl_git_pass(git_repository_set_head_detached(repo, git_object_id(tag), sig, "message3"));
	cl_git_pass(git_repository_set_head(repo, "refs/heads/haacked", sig, "message4"));

	test_reflog(repo, 3, NULL, "refs/heads/haacked", "foo@example.com", "message1");
	test_reflog(repo, 2, "refs/heads/haacked", NULL, "foo@example.com", "message2");
	test_reflog(repo, 1, NULL, "tags/test^{commit}", "foo@example.com", "message3");
	test_reflog(repo, 0, "tags/test^{commit}", "refs/heads/haacked", "foo@example.com", "message4");

	git_object_free(tag);
	git_signature_free(sig);
}

void test_repo_head__setting_creates_head_ref(void)
{
	git_reference *head;
	git_reflog *log;
	const git_reflog_entry *entry;

	cl_git_pass(git_reference_lookup(&head, repo, "HEAD"));
	cl_git_pass(git_reference_delete(head));
	cl_git_pass(git_reflog_delete(repo, "HEAD"));

	cl_git_pass(git_repository_set_head(repo, "refs/heads/haacked", NULL, "create HEAD"));

	cl_git_pass(git_reflog_read(&log, repo, "HEAD"));
	cl_assert_equal_i(1, git_reflog_entrycount(log));
	entry = git_reflog_entry_byindex(log, 0);
	cl_assert_equal_s("create HEAD", git_reflog_entry_message(entry));

	git_reflog_free(log);
	git_reference_free(head);
}
