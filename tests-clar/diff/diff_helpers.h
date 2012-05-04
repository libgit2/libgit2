#include "fileops.h"
#include "git2/diff.h"

extern git_tree *resolve_commit_oid_to_tree(
	git_repository *repo, const char *partial_oid);

typedef struct {
	int files;
	int file_adds;
	int file_dels;
	int file_mods;
	int file_ignored;
	int file_untracked;
	int file_unmodified;

	int hunks;
	int hunk_new_lines;
	int hunk_old_lines;

	int lines;
	int line_ctxt;
	int line_adds;
	int line_dels;

	bool at_least_one_of_them_is_binary;
} diff_expects;

extern int diff_file_fn(
	void *cb_data,
	git_diff_delta *delta,
	float progress);

extern int diff_hunk_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len);

extern int diff_line_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len);

