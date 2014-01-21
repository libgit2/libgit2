/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filediff_h__
#define INCLUDE_filediff_h__

#include "xdiff/xdiff.h"

#include "git2/merge.h"

typedef struct {
	const char *label;
	char *path;
	unsigned int mode;
	mmfile_t mmfile;

	git_odb_object *odb_object;
} git_merge_file_input;

#define GIT_MERGE_FILE_INPUT_INIT	{0}

typedef struct {
	bool automergeable;

	const char *path;
	int mode;

	unsigned char *data;
	size_t len;
} git_merge_file_result;

#define GIT_MERGE_FILE_RESULT_INIT	{0}

typedef enum {
	/* Condense non-alphanumeric regions for simplified diff file */
	GIT_MERGE_FILE_SIMPLIFY_ALNUM = (1 << 0),
} git_merge_file_flags_t;

typedef enum {
	/* Create standard conflicted merge files */
	GIT_MERGE_FILE_STYLE_MERGE = 0,

	/* Create diff3-style files */
	GIT_MERGE_FILE_STYLE_DIFF3 = 1,
} git_merge_file_style_t;

typedef struct {
	git_merge_file_favor_t favor;
	git_merge_file_flags_t flags;
	git_merge_file_style_t style;
} git_merge_file_options;

#define GIT_MERGE_FILE_OPTIONS_INIT	{0}

int git_merge_file_input_from_index_entry(
	git_merge_file_input *input,
	git_repository *repo,
	const git_index_entry *entry);

int git_merge_file_input_from_diff_file(
	git_merge_file_input *input,
	git_repository *repo,
	const git_diff_file *file);

int git_merge_files(
	git_merge_file_result *out,
	git_merge_file_input *ancestor,
	git_merge_file_input *ours,
	git_merge_file_input *theirs,
	git_merge_file_options *opts);

GIT_INLINE(void) git_merge_file_input_free(git_merge_file_input *input)
{
	assert(input);
	git__free(input->path);
	git_odb_object_free(input->odb_object);
}

GIT_INLINE(void) git_merge_file_result_free(git_merge_file_result *filediff)
{
	if (filediff == NULL)
		return;

	/* xdiff uses malloc() not git_malloc, so we use free(), not git_free() */
	if (filediff->data != NULL)
		free(filediff->data);
}

#endif
