#include "fileops.h"
#include "git2/diff.h"

extern git_tree *resolve_commit_oid_to_tree(
	git_repository *repo, const char *partial_oid);

typedef struct {
	int files;
	int files_binary;

	int file_status[10]; /* indexed by git_delta_t value */

	int hunks;
	int hunk_new_lines;
	int hunk_old_lines;

	int lines;
	int line_ctxt;
	int line_adds;
	int line_dels;
} diff_expects;

typedef struct {
	const char *path;
	const char *matched_pathspec;
} notify_expected;

extern int diff_file_cb(
	const git_diff_delta *delta,
	float progress,
	void *cb_data);

extern int diff_hunk_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	const char *header,
	size_t header_len,
	void *cb_data);

extern int diff_line_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len,
	void *cb_data);

extern int diff_foreach_via_iterator(
	git_diff_list *diff,
	git_diff_file_cb file_cb,
	git_diff_hunk_cb hunk_cb,
	git_diff_data_cb line_cb,
	void *data);

extern void diff_print(FILE *fp, git_diff_list *diff);

