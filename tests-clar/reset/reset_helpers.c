#include "clar_libgit2.h"
#include "reset_helpers.h"

void retrieve_target_from_oid(git_object **object_out, git_repository *repo, const char *sha)
{
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, sha));
	cl_git_pass(git_object_lookup(object_out, repo, &oid, GIT_OBJ_ANY));
}
