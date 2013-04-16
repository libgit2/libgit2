#include "common.h"
#include "vector.h"
#include "util.h"
#include <git2/refdb.h>
#include <git2/refdb_backend.h>
#include <git2/errors.h>
#include <git2/repository.h>

typedef struct refdb_test_backend {
	git_refdb_backend parent;
	
	git_repository *repo;
	git_refdb *refdb;
	git_vector refs;
} refdb_test_backend;

typedef struct refdb_test_entry {
	char *name;
	git_ref_t type;
	
	union {
		git_oid oid;
		char *symbolic;
	} target;
} refdb_test_entry;

static int ref_name_cmp(const void *a, const void *b)
{
	return strcmp(git_reference_name((git_reference *)a),
		git_reference_name((git_reference *)b));
}

static int refdb_test_backend__exists(
	int *exists,
	git_refdb_backend *_backend,
	const char *ref_name)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	size_t i;
	
	assert(_backend);
	backend = (refdb_test_backend *)_backend;
	
	*exists = 0;
	
	git_vector_foreach(&backend->refs, i, entry) {
		if (strcmp(entry->name, ref_name) == 0) {
			*exists = 1;
			break;
		}
	}
	
	return 0;
}

static int refdb_test_backend__write(
	git_refdb_backend *_backend,
	const git_reference *ref)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	
	assert(_backend);
	backend = (refdb_test_backend *)_backend;

	entry = git__calloc(1, sizeof(refdb_test_entry));
	GITERR_CHECK_ALLOC(entry);
	
	entry->name = git__strdup(git_reference_name(ref));
	GITERR_CHECK_ALLOC(entry->name);
	
	entry->type = git_reference_type(ref);
	
	if (entry->type == GIT_REF_OID)
		git_oid_cpy(&entry->target.oid, git_reference_target(ref));
	else {
		entry->target.symbolic = git__strdup(git_reference_symbolic_target(ref));
		GITERR_CHECK_ALLOC(entry->target.symbolic);
	}

	git_vector_insert(&backend->refs, entry);
	
	return 0;
}

static int refdb_test_backend__lookup(
	git_reference **out,
	git_refdb_backend *_backend,
	const char *ref_name)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	size_t i;

	assert(_backend);
	backend = (refdb_test_backend *)_backend;
	
	git_vector_foreach(&backend->refs, i, entry) {
		if (strcmp(entry->name, ref_name) == 0) {
			const git_oid *oid =
				entry->type == GIT_REF_OID ? &entry->target.oid : NULL;
			const char *symbolic =
				entry->type == GIT_REF_SYMBOLIC ? entry->target.symbolic : NULL;
			
			if ((*out = git_reference__alloc(backend->refdb, ref_name, oid, symbolic)) == NULL)
				return -1;
			
			return 0;
		}
	}

	return GIT_ENOTFOUND;
}

static int refdb_test_backend__foreach(
	git_refdb_backend *_backend,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	size_t i;
	
	assert(_backend);
	backend = (refdb_test_backend *)_backend;

	git_vector_foreach(&backend->refs, i, entry) {
		if (entry->type == GIT_REF_OID && (list_flags & GIT_REF_OID) == 0)
			continue;
		
		if (entry->type == GIT_REF_SYMBOLIC && (list_flags & GIT_REF_SYMBOLIC) == 0)
			continue;
		
		if (callback(entry->name, payload) != 0)
			return GIT_EUSER;
	}
	
	return 0;
}

static void refdb_test_entry_free(refdb_test_entry *entry)
{
	if (entry->type == GIT_REF_SYMBOLIC)
		git__free(entry->target.symbolic);
	
	git__free(entry->name);
	git__free(entry);
}

static int refdb_test_backend__delete(
	git_refdb_backend *_backend,
	const git_reference *ref)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	size_t i;

	assert(_backend);
	backend = (refdb_test_backend *)_backend;

	git_vector_foreach(&backend->refs, i, entry) {
		if (strcmp(entry->name, git_reference_name(ref)) == 0) {
			git_vector_remove(&backend->refs, i);
			refdb_test_entry_free(entry);
		}
	}

	return GIT_ENOTFOUND;
}

static void refdb_test_backend__free(git_refdb_backend *_backend)
{
	refdb_test_backend *backend;
	refdb_test_entry *entry;
	size_t i;
	
	assert(_backend);
	backend = (refdb_test_backend *)_backend;

	git_vector_foreach(&backend->refs, i, entry)
		refdb_test_entry_free(entry);

	git_vector_free(&backend->refs);	
	git__free(backend);
}

int refdb_backend_test(
	git_refdb_backend **backend_out,
	git_repository *repo)
{
	refdb_test_backend *backend;
	git_refdb *refdb;
	int error = 0;

	if ((error = git_repository_refdb(&refdb, repo)) < 0)
		return error;

	backend = git__calloc(1, sizeof(refdb_test_backend));
	GITERR_CHECK_ALLOC(backend);
	
	git_vector_init(&backend->refs, 0, ref_name_cmp);

	backend->repo = repo;
	backend->refdb = refdb;

	backend->parent.exists = &refdb_test_backend__exists;
	backend->parent.lookup = &refdb_test_backend__lookup;
	backend->parent.foreach = &refdb_test_backend__foreach;
	backend->parent.write = &refdb_test_backend__write;
	backend->parent.delete = &refdb_test_backend__delete;
	backend->parent.free = &refdb_test_backend__free;

	*backend_out = (git_refdb_backend *)backend;
	return 0;
}
