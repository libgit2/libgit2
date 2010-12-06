#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git2/odb.h>
#include <git2/commit.h>
#include <git2/revwalk.h>

static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

BEGIN_TEST(tree_entry_access_test)
	git_oid id;
	git_repository *repo;
	git_tree *tree;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_mkstr(&id, tree_oid);

	must_pass(git_tree_lookup(&tree, repo, &id));

	must_be_true(git_tree_entry_byname(tree, "README") != NULL);
	must_be_true(git_tree_entry_byname(tree, "NOTEXISTS") == NULL);
	must_be_true(git_tree_entry_byname(tree, "") == NULL);
	must_be_true(git_tree_entry_byindex(tree, 0) != NULL);
	must_be_true(git_tree_entry_byindex(tree, 2) != NULL);
	must_be_true(git_tree_entry_byindex(tree, 3) == NULL);
	must_be_true(git_tree_entry_byindex(tree, -1) == NULL);

	git_repository_free(repo);
END_TEST

BEGIN_TEST(tree_read_test)
	git_oid id;
	git_repository *repo;
	git_tree *tree;
	git_tree_entry *entry;
	git_object *obj;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_mkstr(&id, tree_oid);

	must_pass(git_tree_lookup(&tree, repo, &id));

	must_be_true(git_tree_entrycount(tree) == 3);

	entry = git_tree_entry_byname(tree, "README");
	must_be_true(entry != NULL);

	must_be_true(strcmp(git_tree_entry_name(entry), "README") == 0);

	must_pass(git_tree_entry_2object(&obj, entry));

	git_repository_free(repo);
END_TEST
