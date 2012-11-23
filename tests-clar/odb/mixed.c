#include "clar_libgit2.h"
#include "odb.h"

static git_odb *_odb;

void test_odb_mixed__initialize(void)
{
	cl_git_pass(git_odb_open(&_odb, cl_fixture("duplicate.git/objects")));
}

void test_odb_mixed__cleanup(void)
{
	git_odb_free(_odb);
	_odb = NULL;
}

void test_odb_mixed__dup_oid(void) {
	const char hex[] = "ce013625030ba8dba906f756967f9e9ca394464a";
	git_oid oid;
	git_odb_object *obj;
	cl_git_pass(git_oid_fromstr(&oid, hex));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, GIT_OID_HEXSZ));
	git_odb_object_free(obj);
}

