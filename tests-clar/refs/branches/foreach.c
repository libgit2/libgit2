#include "clar_libgit2.h"
#include "refs.h"

static git_repository *repo;
static git_reference *fake_remote;

void test_refs_branches_foreach__initialize(void)
{
	git_oid id;

	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&repo, "testrepo.git"));

	cl_git_pass(git_oid_fromstr(&id, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644"));
	cl_git_pass(git_reference_create(&fake_remote, repo, "refs/remotes/nulltoken/master", &id, 0));
}

void test_refs_branches_foreach__cleanup(void)
{
	git_reference_free(fake_remote);
	fake_remote = NULL;

	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

static int count_branch_list_cb(const char *branch_name, git_branch_t branch_type, void *payload)
{
	int *count;

	GIT_UNUSED(branch_type);
	GIT_UNUSED(branch_name);

	count = (int *)payload;
	(*count)++;

	return 0;
}

static void assert_retrieval(unsigned int flags, unsigned int expected_count)
{
	int count = 0;

	cl_git_pass(git_branch_foreach(repo, flags, count_branch_list_cb, &count));

	cl_assert_equal_i(expected_count, count);
}

void test_refs_branches_foreach__retrieve_all_branches(void)
{
	assert_retrieval(GIT_BRANCH_LOCAL | GIT_BRANCH_REMOTE, 14);
}

void test_refs_branches_foreach__retrieve_remote_branches(void)
{
	assert_retrieval(GIT_BRANCH_REMOTE, 2);
}

void test_refs_branches_foreach__retrieve_local_branches(void)
{
	assert_retrieval(GIT_BRANCH_LOCAL, 12);
}

struct expectations {
	const char *branch_name;
	int encounters;
};

static void assert_branch_has_been_found(struct expectations *findings, const char* expected_branch_name)
{
	int pos = 0;

	while (findings[pos].branch_name)
	{
		if (strcmp(expected_branch_name, findings[pos].branch_name) == 0) {
			cl_assert_equal_i(1, findings[pos].encounters);
			return;
		}

		pos++;
	}

	cl_fail("expected branch not found in list.");
}

static int contains_branch_list_cb(const char *branch_name, git_branch_t branch_type, void *payload)
{
	int pos = 0;
	struct expectations *exp;

	GIT_UNUSED(branch_type);

	exp = (struct expectations *)payload;

	while (exp[pos].branch_name)
	{
		if (strcmp(branch_name, exp[pos].branch_name) == 0)
			exp[pos].encounters++;
		
		pos++;
	}

	return 0;
}

/*
 * $ git branch -r
 *  nulltoken/HEAD -> nulltoken/master
 *  nulltoken/master
 */
void test_refs_branches_foreach__retrieve_remote_symbolic_HEAD_when_present(void)
{
	struct expectations exp[] = {
		{ "nulltoken/HEAD", 0 },
		{ "nulltoken/master", 0 },
		{ NULL, 0 }
	};

	git_reference_free(fake_remote);
	cl_git_pass(git_reference_symbolic_create(&fake_remote, repo, "refs/remotes/nulltoken/HEAD", "refs/remotes/nulltoken/master", 0));

	assert_retrieval(GIT_BRANCH_REMOTE, 3);

	cl_git_pass(git_branch_foreach(repo, GIT_BRANCH_REMOTE, contains_branch_list_cb, &exp));

	assert_branch_has_been_found(exp, "nulltoken/HEAD");
	assert_branch_has_been_found(exp, "nulltoken/HEAD");
}

static int branch_list_interrupt_cb(
	const char *branch_name, git_branch_t branch_type, void *payload)
{
	int *count;

	GIT_UNUSED(branch_type);
	GIT_UNUSED(branch_name);

	count = (int *)payload;
	(*count)++;

	return (*count == 5);
}

void test_refs_branches_foreach__can_cancel(void)
{
	int count = 0;

	cl_assert_equal_i(GIT_EUSER,
		git_branch_foreach(repo, GIT_BRANCH_LOCAL | GIT_BRANCH_REMOTE,
			branch_list_interrupt_cb, &count));

	cl_assert_equal_i(5, count);
}
