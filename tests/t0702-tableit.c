#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
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


BEGIN_TEST(table_iterator)

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
