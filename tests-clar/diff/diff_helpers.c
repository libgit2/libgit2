#include "clar_libgit2.h"
#include "diff_helpers.h"

git_tree *resolve_commit_oid_to_tree(
	git_repository *repo,
	const char *partial_oid)
{
	size_t len = strlen(partial_oid);
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

int diff_file_cb(
	const git_diff_delta *delta,
	float progress,
	void *payload)
{
	diff_expects *e = payload;

	GIT_UNUSED(progress);

	e->files++;

	if ((delta->flags & GIT_DIFF_FLAG_BINARY) != 0)
		e->files_binary++;

	cl_assert(delta->status <= GIT_DELTA_TYPECHANGE);

	e->file_status[delta->status] += 1;

	return 0;
}

int diff_hunk_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	const char *header,
	size_t header_len,
	void *payload)
{
	diff_expects *e = payload;

	GIT_UNUSED(delta);
	GIT_UNUSED(header);
	GIT_UNUSED(header_len);

	e->hunks++;
	e->hunk_old_lines += range->old_lines;
	e->hunk_new_lines += range->new_lines;
	return 0;
}

int diff_line_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len,
	void *payload)
{
	diff_expects *e = payload;

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
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_ADD_EOFNL:
		/* technically not a line add, but we'll count it as such */
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_DELETION:
		e->line_dels++;
		break;
	case GIT_DIFF_LINE_DEL_EOFNL:
		/* technically not a line delete, but we'll count it as such */
		e->line_dels++;
		break;
	default:
		break;
	}
	return 0;
}

int diff_foreach_via_iterator(
	git_diff_list *diff,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb line_cb,
	void *data)
{
	size_t d, num_d = git_diff_num_deltas(diff);

	for (d = 0; d < num_d; ++d) {
		git_diff_patch *patch;
		const git_diff_delta *delta;
		size_t h, num_h;

		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));
		cl_assert(delta);

		/* call file_cb for this file */
		if (file_cb != NULL && file_cb(delta, (float)d / num_d, data) != 0) {
			git_diff_patch_free(patch);
			goto abort;
		}

		/* if there are no changes, then the patch will be NULL */
		if (!patch) {
			cl_assert(delta->status == GIT_DELTA_UNMODIFIED ||
					  (delta->flags & GIT_DIFF_FLAG_BINARY) != 0);
			continue;
		}

		if (!hunk_cb && !line_cb) {
			git_diff_patch_free(patch);
			continue;
		}

		num_h = git_diff_patch_num_hunks(patch);

		for (h = 0; h < num_h; h++) {
			const git_diff_range *range;
			const char *hdr;
			size_t hdr_len, l, num_l;

			cl_git_pass(git_diff_patch_get_hunk(
				&range, &hdr, &hdr_len, &num_l, patch, h));

			if (hunk_cb && hunk_cb(delta, range, hdr, hdr_len, data) != 0) {
				git_diff_patch_free(patch);
				goto abort;
			}

			for (l = 0; l < num_l; ++l) {
				char origin;
				const char *line;
				size_t line_len;
				int old_lineno, new_lineno;

				cl_git_pass(git_diff_patch_get_line_in_hunk(
					&origin, &line, &line_len, &old_lineno, &new_lineno,
					patch, h, l));

				if (line_cb &&
					line_cb(delta, range, origin, line, line_len, data) != 0) {
					git_diff_patch_free(patch);
					goto abort;
				}
			}
		}

		git_diff_patch_free(patch);
	}

	return 0;

abort:
	giterr_clear();
	return GIT_EUSER;
}

static int diff_print_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin, /**< GIT_DIFF_LINE_... value from above */
	const char *content,
	size_t content_len,
	void *payload)
{
	GIT_UNUSED(payload);
	GIT_UNUSED(delta);
	GIT_UNUSED(range);
	GIT_UNUSED(line_origin);
	GIT_UNUSED(content_len);
	fputs(content, (FILE *)payload);
	return 0;
}

void diff_print(FILE *fp, git_diff_list *diff)
{
	cl_git_pass(git_diff_print_patch(diff, diff_print_cb, fp ? fp : stderr));
}
