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

static uint32_t hash_func(const void *key, int hash_id)
{
	uint32_t r;
	const git_oid *id = key;

	memcpy(&r, id->id + (hash_id * sizeof(uint32_t)), sizeof(r));
	return r;
}

static int hash_cmpkey(const void *a, const void *b)
{
	return git_oid_cmp(a, b);
}

BEGIN_TEST(table0, "create a new hashtable")

	git_hashtable *table = NULL;

	table = git_hashtable_alloc(55, hash_func, hash_cmpkey);
	must_be_true(table != NULL);
	must_be_true(table->size_mask + 1 == 64);

	git_hashtable_free(table);

END_TEST

BEGIN_TEST(table1, "fill the hashtable with random entries")

	const int objects_n = 32;
	int i;

	table_item *objects;
	git_hashtable *table = NULL;

	table = git_hashtable_alloc(objects_n * 2, hash_func, hash_cmpkey);
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
	git__free(objects);

END_TEST


BEGIN_TEST(table2, "make sure the table resizes automatically")

	const int objects_n = 64;
	int i;
	unsigned int old_size;
	table_item *objects;
	git_hashtable *table = NULL;

	table = git_hashtable_alloc(objects_n, hash_func, hash_cmpkey);
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
	git__free(objects);

END_TEST

BEGIN_TEST(tableit0, "iterate through all the contents of the table")

	const int objects_n = 32;
	int i;
	table_item *objects, *ob;
	const void *GIT_UNUSED(_unused);

	git_hashtable *table = NULL;

	table = git_hashtable_alloc(objects_n * 2, hash_func, hash_cmpkey);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(table_item));
	memset(objects, 0x0, objects_n * sizeof(table_item));

	/* populate the hash table */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].id), &i, sizeof(int));
		must_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
	}

	GIT_HASHTABLE_FOREACH(table, _unused, ob,
		ob->visited = 1;
	);

	/* make sure all nodes have been visited */
	for (i = 0; i < objects_n; ++i)
		must_be_true(objects[i].visited);

	git_hashtable_free(table);
	git__free(objects);
END_TEST


BEGIN_SUITE(hashtable)
	ADD_TEST(table0);
	ADD_TEST(table1);
	ADD_TEST(table2);
	ADD_TEST(tableit0);
END_SUITE

