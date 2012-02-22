#include "clar_libgit2.h"
#include "diff_helpers.h"

git_tree *resolve_commit_oid_to_tree(
	git_repository *repo,
	const char *partial_oid)
{
	size_t len = strlen(partial_oid);
	git_oid oid;
	git_object *obj;
	git_tree *tree;

	if (git_oid_fromstrn(&oid, partial_oid, len) == 0)
		git_object_lookup_prefix(&obj, repo, &oid, len, GIT_OBJ_ANY);
	cl_assert(obj);
	if (git_object_type(obj) == GIT_OBJ_TREE)
		return (git_tree *)obj;
	cl_assert(git_object_type(obj) == GIT_OBJ_COMMIT);
	cl_git_pass(git_commit_tree(&tree, (git_commit *)obj));
	git_object_free(obj);
	return tree;
}
