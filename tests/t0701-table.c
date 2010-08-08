#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "hash.h"
#include "hashtable.h"

typedef struct table_item
{
	int __bulk;
	git_oid id;
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

BEGIN_TEST(table_create)

	git_hashtable *table = NULL;

	table = git_hashtable_alloc(55, hash_func, hash_haskey);
	must_be_true(table != NULL);
	must_be_true(table->size_mask + 1 == 64);

	git_hashtable_free(table);

END_TEST

BEGIN_TEST(table_populate)

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


BEGIN_TEST(table_resize)

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
