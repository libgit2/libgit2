#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git2/odb.h>
#include <git2/commit.h>
#include <git2/revwalk.h>

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


static int get_commit_index(git_commit *commit)
{
	int i;
	char oid[40];

	git_oid_fmt(oid, &commit->object.id);
	
	for (i = 0; i < commit_count; ++i)
		if (memcmp(oid, commit_ids[i], 40) == 0)
			return i;

	return -1;
}

static int test_walk(git_revwalk *walk, git_commit *start_from,
		int flags, const int possible_results[][6], int results_count)
{
	git_commit *commit = NULL;

	int i;
	int result_array[commit_count];

	git_revwalk_sorting(walk, flags);
	git_revwalk_push(walk, start_from);

	for (i = 0; i < commit_count; ++i)
		result_array[i] = -1;

	i = 0;
	while ((commit = git_revwalk_next(walk)) != NULL)
		result_array[i++] = get_commit_index(commit);

	for (i = 0; i < results_count; ++i) 
		if (memcmp(possible_results[i],
				result_array, result_bytes) == 0)
			return GIT_SUCCESS;

	return GIT_ERROR;
}

BEGIN_TEST(simple_walk_test)
	git_oid id;
	git_repository *repo;
	git_revwalk *walk;
	git_commit *head = NULL;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_revwalk_new(&walk, repo));

	git_oid_mkstr(&id, commit_head);

	must_pass(git_commit_lookup(&head, repo, &id));

	must_pass(test_walk(walk, head,
				GIT_SORT_TIME,
				commit_sorting_time, 1));

	must_pass(test_walk(walk, head,
				GIT_SORT_TOPOLOGICAL,
				commit_sorting_topo, 2));

	must_pass(test_walk(walk, head,
				GIT_SORT_TIME | GIT_SORT_REVERSE,
				commit_sorting_time_reverse, 1));

	must_pass(test_walk(walk, head,
				GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE,
				commit_sorting_topo_reverse, 2));


	git_revwalk_free(walk);
	git_repository_free(repo);
END_TEST
