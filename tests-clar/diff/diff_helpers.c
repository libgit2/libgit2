#include "clar_libgit2.h"
#include "diff_helpers.h"

git_tree *resolve_commit_oid_to_tree(
	git_repository *repo,
	const char *partial_oid)
{
	unsigned int len = (unsigned int)strlen(partial_oid);
	git_oid oid;
	git_object *obj = NULL;
	git_tree *tree = NULL;

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

int diff_file_fn(
	void *cb_data,
	git_diff_delta *delta,
	float progress)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(progress);

	e->	at_least_one_of_them_is_binary = delta->binary;

	e->files++;
	switch (delta->status) {
	case GIT_DELTA_ADDED: e->file_adds++; break;
	case GIT_DELTA_DELETED: e->file_dels++; break;
	case GIT_DELTA_MODIFIED: e->file_mods++; break;
	case GIT_DELTA_IGNORED: e->file_ignored++; break;
	case GIT_DELTA_UNTRACKED: e->file_untracked++; break;
	case GIT_DELTA_UNMODIFIED: e->file_unmodified++; break;
	default: break;
	}
	return 0;
}

int diff_hunk_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(delta);
	GIT_UNUSED(header);
	GIT_UNUSED(header_len);

	e->hunks++;
	e->hunk_old_lines += range->old_lines;
	e->hunk_new_lines += range->new_lines;
	return 0;
}

int diff_line_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);
	GIT_UNUSED(content);
	GIT_UNUSED(content_len);

	e->lines++;
	switch (line_origin) {
	case GIT_DIFF_LINE_CONTEXT:
		e->line_ctxt++;
		break;
	case GIT_DIFF_LINE_ADDITION:
	case GIT_DIFF_LINE_ADD_EOFNL:
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_DELETION:
	case GIT_DIFF_LINE_DEL_EOFNL:
		e->line_dels++;
		break;
	default:
		break;
	}
	return 0;
}
