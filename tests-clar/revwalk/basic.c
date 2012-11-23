#include "clar_libgit2.h"

/*
	$ git log --oneline --graph --decorate
	*   a4a7dce (HEAD, br2) Merge branch 'master' into br2
	|\
	| * 9fd738e (master) a fourth commit
	| * 4a202b3 a third commit
	* | c47800c branch commit one
	|/
	* 5b5b025 another commit
	* 8496071 testing
*/
static const char *commit_head = "a4a7dce85cf63874e984719f4fdd239f5145052f";

static const char *commit_ids[] = {
	"a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
	"9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
	"c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
	"8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
	"5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
};

/* Careful: there are two possible topological sorts */
static const int commit_sorting_topo[][6] = {
	{0, 1, 2, 3, 5, 4}, {0, 3, 1, 2, 5, 4}
};

static const int commit_sorting_time[][6] = {
	{0, 3, 1, 2, 5, 4}
};

static const int commit_sorting_topo_reverse[][6] = {
	{4, 5, 3, 2, 1, 0}, {4, 5, 2, 1, 3, 0}
};

static const int commit_sorting_time_reverse[][6] = {
	{4, 5, 2, 1, 3, 0}
};

#define commit_count 6
static const int result_bytes = 24;


static int get_commit_index(git_oid *raw_oid)
{
	int i;
	char oid[40];

	git_oid_fmt(oid, raw_oid);

	for (i = 0; i < commit_count; ++i)
		if (memcmp(oid, commit_ids[i], 40) == 0)
			return i;

	return -1;
}

static int test_walk(git_revwalk *walk, const git_oid *root,
		int flags, const int possible_results[][6], int results_count)
{
	git_oid oid;

	int i;
	int result_array[commit_count];

	git_revwalk_sorting(walk, flags);
	git_revwalk_push(walk, root);

	for (i = 0; i < commit_count; ++i)
		result_array[i] = -1;

	i = 0;

	while (git_revwalk_next(&oid, walk) == 0) {
		result_array[i++] = get_commit_index(&oid);
		/*{
			char str[41];
			git_oid_fmt(str, &oid);
			str[40] = 0;
			printf("  %d) %s\n", i, str);
		}*/
	}

	for (i = 0; i < results_count; ++i)
		if (memcmp(possible_results[i],
				result_array, result_bytes) == 0)
			return 0;

	return GIT_ERROR;
}

static git_repository *_repo;
static git_revwalk *_walk;

void test_revwalk_basic__initialize(void)
{
	cl_git_pass(git_repository_open(&_repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_revwalk_new(&_walk, _repo));
}

void test_revwalk_basic__cleanup(void)
{
	git_revwalk_free(_walk);
	_walk = NULL;
	git_repository_free(_repo);
	_repo = NULL;
}

void test_revwalk_basic__sorting_modes(void)
{
	git_oid id;

	git_oid_fromstr(&id, commit_head);

	cl_git_pass(test_walk(_walk, &id, GIT_SORT_TIME, commit_sorting_time, 1));
	cl_git_pass(test_walk(_walk, &id, GIT_SORT_TOPOLOGICAL, commit_sorting_topo, 2));
	cl_git_pass(test_walk(_walk, &id, GIT_SORT_TIME | GIT_SORT_REVERSE, commit_sorting_time_reverse, 1));
	cl_git_pass(test_walk(_walk, &id, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE, commit_sorting_topo_reverse, 2));
}

void test_revwalk_basic__glob_heads(void)
{
	int i = 0;
	git_oid oid;

	cl_git_pass(git_revwalk_push_glob(_walk, "heads"));

	while (git_revwalk_next(&oid, _walk) == 0) {
		i++;
	}

	/* git log --branches --oneline | wc -l => 14 */
	cl_assert(i == 14);
}

void test_revwalk_basic__push_head(void)
{
	int i = 0;
	git_oid oid;

	cl_git_pass(git_revwalk_push_head(_walk));

	while (git_revwalk_next(&oid, _walk) == 0) {
		i++;
	}

	/* git log HEAD --oneline | wc -l => 7 */
	cl_assert(i == 7);
}

void test_revwalk_basic__push_head_hide_ref(void)
{
	int i = 0;
	git_oid oid;

	cl_git_pass(git_revwalk_push_head(_walk));
	cl_git_pass(git_revwalk_hide_ref(_walk, "refs/heads/packed-test"));

	while (git_revwalk_next(&oid, _walk) == 0) {
		i++;
	}

	/* git log HEAD --oneline --not refs/heads/packed-test | wc -l => 4 */
	cl_assert(i == 4);
}

void test_revwalk_basic__push_head_hide_ref_nobase(void)
{
	int i = 0;
	git_oid oid;

	cl_git_pass(git_revwalk_push_head(_walk));
	cl_git_pass(git_revwalk_hide_ref(_walk, "refs/heads/packed"));

	while (git_revwalk_next(&oid, _walk) == 0) {
		i++;
	}

	/* git log HEAD --oneline --not refs/heads/packed | wc -l => 7 */
	cl_assert(i == 7);
}

void test_revwalk_basic__disallow_non_commit(void)
{
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, "521d87c1ec3aef9824daf6d96cc0ae3710766d91"));
	cl_git_fail(git_revwalk_push(_walk, &oid));
}
