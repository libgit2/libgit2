#ifndef INCLUDE_repository_h__
#define INCLUDE_repository_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"
#include "git2/repository.h"
#include "git2/object.h"

#include "hashtable.h"
#include "index.h"
#include "refs.h"

#define DOT_GIT ".git"
#define GIT_DIR DOT_GIT "/"
#define GIT_OBJECTS_DIR "objects/"
#define GIT_INDEX_FILE "index"

typedef struct {
	git_rawobj raw;
	void *write_ptr;
	size_t written_bytes;
	int open:1;
} git_odb_source;

struct git_object {
	git_oid id;
	git_repository *repo;
	git_odb_source source;
	unsigned short refcount;
	unsigned char in_memory, modified;
};

struct git_repository {
	git_odb *db;
	git_index *index;

	git_hashtable *objects;
	git_vector memory_objects;

	git_refcache references;

	char *path_repository;
	char *path_index;
	char *path_odb;
	char *path_workdir;

	unsigned is_bare:1, gc_enabled:1;
};

int git_object__source_open(git_object *object);
void git_object__source_close(git_object *object);

/* fully free the object; internal method, do not
 * export */
void git_object__free(git_object *object);

int git__source_printf(git_odb_source *source, const char *format, ...);
int git__source_write(git_odb_source *source, const void *bytes, size_t len);

int git__parse_oid(git_oid *oid, char **buffer_out, const char *buffer_end, const char *header);
int git__write_oid(git_odb_source *src, const char *header, const git_oid *oid);

#define GIT_OBJECT_INCREF(repo, ob) git_object__incref((repo), (git_object *)(ob))
#define GIT_OBJECT_DECREF(repo, ob) git_object__decref((repo), (git_object *)(ob)) 

GIT_INLINE(void) git_object__incref(git_repository *repo, struct git_object *object)
{
	if (repo && repo->gc_enabled && object)
		object->refcount++;
}

GIT_INLINE(void) git_object__decref(git_repository *repo, struct git_object *object)
{
	if (repo && repo->gc_enabled && object)
		git_object_close(object);
}

#endif
