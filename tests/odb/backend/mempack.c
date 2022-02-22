#include "clar_libgit2.h"
#include "repository.h"
#include "backend_helpers.h"
#include "git2/sys/mempack.h"
#include "git2/sys/repository.h"

static git_odb *_odb;
static git_odb_backend *backend;
static git_oid _oid;
static git_odb_object *_obj;
static git_repository *_repo;

static const char DATA[] = "data";

void test_odb_backend_mempack__initialize(void)
{
	cl_git_pass(git_mempack_new(&backend));
	cl_git_pass(git_odb_new(&_odb));
	cl_git_pass(git_odb_add_backend(_odb, backend, 10));
	cl_git_pass(git_repository_wrap_odb(&_repo, _odb));
}

void test_odb_backend_mempack__cleanup(void)
{
	git_odb_object_free(_obj);
	git_odb_free(_odb);
	git_repository_free(_repo);
}

void test_odb_backend_mempack__write_succeeds(void)
{
	cl_git_pass(git_odb_write(&_oid, _odb, DATA, strlen(DATA) + 1, GIT_OBJECT_BLOB));
	cl_git_pass(git_odb_read(&_obj, _odb, &_oid));
}

void test_odb_backend_mempack__read_of_missing_object_fails(void)
{
	cl_git_pass(git_oid_fromstr(&_oid, "f6ea0495187600e7b2288c8ac19c5886383a4633"));
	cl_git_fail_with(GIT_ENOTFOUND, git_odb_read(&_obj, _odb, &_oid));
}

void test_odb_backend_mempack__exists_of_missing_object_fails(void)
{
	cl_git_pass(git_oid_fromstr(&_oid, "f6ea0495187600e7b2288c8ac19c5886383a4633"));
	cl_assert(git_odb_exists(_odb, &_oid) == 0);
}

void test_odb_backend_mempack__exists_with_existing_objects_succeeds(void)
{
	cl_git_pass(git_odb_write(&_oid, _odb, DATA, strlen(DATA) + 1, GIT_OBJECT_BLOB));
	cl_assert(git_odb_exists(_odb, &_oid) == 1);
}

void test_odb_backend_mempack__blob_create_from_buffer_succeeds(void)
{
	cl_git_pass(git_blob_create_from_buffer(&_oid, _repo, DATA, strlen(DATA) + 1));
	cl_assert(git_odb_exists(_odb, &_oid) == 1);
}

void test_odb_backend_mempack__dump_to_pack_dir(void){
	git_str object_path = GIT_STR_INIT;
	git_str pack_path = GIT_STR_INIT;
	git_buf pack_filename = GIT_BUF_INIT;

	test_odb_backend_mempack__cleanup();
	_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_odb__weakptr(&_odb, _repo));
	cl_git_pass(git_mempack_new_ext(&backend, 0));
	cl_git_pass(git_odb_add_backend(_odb, backend, 1000));
	cl_git_pass(git_odb_write(&_oid, _odb, DATA, strlen(DATA) + 1, GIT_OBJECT_BLOB));
	cl_assert(git_odb_exists(_odb, &_oid) == 1);
	cl_git_pass(git_mempack_dump_to_pack_dir(&pack_filename, _repo, backend));

	cl_git_pass(git_repository__item_path(&object_path, _repo, GIT_REPOSITORY_ITEM_OBJECTS));

	/* Assert packfile and index were written to pack directory. */
	cl_git_pass(git_str_joinpath(&pack_path, git_str_cstr(&object_path), "pack"));
	cl_git_pass(git_str_joinpath(&pack_path, git_str_cstr(&pack_path), pack_filename.ptr));
	cl_assert(git_fs_path_exists(git_str_cstr(&pack_path)) == 1);

	git_str_rtruncate_at_char(&pack_path, '.');
	cl_git_pass(git_str_puts(&pack_path, ".idx"));
	cl_assert(git_fs_path_exists(git_str_cstr(&pack_path)) == 1);

	/* Close and reopen a new ODB with no mempack backend, make sure object is written to pack dir. */
	git_odb_open(&_odb, git_str_cstr(&object_path));
	git_repository_set_odb(_repo, _odb);
	cl_assert(git_odb_exists(_odb, &_oid) == 1);
}
