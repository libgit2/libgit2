#include "clar_libgit2.h"
#include "git2/sys/odb_backend.h"
#include "backend_helpers.h"

static int fake_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	fake_backend *fake;

	GIT_UNUSED(oid);

	fake = (fake_backend *)backend;

	fake->exists_calls++;

	return (fake->error_code == GIT_OK);
}

static int fake_backend__read(
	void **buffer_p, size_t *len_p, git_otype *type_p,
	git_odb_backend *backend, const git_oid *oid)
{
	fake_backend *fake;

	GIT_UNUSED(buffer_p);
	GIT_UNUSED(len_p);
	GIT_UNUSED(type_p);
	GIT_UNUSED(oid);

	fake = (fake_backend *)backend;

	fake->read_calls++;

	*len_p = 0;
	*buffer_p = NULL;
	*type_p = GIT_OBJ_BLOB;

	return fake->error_code;
}

static int fake_backend__read_header(
	size_t *len_p, git_otype *type_p,
	git_odb_backend *backend, const git_oid *oid)
{
	fake_backend *fake;

	GIT_UNUSED(len_p);
	GIT_UNUSED(type_p);
	GIT_UNUSED(oid);

	fake = (fake_backend *)backend;

	fake->read_header_calls++;

	*len_p = 0;
	*type_p = GIT_OBJ_BLOB;

	return fake->error_code;
}

static int fake_backend__read_prefix(
	git_oid *out_oid, void **buffer_p, size_t *len_p, git_otype *type_p,
	git_odb_backend *backend, const git_oid *short_oid, size_t len)
{
	fake_backend *fake;

	GIT_UNUSED(buffer_p);
	GIT_UNUSED(len_p);
	GIT_UNUSED(type_p);
	GIT_UNUSED(short_oid);
	GIT_UNUSED(len);

	fake = (fake_backend *)backend;

	fake->read_prefix_calls++;

	git_oid_cpy(out_oid, &fake->oid);
	*len_p = 0;
	*buffer_p = NULL;
	*type_p = GIT_OBJ_BLOB;

	return fake->error_code;
}

static void fake_backend__free(git_odb_backend *_backend)
{
	fake_backend *backend;

	backend = (fake_backend *)_backend;

	git__free(backend);
}

int build_fake_backend(
	git_odb_backend **out,
	git_error_code error_code,
	const git_oid *oid)
{
	fake_backend *backend;

	backend = git__calloc(1, sizeof(fake_backend));
	GITERR_CHECK_ALLOC(backend);

	backend->parent.version = GIT_ODB_BACKEND_VERSION;

	backend->parent.refresh = NULL;
	backend->error_code = error_code;

	backend->parent.read = fake_backend__read;
	backend->parent.read_prefix = fake_backend__read_prefix;
	backend->parent.read_header = fake_backend__read_header;
	backend->parent.exists = fake_backend__exists;
	backend->parent.free = &fake_backend__free;

	git_oid_cpy(&backend->oid, oid);

	*out = (git_odb_backend *)backend;

	return 0;
}
