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

#include "tree.h"

static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";

BEGIN_TEST("readtree", tree_entry_access_test)
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

BEGIN_TEST("readtree", tree_read_test)
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

BEGIN_TEST("modify", tree_in_memory_add_test)
	const unsigned int entry_count = 128;

	git_repository *repo;
	git_tree *tree;
	unsigned int i;
	git_oid entry_id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_tree_new(&tree, repo));

	git_oid_mkstr(&entry_id, tree_oid);
	for (i = 0; i < entry_count; ++i) {
		char filename[32];
		git_tree_entry *ent = NULL;

		sprintf(filename, "file%d.txt", i);
		must_pass(git_tree_add_entry(&ent, tree, &entry_id, filename, 040000));
		must_be_true(ent != NULL);
	}

	must_be_true(git_tree_entrycount(tree) == entry_count);
	must_pass(git_object_write((git_object *)tree));
	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)tree));

	git_object_free((git_object *)tree);

	git_repository_free(repo);
END_TEST

BEGIN_TEST("modify", tree_add_entry_test)
	git_oid id;
	git_repository *repo;
	git_tree *tree;
	git_tree_entry *entry;
	unsigned int i;
	/* char hex_oid[41]; */

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_mkstr(&id, tree_oid);

	must_pass(git_tree_lookup(&tree, repo, &id));

	must_be_true(git_tree_entrycount(tree) == 3);

	/* check there is NP if we don't want the
	 * created entry back */
	git_tree_add_entry(NULL, tree, &id, "zzz_test_entry.dat", 0);
	git_tree_add_entry(NULL, tree, &id, "01_test_entry.txt", 0);

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

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)tree));
	git_object_free((git_object *)tree);
	git_repository_free(repo);
END_TEST


git_testsuite *libgit2_suite_tree(void)
{
	git_testsuite *suite = git_testsuite_new("Tree");

	ADD_TEST(suite, "readtree", tree_entry_access_test);
	ADD_TEST(suite, "readtree", tree_read_test);
	ADD_TEST(suite, "modify", tree_in_memory_add_test);
	ADD_TEST(suite, "modify", tree_add_entry_test);

	return suite;
}
