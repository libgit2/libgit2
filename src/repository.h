#ifndef INCLUDE_repository_h__
#define INCLUDE_repository_h__

#include "git/common.h"
#include "git/oid.h"
#include "git/odb.h"
#include "git/repository.h"

#include "hashtable.h"

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
	int in_memory:1, modified:1;
};

struct git_repository {
	git_odb *db;
	git_hashtable *objects;
};


int git_object__source_open(git_object *object);
void git_object__source_close(git_object *object);

int git__source_printf(git_odb_source *source, const char *format, ...);
int git__source_write(git_odb_source *source, const void *bytes, size_t len);

#endif
