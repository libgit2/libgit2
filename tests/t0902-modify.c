#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"

#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static const char *odb_dir = "../resources/sample-odb";
static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

BEGIN_TEST(tree_in_memory_add_test)
	const unsigned int entry_count = 128;


	git_odb *db;
	git_repository *repo;
	git_tree *tree;
	unsigned int i;
	git_oid entry_id;

	must_pass(git_odb_open(&db, odb_dir));

	repo = git_repository_alloc(db);
	must_be_true(repo != NULL);

	tree = git_tree_new(repo);
	must_be_true(tree != NULL);

	git_oid_mkstr(&entry_id, tree_oid);
	for (i = 0; i < entry_count; ++i) {
		char filename[32];
		sprintf(filename, "file%d.txt", i);
		must_pass(git_tree_add_entry(tree, &entry_id, filename, 040000));
	}

	must_be_true(git_tree_entrycount(tree) == entry_count);
	must_pass(git_object_write((git_object *)tree));
	must_pass(remove_loose_object(odb_dir, (git_object *)tree));

	git_repository_free(repo);
	git_odb_close(db);
END_TEST

BEGIN_TEST(tree_add_entry_test)
	git_odb *db;
	git_oid id;
	git_repository *repo;
	git_tree *tree;
	git_tree_entry *entry;
	unsigned int i;
	/* char hex_oid[41]; */

	must_pass(git_odb_open(&db, odb_dir));

	repo = git_repository_alloc(db);
	must_be_true(repo != NULL);

	git_oid_mkstr(&id, tree_oid);

	tree = git_tree_lookup(repo, &id);
	must_be_true(tree != NULL);

	must_be_true(git_tree_entrycount(tree) == 3);

	git_tree_add_entry(tree, &id, "zzz_test_entry.dat", 0);
	git_tree_add_entry(tree, &id, "01_test_entry.txt", 0);

	must_be_true(git_tree_entrycount(tree) == 5);

	entry = git_tree_entry_byindex(tree, 0);
	must_be_true(strcmp(git_tree_entry_name(entry), "01_test_entry.txt") == 0);

	entry = git_tree_entry_byindex(tree, 4);
	must_be_true(strcmp(git_tree_entry_name(entry), "zzz_test_entry.dat") == 0);

	must_pass(git_tree_remove_entry_byname(tree, "README"));
	must_be_true(git_tree_entrycount(tree) == 4);

	for (i = 0; i < git_tree_entrycount(tree); ++i) {
		entry = git_tree_entry_byindex(tree, i);
		must_be_true(strcmp(git_tree_entry_name(entry), "README") != 0);
	}

	must_pass(git_object_write((git_object *)tree));

/*
	git_oid_fmt(hex_oid, git_tree_id(tree));
	hex_oid[40] = 0;
	printf("TREE New SHA1: %s\n", hex_oid);
*/

	must_pass(remove_loose_object(odb_dir, (git_object *)tree));

	git_repository_free(repo);
	git_odb_close(db);
END_TEST
