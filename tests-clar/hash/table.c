#include "clar_libgit2.h"

#include "hashtable.h"
#include "hash.h"


// Helpers
typedef struct _aux_object {
   int __bulk;
   git_oid id;
   int visited;
} table_item;

static uint32_t hash_func(const void *key, int hash_id)
{
   uint32_t r;
   const git_oid *id = (const git_oid *)key;

   memcpy(&r, id->id + (hash_id * sizeof(uint32_t)), sizeof(r));
   return r;
}

static int hash_cmpkey(const void *a, const void *b)
{
   return git_oid_cmp((const git_oid*)a, (const git_oid*)b);
}


void test_hash_table__new(void)
{
   // create a new hashtable
   git_hashtable *table = NULL;

   table = git_hashtable_alloc(55, hash_func, hash_cmpkey);
   cl_assert(table != NULL);
   cl_assert(table->size_mask + 1 == 64);

   git_hashtable_free(table);
}

void test_hash_table__fill(void)
{
   // fill the hashtable with random entries
   const int objects_n = 32;
   int i;
   table_item *objects;
   git_hashtable *table = NULL;

   table = git_hashtable_alloc(objects_n * 2, hash_func, hash_cmpkey);
   cl_assert(table != NULL);

   objects = (table_item *)git__malloc(objects_n * sizeof(table_item));
   memset(objects, 0x0, objects_n * sizeof(table_item));

   /* populate the hash table */
   for (i = 0; i < objects_n; ++i) {
      git_hash_buf(&(objects[i].id), &i, sizeof(int));
      cl_git_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
   }

   /* make sure all the inserted objects can be found */
   for (i = 0; i < objects_n; ++i) {
      git_oid id;
      table_item *ob;

      git_hash_buf(&id, &i, sizeof(int));
      ob = (table_item *)git_hashtable_lookup(table, &id);

      cl_assert(ob != NULL);
      cl_assert(ob == &(objects[i]));
   }

   /* make sure we cannot find inexisting objects */
   for (i = 0; i < 50; ++i) {
      int hash_id;
      git_oid id;

      hash_id = (rand() % 50000) + objects_n;
      git_hash_buf(&id, &hash_id, sizeof(int));
      cl_assert(git_hashtable_lookup(table, &id) == NULL);
   }

   git_hashtable_free(table);
   git__free(objects);
}


void test_hash_table__resize(void)
{
   // make sure the table resizes automatically
   const int objects_n = 64;
   int i;
   unsigned int old_size;
   table_item *objects;
   git_hashtable *table = NULL;

   table = git_hashtable_alloc(objects_n, hash_func, hash_cmpkey);
   cl_assert(table != NULL);

   objects = (table_item*)git__malloc(objects_n * sizeof(table_item));
   memset(objects, 0x0, objects_n * sizeof(table_item));

   old_size = table->size_mask + 1;

   /* populate the hash table -- should be automatically resized */
   for (i = 0; i < objects_n; ++i) {
      git_hash_buf(&(objects[i].id), &i, sizeof(int));
      cl_git_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
   }

   cl_assert(table->size_mask > old_size);

   /* make sure all the inserted objects can be found */
   for (i = 0; i < objects_n; ++i) {
      git_oid id;
      table_item *ob;

      git_hash_buf(&id, &i, sizeof(int));
      ob = (table_item *)git_hashtable_lookup(table, &id);

      cl_assert(ob != NULL);
      cl_assert(ob == &(objects[i]));
   }

   git_hashtable_free(table);
   git__free(objects);
}


void test_hash_table__iterate(void)
{
   // iterate through all the contents of the table

   const int objects_n = 32;
   int i;
   table_item *objects, *ob;

   git_hashtable *table = NULL;

   table = git_hashtable_alloc(objects_n * 2, hash_func, hash_cmpkey);
   cl_assert(table != NULL);

   objects = git__malloc(objects_n * sizeof(table_item));
   memset(objects, 0x0, objects_n * sizeof(table_item));

   /* populate the hash table */
   for (i = 0; i < objects_n; ++i) {
      git_hash_buf(&(objects[i].id), &i, sizeof(int));
      cl_git_pass(git_hashtable_insert(table, &(objects[i].id), &(objects[i])));
   }

   GIT_HASHTABLE_FOREACH_VALUE(table, ob, ob->visited = 1);

   /* make sure all nodes have been visited */
   for (i = 0; i < objects_n; ++i)
   {
      cl_assert(objects[i].visited);
   }

   git_hashtable_free(table);
   git__free(objects);
}
