#include "clar_libgit2.h"
#include "odb.h"
#include "git2/odb_backend.h"
#include "pack.h"

static git_odb *_odb;
static git_repository *_repo;
static int nobj;

void test_odb_foreach__initialize(void)
{
	cl_git_pass(git_repository_open(&_repo, cl_fixture("testrepo.git")));
	git_repository_odb(&_odb, _repo);
}

void test_odb_foreach__cleanup(void)
{
	git_odb_free(_odb);
	git_repository_free(_repo);
}

static int foreach_cb(git_oid *oid, void *data)
{
	GIT_UNUSED(data);
	GIT_UNUSED(oid);

	nobj++;

	return 0;
}

void test_odb_foreach__foreach(void)
{
	cl_git_pass(git_odb_foreach(_odb, foreach_cb, NULL));
	cl_assert(nobj == 1683);
}
