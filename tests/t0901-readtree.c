#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static const char *odb_dir = "../resources/sample-odb";
static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

BEGIN_TEST(tree_read_test)
	git_odb *db;
	git_oid id;
	git_repository *repo;
	git_tree *tree;
	git_tree_entry *entry;

	must_pass(git_odb_open(&db, odb_dir));

	repo = git_repository_alloc(db);
	must_be_true(repo != NULL);

	git_oid_mkstr(&id, tree_oid);

	tree = git_tree_lookup(repo, &id);
	must_be_true(tree != NULL);

	must_pass(git_tree__parse(tree));

	must_be_true(git_tree_entrycount(tree) == 3);

	entry = git_tree_entry_byname(tree, "README");
	must_be_true(entry != NULL);

	must_be_true(strcmp(git_tree_entry_name(entry), "README") == 0);


	must_be_true(git_tree_entry_2object(entry) != NULL);

	git_repository_free(repo);
	git_odb_close(db);
END_TEST
