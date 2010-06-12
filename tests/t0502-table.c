#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "revobject.h"
#include "hash.h"


BEGIN_TEST(table_create)

	git_revpool_table *table = NULL;

	table = git_revpool_table_create(55);
	must_be_true(table != NULL);
	must_be_true(table->size_mask + 1 == 64);

	git_revpool_table_free(table);

END_TEST

BEGIN_TEST(table_populate)

	const int objects_n = 32;
	int i;
	git_revpool_object *objects;
	git_revpool_table *table = NULL;

	table = git_revpool_table_create(objects_n * 2);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(git_revpool_object));
	memset(objects, 0x0, objects_n * sizeof(git_revpool_object));

	/* populate the hash table */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_revpool_table_insert(table, &(objects[i])));
	}

	/* make sure all the inserted objects can be found */
	for (i = 0; i < objects_n; ++i) {
		git_oid id;
		git_revpool_object *ob;

		git_hash_buf(&id, &i, sizeof(int));
		ob = git_revpool_table_lookup(table, &id);

		must_be_true(ob != NULL);
		must_be_true(ob == &(objects[i]));
	}

	/* make sure we cannot find inexisting objects */
	for (i = 0; i < 50; ++i) {
		int hash_id;
		git_oid id;

		hash_id = (rand() % 50000) + objects_n;
		git_hash_buf(&id, &hash_id, sizeof(int));
		must_be_true(git_revpool_table_lookup(table, &id) == NULL);
	}

	git_revpool_table_free(table);
	free(objects);

END_TEST


BEGIN_TEST(table_resize)

	const int objects_n = 64;
	int i;
	unsigned int old_size;
	git_revpool_object *objects;
	git_revpool_table *table = NULL;

	table = git_revpool_table_create(objects_n);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(git_revpool_object));
	memset(objects, 0x0, objects_n * sizeof(git_revpool_object));

	old_size = table->size_mask + 1;

	/* populate the hash table -- should be automatically resized */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_revpool_table_insert(table, &(objects[i])));
	}

	must_be_true(table->size_mask > old_size);

	/* make sure all the inserted objects can be found */
	for (i = 0; i < objects_n; ++i) {
		git_oid id;
		git_revpool_object *ob;

		git_hash_buf(&id, &i, sizeof(int));
		ob = git_revpool_table_lookup(table, &id);

		must_be_true(ob != NULL);
		must_be_true(ob == &(objects[i]));
	}

	/* force another resize */
	old_size = table->size_mask + 1;
	git_revpool_table_resize(table);
	must_be_true(table->size_mask > old_size);

	/* make sure all the inserted objects can be found */
	for (i = 0; i < objects_n; ++i) {
		git_oid id;
		git_revpool_object *ob;

		git_hash_buf(&id, &i, sizeof(int));
		ob = git_revpool_table_lookup(table, &id);

		must_be_true(ob != NULL);
		must_be_true(ob == &(objects[i]));
	}

	git_revpool_table_free(table);
	free(objects);

END_TEST
