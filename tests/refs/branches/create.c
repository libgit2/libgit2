#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_commit *target;
static git_reference *branch;

void test_refs_branches_create__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	branch = NULL;
}

void test_refs_branches_create__cleanup(void)
{
	git_reference_free(branch);
	branch = NULL;

	git_commit_free(target);
	target = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

static void retrieve_target_from_oid(git_commit **out, git_repository *repo, const char *sha)
{
	git_object *obj;

	cl_git_pass(git_revparse_single(&obj, repo, sha));
	cl_git_pass(git_commit_lookup(out, repo, git_object_id(obj)));
	git_object_free(obj);
}

static void retrieve_known_commit(git_commit **commit, git_repository *repo)
{
	retrieve_target_from_oid(commit, repo, "e90810b8df3");
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_create__can_create_a_local_branch(void)
{
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, 0, NULL, NULL));
	cl_git_pass(git_oid_cmp(git_reference_target(branch), git_commit_id(target)));
}

void test_refs_branches_create__can_not_create_a_branch_if_its_name_collide_with_an_existing_one(void)
{
	retrieve_known_commit(&target, repo);

	cl_assert_equal_i(GIT_EEXISTS, git_branch_create(&branch, repo, "br2", target, 0, NULL, NULL));
}

void test_refs_branches_create__can_force_create_over_an_existing_branch(void)
{
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_create(&branch, repo, "br2", target, 1, NULL, NULL));
	cl_git_pass(git_oid_cmp(git_reference_target(branch), git_commit_id(target)));
	cl_assert_equal_s("refs/heads/br2", git_reference_name(branch));
}

void test_refs_branches_create__cannot_force_create_over_current_branch(void)
{
	const git_oid *oid;
	git_reference *branch2;
	retrieve_known_commit(&target, repo);

	cl_git_pass(git_branch_lookup(&branch2, repo, "master", GIT_BRANCH_LOCAL));
	cl_assert_equal_s("refs/heads/master", git_reference_name(branch2));
	cl_assert_equal_i(true, git_branch_is_head(branch2));
	oid = git_reference_target(branch2);

	cl_git_fail_with(-1, git_branch_create(&branch, repo, "master", target, 1, NULL, NULL));
	branch = NULL;
	cl_git_pass(git_branch_lookup(&branch, repo, "master", GIT_BRANCH_LOCAL));
	cl_assert_equal_s("refs/heads/master", git_reference_name(branch));
	cl_git_pass(git_oid_cmp(git_reference_target(branch), oid));
	git_reference_free(branch2);
}

void test_refs_branches_create__creating_a_branch_with_an_invalid_name_returns_EINVALIDSPEC(void)
{
	retrieve_known_commit(&target, repo);

	cl_assert_equal_i(GIT_EINVALIDSPEC,
		git_branch_create(&branch, repo, "inv@{id", target, 0, NULL, NULL));
}

void test_refs_branches_create__creation_creates_new_reflog(void)
{
	git_reflog *log;
	const git_reflog_entry *entry;
	git_signature *sig;

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	retrieve_known_commit(&target, repo);
	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, false, sig, "create!"));
	cl_git_pass(git_reflog_read(&log, repo, "refs/heads/" NEW_BRANCH_NAME));

	cl_assert_equal_i(1, git_reflog_entrycount(log));
	entry = git_reflog_entry_byindex(log, 0);
	cl_assert_equal_s("create!", git_reflog_entry_message(entry));
	cl_assert_equal_s("foo@example.com", git_reflog_entry_committer(entry)->email);

	git_reflog_free(log);
	git_signature_free(sig);
}

void test_refs_branches_create__default_reflog_message(void)
{
	git_reflog *log;
	const git_reflog_entry *entry;
	git_signature *sig;
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, repo));
	cl_git_pass(git_config_set_string(cfg, "user.name", "Foo Bar"));
	cl_git_pass(git_config_set_string(cfg, "user.email", "foo@example.com"));
	git_config_free(cfg);

	cl_git_pass(git_signature_default(&sig, repo));

	retrieve_known_commit(&target, repo);
	cl_git_pass(git_branch_create(&branch, repo, NEW_BRANCH_NAME, target, false, NULL, NULL));
	cl_git_pass(git_reflog_read(&log, repo, "refs/heads/" NEW_BRANCH_NAME));

	entry = git_reflog_entry_byindex(log, 0);
	cl_assert_equal_s("Branch: created", git_reflog_entry_message(entry));
	cl_assert_equal_s(sig->email, git_reflog_entry_committer(entry)->email);

	git_reflog_free(log);
	git_signature_free(sig);
}
