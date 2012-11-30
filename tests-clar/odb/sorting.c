#include "clar_libgit2.h"
#include "git2/odb_backend.h"
#include "odb.h"

typedef struct {
	git_odb_backend base;
	int position;
} fake_backend;

static git_odb_backend *new_backend(int position)
{
	fake_backend *b;

	b = git__calloc(1, sizeof(fake_backend));
	if (b == NULL)
		return NULL;

	b->base.version = GIT_ODB_BACKEND_VERSION;
	b->position = position;
	return (git_odb_backend *)b;
}

static void check_backend_sorting(git_odb *odb)
{
	unsigned int i;

	for (i = 0; i < odb->backends.length; ++i) {
		fake_backend *internal =
			*((fake_backend **)git_vector_get(&odb->backends, i));

		cl_assert(internal != NULL);
		cl_assert(internal->position == (int)i);
	}
}

static git_odb *_odb;

void test_odb_sorting__initialize(void)
{
	cl_git_pass(git_odb_new(&_odb));
}

void test_odb_sorting__cleanup(void)
{
	git_odb_free(_odb);
	_odb = NULL;
}

void test_odb_sorting__basic_backends_sorting(void)
{
	cl_git_pass(git_odb_add_backend(_odb, new_backend(0), 5));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(2), 3));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(1), 4));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(3), 1));

	check_backend_sorting(_odb);
}

void test_odb_sorting__alternate_backends_sorting(void)
{
	cl_git_pass(git_odb_add_backend(_odb, new_backend(0), 5));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(2), 3));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(1), 4));
	cl_git_pass(git_odb_add_backend(_odb, new_backend(3), 1));
	cl_git_pass(git_odb_add_alternate(_odb, new_backend(4), 5));
	cl_git_pass(git_odb_add_alternate(_odb, new_backend(6), 3));
	cl_git_pass(git_odb_add_alternate(_odb, new_backend(5), 4));
	cl_git_pass(git_odb_add_alternate(_odb, new_backend(7), 1));

	check_backend_sorting(_odb);
}
