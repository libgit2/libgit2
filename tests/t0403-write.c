#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "signature.h"

#include <git2/odb.h>
#include <git2/commit.h>
#include <git2/revwalk.h>
#include <git2/signature.h>

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
	const git_signature *author, *committer;
	/* char hex_oid[41]; */

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* Create commit in memory */
	must_pass(git_commit_new(&commit, repo));

	/* Add new parent */
	git_oid_mkstr(&id, commit_ids[4]);
	must_pass(git_commit_lookup(&parent, repo, &id));

	git_commit_add_parent(commit, parent);

	/* Set other attributes */
	committer = git_signature_new(COMMITTER_NAME, COMMITTER_EMAIL, 123456789, 60);
	must_be_true(committer != NULL);

	author = git_signature_new(COMMITTER_NAME, COMMITTER_EMAIL, 987654321, 90);
	must_be_true(author != NULL);

	git_commit_set_committer(commit, committer);
	git_commit_set_author(commit, author);
	git_commit_set_message(commit, COMMIT_MESSAGE);

	git_signature_free((git_signature *)committer);
	git_signature_free((git_signature *)author);

	/* Check attributes were set correctly */
	author = git_commit_author(commit);
	must_be_true(author != NULL);
	must_be_true(strcmp(author->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(author->email, COMMITTER_EMAIL) == 0);
	must_be_true(author->when.time == 987654321);
	must_be_true(author->when.offset == 90);

	committer = git_commit_committer(commit);
	must_be_true(committer != NULL);
	must_be_true(strcmp(committer->name, COMMITTER_NAME) == 0);
	must_be_true(strcmp(committer->email, COMMITTER_EMAIL) == 0);
	must_be_true(committer->when.time == 123456789);
	must_be_true(committer->when.offset == 60);

	must_be_true(strcmp(git_commit_message(commit), COMMIT_MESSAGE) == 0);

	/* add new tree */
	git_oid_mkstr(&id, tree_oid);
	must_pass(git_tree_lookup(&tree, repo, &id));

	git_commit_set_tree(commit, tree);

	/* Test it has no OID */
	must_be_true(git_commit_id(commit) == NULL);

	/* Write to disk */
	must_pass(git_object_write((git_object *)commit));

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

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

	git_commit_set_message(commit, "This is a new test message. Cool!\n");

	git_oid_mkstr(&id, commit_ids[4]);
	must_pass(git_commit_lookup(&parent, repo, &id));

	git_commit_add_parent(commit, parent);

	must_pass(git_object_write((git_object *)commit));

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

	git_repository_free(repo);
END_TEST
