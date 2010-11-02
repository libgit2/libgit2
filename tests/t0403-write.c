#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static const char *commit_ids[] = {
	"a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
	"9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
	"c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
	"8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
	"5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
};
static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

#define COMMITTER_NAME "Vicent Marti"
#define COMMITTER_EMAIL "vicent@github.com"

BEGIN_TEST(writenew_test)
	git_repository *repo;
	git_commit *commit, *parent;
	git_tree *tree;
	git_oid id;
	/* char hex_oid[41]; */

	repo = git_repository_open(REPOSITORY_FOLDER);
	must_be_true(repo != NULL);

	/* Create commit in memory */
	commit = git_commit_new(repo);
	must_be_true(commit != NULL);

	/* Add new parent */
	git_oid_mkstr(&id, commit_ids[4]);
	parent = git_commit_lookup(repo, &id);
	must_be_true(parent != NULL);

	git_commit_add_parent(commit, parent);

	/* Set other attributes */
	git_commit_set_committer(commit, COMMITTER_NAME, COMMITTER_EMAIL, 123456789);
	git_commit_set_author(commit, COMMITTER_NAME, COMMITTER_EMAIL, 987654321);
	git_commit_set_message(commit, 
			"This commit has been created in memory\n\
This is a commit created in memory and it will be written back to disk\n");
	
	/* add new tree */
	git_oid_mkstr(&id, tree_oid);
	tree = git_tree_lookup(repo, &id);
	must_be_true(tree != NULL);

	git_commit_set_tree(commit, tree);

	/* Test it has no OID */
	must_be_true(git_commit_id(commit) == NULL);

	/* Write to disk */
	must_pass(git_object_write((git_object *)commit));

	/* Show new SHA1 */
/*
	git_oid_fmt(hex_oid, git_commit_id(commit));
	hex_oid[40] = 0;
	printf("Written new commit, SHA1: %s\n", hex_oid);
*/

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

	//git_person_free(&author);
	//git_person_free(&committer);

	git_repository_free(repo);
END_TEST

BEGIN_TEST(writeback_test)
	git_repository *repo;
	git_oid id;
	git_commit *commit, *parent;
	const char *message;
	/* char hex_oid[41]; */

	repo = git_repository_open(REPOSITORY_FOLDER);
	must_be_true(repo != NULL);

	git_oid_mkstr(&id, commit_ids[0]);

	commit = git_commit_lookup(repo, &id);
	must_be_true(commit != NULL);

	message = git_commit_message(commit);

/*
	git_oid_fmt(hex_oid, git_commit_id(commit));
	hex_oid[40] = 0;
	printf("Old SHA1: %s\n", hex_oid);
*/

	git_commit_set_message(commit, "This is a new test message. Cool!\n");

	git_oid_mkstr(&id, commit_ids[4]);
	parent = git_commit_lookup(repo, &id);
	must_be_true(parent != NULL);

	git_commit_add_parent(commit, parent);

	must_pass(git_object_write((git_object *)commit));

/*
	git_oid_fmt(hex_oid, git_commit_id(commit));
	hex_oid[40] = 0;
	printf("New SHA1: %s\n", hex_oid);
*/

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

	git_repository_free(repo);
END_TEST
