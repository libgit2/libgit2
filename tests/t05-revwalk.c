/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include "revwalk.h"

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

BEGIN_TEST("walk", simple_walk_test)
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

BEGIN_TEST("list", list_timesort_test)

	git_revwalk_list list;
	git_revwalk_listnode *n;
	int i, t;
	time_t previous_time;

#define TEST_SORTED() \
		previous_time = INT_MAX;\
	for (n = list.head; n != NULL; n = n->next) {\
		must_be_true(n->walk_commit->commit_object->committer->when.time <= previous_time);\
		previous_time = n->walk_commit->commit_object->committer->when.time;\
	}

#define CLEAR_LIST() \
	for (n = list.head; n != NULL; n = n->next) {\
		git_signature_free(n->walk_commit->commit_object->committer);\
		free(n->walk_commit->commit_object);\
		free(n->walk_commit);\
	}\
	git_revwalk_list_clear(&list);

	memset(&list, 0x0, sizeof(git_revwalk_list));
	srand((unsigned int)time(NULL));

	for (t = 0; t < 20; ++t) {
		const int test_size = rand() % 500 + 500;

		/* Purely random sorting test */
		for (i = 0; i < test_size; ++i) {
			git_commit *c = git__malloc(sizeof(git_commit));
			git_revwalk_commit *rc = git__malloc(sizeof(git_revwalk_commit));

			c->committer = git_signature_new("", "", (time_t)rand(), 0);
			rc->commit_object = c;

			git_revwalk_list_push_back(&list, rc);
		}

		git_revwalk_list_timesort(&list);
		TEST_SORTED();
		CLEAR_LIST();
	}

	/* Try to sort list with all dates equal. */
	for (i = 0; i < 200; ++i) {
		git_commit *c = git__malloc(sizeof(git_commit));
		git_revwalk_commit *rc = git__malloc(sizeof(git_revwalk_commit));

		c->committer = git_signature_new("", "", 0, 0);
		rc->commit_object = c;

		git_revwalk_list_push_back(&list, rc);
	}

	git_revwalk_list_timesort(&list);
	TEST_SORTED();
	CLEAR_LIST();

	/* Try to sort empty list */
	git_revwalk_list_timesort(&list);
	TEST_SORTED();

END_TEST

git_testsuite *libgit2_suite_revwalk(void)
{
	git_testsuite *suite = git_testsuite_new("Revwalk");

	ADD_TEST(suite, "walk", simple_walk_test);
	ADD_TEST(suite, "list", list_timesort_test);

	return suite;
}
