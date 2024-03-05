#include "clar_libgit2.h"
#include "repo/repo_helpers.h"

void test_repo_getters__is_empty_correctly_deals_with_pristine_looking_repos(void)
{
	git_repository *repo;

	repo = cl_git_sandbox_init("empty_bare.git");
	cl_git_remove_placeholders(git_repository_path(repo), "dummy-marker.txt");

	cl_assert_equal_i(true, git_repository_is_empty(repo));

	cl_git_sandbox_cleanup();
}

void test_repo_getters__is_empty_can_detect_used_repositories(void)
{
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	cl_assert_equal_i(false, git_repository_is_empty(repo));

	git_repository_free(repo);
}

void test_repo_getters__is_empty_can_detect_repositories_with_defaultbranch_config_empty(void)
{
	git_repository *repo;

	create_tmp_global_config("tmp_global_path", "init.defaultBranch", "");

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_assert_equal_i(false, git_repository_is_empty(repo));

	git_repository_free(repo);
}

void test_repo_getters__retrieving_the_odb_honors_the_refcount(void)
{
	git_odb *odb;
	git_repository *repo;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));

	cl_git_pass(git_repository_odb(&odb, repo));
	cl_assert(((git_refcount *)odb)->refcount.val == 2);

	git_repository_free(repo);
	cl_assert(((git_refcount *)odb)->refcount.val == 1);

	git_odb_free(odb);
}

void test_repo_getters__commit_parents(void)
{
	git_repository *repo;
	git_commitarray parents;
	git_oid first_parent;
	git_oid merge_parents[4];

	git_oid__fromstr(&first_parent, "099fabac3a9ea935598528c27f866e34089c2eff", GIT_OID_SHA1);

	/* A commit on a new repository has no parents */

	cl_git_pass(git_repository_init(&repo, "new_repo", false));
	cl_git_pass(git_repository_commit_parents(&parents, repo));

	cl_assert_equal_sz(0, parents.count);
	cl_assert_equal_p(NULL, parents.commits);

	git_commitarray_dispose(&parents);
	git_repository_free(repo);

	/* A standard commit has one parent */

	repo = cl_git_sandbox_init("testrepo");
	cl_git_pass(git_repository_commit_parents(&parents, repo));

	cl_assert_equal_sz(1, parents.count);
	cl_assert_equal_oid(&first_parent, git_commit_id(parents.commits[0]));

	git_commitarray_dispose(&parents);

	/* A merge commit has multiple parents */

	cl_git_rewritefile("testrepo/.git/MERGE_HEAD",
		"8496071c1b46c854b31185ea97743be6a8774479\n"
		"5b5b025afb0b4c913b4c338a42934a3863bf3644\n"
		"4a202b346bb0fb0db7eff3cffeb3c70babbd2045\n"
		"9fd738e8f7967c078dceed8190330fc8648ee56a\n");

	cl_git_pass(git_repository_commit_parents(&parents, repo));

	cl_assert_equal_sz(5, parents.count);

	cl_assert_equal_oid(&first_parent, git_commit_id(parents.commits[0]));

	git_oid__fromstr(&merge_parents[0], "8496071c1b46c854b31185ea97743be6a8774479", GIT_OID_SHA1);
	cl_assert_equal_oid(&merge_parents[0], git_commit_id(parents.commits[1]));
	git_oid__fromstr(&merge_parents[1], "5b5b025afb0b4c913b4c338a42934a3863bf3644", GIT_OID_SHA1);
	cl_assert_equal_oid(&merge_parents[1], git_commit_id(parents.commits[2]));
	git_oid__fromstr(&merge_parents[2], "4a202b346bb0fb0db7eff3cffeb3c70babbd2045", GIT_OID_SHA1);
	cl_assert_equal_oid(&merge_parents[2], git_commit_id(parents.commits[3]));
	git_oid__fromstr(&merge_parents[3], "9fd738e8f7967c078dceed8190330fc8648ee56a", GIT_OID_SHA1);
	cl_assert_equal_oid(&merge_parents[3], git_commit_id(parents.commits[4]));

	git_commitarray_dispose(&parents);

	git_repository_free(repo);

	cl_fixture_cleanup("testrepo");
}
