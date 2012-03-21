#include "clar_libgit2.h"

#define REPOSITORY_FOLDER CLAR_RESOURCES "/testrepo.git/"

static const char *commit_ids[] = {
   "a4a7dce85cf63874e984719f4fdd239f5145052f", /* 0 */
   "9fd738e8f7967c078dceed8190330fc8648ee56a", /* 1 */
   "4a202b346bb0fb0db7eff3cffeb3c70babbd2045", /* 2 */
   "c47800c7266a2be04c571c04d5a6614691ea99bd", /* 3 */
   "8496071c1b46c854b31185ea97743be6a8774479", /* 4 */
   "5b5b025afb0b4c913b4c338a42934a3863bf3644", /* 5 */
   "a65fedf39aefe402d3bb6e24df4d4f5fe4547750", /* 6 */
};

#define COMMITTER_NAME "Vicent Marti"
#define COMMITTER_EMAIL "vicent@github.com"
#define COMMIT_MESSAGE "This commit has been created in memory\n\
   This is a commit created in memory and it will be written back to disk\n"

static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

void test_commit_write__from_memory(void)
{
   // write a new commit object from memory to disk

   git_repository *repo;
   git_commit *commit;
   git_oid tree_id, parent_id, commit_id;
   git_signature *author, *committer;
   const git_signature *author1, *committer1;
   git_commit *parent;
   git_tree *tree;

   cl_git_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

   git_oid_fromstr(&tree_id, tree_oid);
   cl_git_pass(git_tree_lookup(&tree, repo, &tree_id));

   git_oid_fromstr(&parent_id, commit_ids[4]);
   cl_git_pass(git_commit_lookup(&parent, repo, &parent_id));

   /* create signatures */
   cl_git_pass(git_signature_new(&committer, COMMITTER_NAME, COMMITTER_EMAIL, 123456789, 60));
   cl_git_pass(git_signature_new(&author, COMMITTER_NAME, COMMITTER_EMAIL, 987654321, 90));

   cl_git_pass(git_commit_create_v(
      &commit_id, /* out id */
      repo,
      NULL, /* do not update the HEAD */
      author,
      committer,
      NULL,
      COMMIT_MESSAGE,
      tree,
      1, parent));

   git_object_free((git_object *)parent);
   git_object_free((git_object *)tree);

   git_signature_free(committer);
   git_signature_free(author);

   cl_git_pass(git_commit_lookup(&commit, repo, &commit_id));

   /* Check attributes were set correctly */
   author1 = git_commit_author(commit);
   cl_assert(author1 != NULL);
   cl_assert(strcmp(author1->name, COMMITTER_NAME) == 0);
   cl_assert(strcmp(author1->email, COMMITTER_EMAIL) == 0);
   cl_assert(author1->when.time == 987654321);
   cl_assert(author1->when.offset == 90);

   committer1 = git_commit_committer(commit);
   cl_assert(committer1 != NULL);
   cl_assert(strcmp(committer1->name, COMMITTER_NAME) == 0);
   cl_assert(strcmp(committer1->email, COMMITTER_EMAIL) == 0);
   cl_assert(committer1->when.time == 123456789);
   cl_assert(committer1->when.offset == 60);

   cl_assert(strcmp(git_commit_message(commit), COMMIT_MESSAGE) == 0);

#ifndef GIT_WIN32
   cl_assert((loose_object_mode(REPOSITORY_FOLDER, (git_object *)commit) & 0777) == GIT_OBJECT_FILE_MODE);
#endif

   cl_git_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));

   git_commit_free(commit);
   git_repository_free(repo);
}


#define ROOT_COMMIT_MESSAGE "This is a root commit\n\
This is a root commit and should be the only one in this branch\n"

void test_commit_write__root(void)
{
   // create a root commit
	git_repository *repo;
	git_commit *commit;
	git_oid tree_id, commit_id;
	const git_oid *branch_oid;
	git_signature *author, *committer;
	const char *branch_name = "refs/heads/root-commit-branch";
	git_reference *head, *branch;
	char *head_old;
	git_tree *tree;

	cl_git_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&tree_id, tree_oid);
	cl_git_pass(git_tree_lookup(&tree, repo, &tree_id));

	/* create signatures */
	cl_git_pass(git_signature_new(&committer, COMMITTER_NAME, COMMITTER_EMAIL, 123456789, 60));
	cl_git_pass(git_signature_new(&author, COMMITTER_NAME, COMMITTER_EMAIL, 987654321, 90));

	/* First we need to update HEAD so it points to our non-existant branch */
	cl_git_pass(git_reference_lookup(&head, repo, "HEAD"));
	cl_assert(git_reference_type(head) == GIT_REF_SYMBOLIC);
	head_old = git__strdup(git_reference_target(head));
	cl_assert(head_old != NULL);

	cl_git_pass(git_reference_set_target(head, branch_name));

	cl_git_pass(git_commit_create_v(
		&commit_id, /* out id */
		repo,
		"HEAD",
		author,
		committer,
		NULL,
		ROOT_COMMIT_MESSAGE,
		tree,
		0));

	git_object_free((git_object *)tree);
	git_signature_free(committer);
	git_signature_free(author);

	/*
	 * The fact that creating a commit works has already been
	 * tested. Here we just make sure it's our commit and that it was
	 * written as a root commit.
	 */
	cl_git_pass(git_commit_lookup(&commit, repo, &commit_id));
	cl_assert(git_commit_parentcount(commit) == 0);
	cl_git_pass(git_reference_lookup(&branch, repo, branch_name));
	branch_oid = git_reference_oid(branch);
	cl_git_pass(git_oid_cmp(branch_oid, &commit_id));
	cl_assert(!strcmp(git_commit_message(commit), ROOT_COMMIT_MESSAGE));

	/* Remove the data we just added to the repo */
	git_reference_free(head);
	cl_git_pass(git_reference_lookup(&head, repo, "HEAD"));
	cl_git_pass(git_reference_set_target(head, head_old));
	cl_git_pass(git_reference_delete(branch));
	cl_git_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)commit));
	git__free(head_old);
	git_commit_free(commit);
	git_repository_free(repo);

	git_reference_free(head);
}