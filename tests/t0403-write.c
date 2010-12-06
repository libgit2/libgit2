#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "person.h"

#include <git2/odb.h>
#include <git2/commit.h>
#include <git2/revwalk.h>

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
#define COMMIT_MESSAGE "This commit has been created in memory\n\
This is a commit created in memory and it will be written back to disk\n"

BEGIN_TEST(writenew_test)
	git_repository *repo;
	git_commit *commit, *parent;
	git_tree *tree;
	git_oid id;
	const git_person *author, *committer;
	/* char hex_oid[41]; */

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* Create commit in memory */
	must_pass(git_commit_new(&commit, repo));

	/* Add new parent */
	git_oid_mkstr(&id, commit_ids[4]);
	must_pass(git_commit_lookup(&parent, repo, &id));

	git_commit_add_parent(commit, parent);

	/* Set other attributes */
	git_commit_set_committer(commit, COMMITTER_NAME, COMMITTER_EMAIL, 123456789);
	git_commit_set_author(commit, COMMITTER_NAME, COMMITTER_EMAIL, 987654321);
	git_commit_set_message(commit, COMMIT_MESSAGE);

	/* Check attributes were set correctly */
	author = git_commit_author(commit);
	must_be_true(author != NULL);
	must_be_true(strcmp(author->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(author->email, COMMITTER_EMAIL) == 0);
	must_be_true(author->time == 987654321);

	committer = git_commit_committer(commit);
	must_be_true(committer != NULL);
	must_be_true(strcmp(committer->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(committer->email, COMMITTER_EMAIL) == 0);
	must_be_true(committer->time == 123456789);
	must_be_true(git_commit_time(commit) == 123456789);

	must_be_true(strcmp(git_commit_message(commit), COMMIT_MESSAGE) == 0);

	/* add new tree */
	git_oid_mkstr(&id, tree_oid);
	must_pass(git_tree_lookup(&tree, repo, &id));

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

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_mkstr(&id, commit_ids[0]);

	must_pass(git_commit_lookup(&commit, repo, &id));

	message = git_commit_message(commit);

/*
	git_oid_fmt(hex_oid, git_commit_id(commit));
	hex_oid[40] = 0;
	printf("Old SHA1: %s\n", hex_oid);
*/

	git_commit_set_message(commit, "This is a new test message. Cool!\n");

	git_oid_mkstr(&id, commit_ids[4]);
	must_pass(git_commit_lookup(&parent, repo, &id));

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
