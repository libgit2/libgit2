#include "test_lib.h"
#include "test_helpers.h"
#include "commit.h"
#include "revobject.h"
#include "hash.h"

typedef struct _aux_object {
	git_revpool_object object;
	int visited;
} aux_object;


BEGIN_TEST(table_iterator)

	const int objects_n = 32;
	int i;
	aux_object *objects, *ob;

	git_revpool_table *table = NULL;
	git_revpool_tableit iterator;

	table = git_revpool_table_create(objects_n * 2);
	must_be_true(table != NULL);

	objects = git__malloc(objects_n * sizeof(aux_object));
	memset(objects, 0x0, objects_n * sizeof(aux_object));

	/* populate the hash table */
	for (i = 0; i < objects_n; ++i) {
		git_hash_buf(&(objects[i].object.id), &i, sizeof(int));
		must_pass(git_revpool_table_insert(table, (git_revpool_object *)&(objects[i])));
	}

	git_revpool_tableit_init(table, &iterator);

	/* iterate through all nodes, mark as visited */
	while ((ob = (aux_object *)git_revpool_tableit_next(&iterator)) != NULL)
		ob->visited = 1;

	/* make sure all nodes have been visited */
	for (i = 0; i < objects_n; ++i)
		must_be_true(objects[i].visited);

	git_revpool_table_free(table);
	free(objects);

END_TEST
