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

#include "hashtable.h"
#include "hash.h"

typedef struct _aux_object {
	int __bulk;
	git_oid id;
	int visited;
} table_item;

uint32_t hash_func(const void *key)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
	memcpy(&r, id->id, sizeof(r));
	return r;
}

int hash_haskey(void *item, const void *key)
{
	table_item *obj;
	git_oid *oid;

	obj = (table_item *)item;
	oid = (git_oid *)key;

	return (git_oid_cmp(oid, &obj->id) == 0);
}

BEGIN_TEST("table", table_create)

	git_hashtable *table = NULL;

	table = git_hashtable_alloc(55, hash_func, hash_haskey);
	must_be_true(table != NULL);
	must_be_true(table->size_mask + 1 == 64);

	git_hashtable_free(table);

END_TEST

BEGIN_TEST("table", table_populate)

	const int objects_n = 32;
	int i;

	table_item *objects;
	git_hashtable *table = NULL;

	table = git_hashtable_alloc(objects_n * 2, hash_func, hash_haskey);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(table_item));
	memset(objects, 0x0, objects_n * sizeof(table_item));

	/* populate the hash table */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
	}

	/* make sure all the inserted objects can be found */
	for (i = 0; i < objects_n; ++i) {
		git_oid id;
		table_item *ob;

		git_hash_buf(&id, &i, sizeof(int));
		ob = (table_item *)git_hashtable_lookup(table, &id);

		must_be_true(ob != NULL);
		must_be_true(ob == &(objects[i]));
	}

	/* make sure we cannot find inexisting objects */
	for (i = 0; i < 50; ++i) {
		int hash_id;
		git_oid id;

		hash_id = (rand() % 50000) + objects_n;
		git_hash_buf(&id, &hash_id, sizeof(int));
		must_be_true(git_hashtable_lookup(table, &id) == NULL);
	}

	git_hashtable_free(table);
	free(objects);

END_TEST


BEGIN_TEST("table", table_resize)

	const int objects_n = 64;
	int i;
	unsigned int old_size;
	table_item *objects;
	git_hashtable *table = NULL;

	table = git_hashtable_alloc(objects_n, hash_func, hash_haskey);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(table_item));
	memset(objects, 0x0, objects_n * sizeof(table_item));

	old_size = table->size_mask + 1;

	/* populate the hash table -- should be automatically resized */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
	}

	must_be_true(table->size_mask > old_size);

	/* make sure all the inserted objects can be found */
	for (i = 0; i < objects_n; ++i) {
		git_oid id;
		table_item *ob;

		git_hash_buf(&id, &i, sizeof(int));
		ob = (table_item *)git_hashtable_lookup(table, &id);

		must_be_true(ob != NULL);
		must_be_true(ob == &(objects[i]));
	}

	git_hashtable_free(table);
	free(objects);

END_TEST

BEGIN_TEST("tableit", table_iterator)

	const int objects_n = 32;
	int i;
	table_item *objects, *ob;

	git_hashtable *table = NULL;
	git_hashtable_iterator iterator;

	table = git_hashtable_alloc(objects_n * 2, hash_func, hash_haskey);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(table_item));
	memset(objects, 0x0, objects_n * sizeof(table_item));

	/* populate the hash table */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
	}

	git_hashtable_iterator_init(table, &iterator);

	/* iterate through all nodes, mark as visited */
	while ((ob = (table_item *)git_hashtable_iterator_next(&iterator)) != NULL)
		ob->visited = 1;

	/* make sure all nodes have been visited */
	for (i = 0; i < objects_n; ++i)
		must_be_true(objects[i].visited);

	git_hashtable_free(table);
	free(objects);

END_TEST


git_testsuite *libgit2_suite_hashtable(void)
{
	git_testsuite *suite = git_testsuite_new("Hashtable");

	ADD_TEST(suite, "table", table_create);
	ADD_TEST(suite, "table", table_populate);
	ADD_TEST(suite, "table", table_resize);
	ADD_TEST(suite, "tableit", table_iterator);

	return suite;
}
