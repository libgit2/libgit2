#include "vector.h"
#include "util.h"
#include "testdb.h"

typedef struct refdb_test_backend {
	git_refdb_backend parent;

	git_repository *repo;
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

			if (entry->type == GIT_REF_OID) {
				*out = git_reference__alloc(ref_name,
					&entry->target.oid, NULL);
			} else if (entry->type == GIT_REF_SYMBOLIC) {
				*out = git_reference__alloc_symbolic(ref_name,
					entry->target.symbolic);
			}

			if (*out == NULL)
				return -1;

			return 0;
		}
	}

	return GIT_ENOTFOUND;
}

typedef struct {
	git_reference_iterator parent;
	size_t i;
} refdb_test_iter;

static int refdb_test_backend__iterator(git_reference_iterator **out, git_refdb_backend *_backend)
{
	refdb_test_iter *iter;

	GIT_UNUSED(_backend);

	iter = git__calloc(1, sizeof(refdb_test_iter));
	GITERR_CHECK_ALLOC(iter);

	iter->parent.backend = _backend;
	iter->i = 0;

	*out = (git_reference_iterator *) iter;

	return 0;
}

static int refdb_test_backend__next(const char **name, git_reference_iterator *_iter)
{
	refdb_test_entry *entry;
	refdb_test_backend *backend = (refdb_test_backend *) _iter->backend;
	refdb_test_iter *iter = (refdb_test_iter *) _iter;

	entry = git_vector_get(&backend->refs, iter->i);
	if (!entry)
		return GIT_ITEROVER;

	*name = entry->name;
	iter->i++;

	return 0;
}

static void refdb_test_backend__iterator_free(git_reference_iterator *iter)
{
	git__free(iter);
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

	backend = git__calloc(1, sizeof(refdb_test_backend));
	GITERR_CHECK_ALLOC(backend);

	git_vector_init(&backend->refs, 0, ref_name_cmp);

	backend->repo = repo;

	backend->parent.exists = &refdb_test_backend__exists;
	backend->parent.lookup = &refdb_test_backend__lookup;
	backend->parent.iterator = &refdb_test_backend__iterator;
	backend->parent.next = &refdb_test_backend__next;
	backend->parent.iterator_free = &refdb_test_backend__iterator_free;
	backend->parent.write = &refdb_test_backend__write;
	backend->parent.delete = &refdb_test_backend__delete;
	backend->parent.free = &refdb_test_backend__free;

	*backend_out = (git_refdb_backend *)backend;
	return 0;
}
