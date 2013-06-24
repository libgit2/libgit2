/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/attr.h"
#include "git2/blob.h"
#include "git2/index.h"
#include "git2/sys/filter.h"

#include "common.h"
#include "fileops.h"
#include "hash.h"
#include "filter.h"
#include "buf_text.h"
#include "repository.h"

typedef enum {
	GIT_CRLF_GUESS = -1,
	GIT_CRLF_BINARY = 0,
	GIT_CRLF_TEXT,
	GIT_CRLF_INPUT,
	GIT_CRLF_CRLF,
	GIT_CRLF_AUTO,
} git_crlf_t;

struct crlf_attrs {
	int crlf_action;
	int eol;
};

typedef struct {
	git_filter base;
	git_repository *repo;
} git_filter_crlf;

static int check_crlf(const char *value)
{
	if (GIT_ATTR_TRUE(value))
		return GIT_CRLF_TEXT;

	if (GIT_ATTR_FALSE(value))
		return GIT_CRLF_BINARY;

	if (GIT_ATTR_UNSPECIFIED(value))
		return GIT_CRLF_GUESS;

	if (strcmp(value, "input") == 0)
		return GIT_CRLF_INPUT;

	if (strcmp(value, "auto") == 0)
		return GIT_CRLF_AUTO;

	return GIT_CRLF_GUESS;
}

static int check_eol(const char *value)
{
	if (GIT_ATTR_UNSPECIFIED(value))
		return GIT_EOL_UNSET;

	if (strcmp(value, "lf") == 0)
		return GIT_EOL_LF;

	if (strcmp(value, "crlf") == 0)
		return GIT_EOL_CRLF;

	return GIT_EOL_UNSET;
}

static int crlf_input_action(struct crlf_attrs *ca)
{
	if (ca->crlf_action == GIT_CRLF_BINARY)
		return GIT_CRLF_BINARY;

	if (ca->eol == GIT_EOL_LF)
		return GIT_CRLF_INPUT;

	if (ca->eol == GIT_EOL_CRLF)
		return GIT_CRLF_CRLF;

	return ca->crlf_action;
}

static int crlf_load_attributes(
	struct crlf_attrs *ca,
	git_repository *repo,
	const char *path)
{
#define NUM_CONV_ATTRS 3

	static const char *attr_names[NUM_CONV_ATTRS] = {
		"crlf", "eol", "text",
	};

	const char *attr_vals[NUM_CONV_ATTRS];
	int error;

	error = git_attr_get_many(attr_vals,
		repo, 0, path, NUM_CONV_ATTRS, attr_names);

	if (error < 0 && error != GIT_ENOTFOUND)
		return error;
	else if (error == GIT_ENOTFOUND) {
		ca->crlf_action = GIT_CRLF_GUESS;
		ca->eol = GIT_EOL_UNSET;
	} else {
		ca->crlf_action = check_crlf(attr_vals[2]); /* text */

		if (ca->crlf_action == GIT_CRLF_GUESS)
			ca->crlf_action = check_crlf(attr_vals[0]); /* clrf */

		ca->eol = check_eol(attr_vals[1]); /* eol */
	}

	/*
	 * Use the core Git logic to see if we should perform CRLF for this file
	 * based on its attributes & the value of `core.autocrlf`
	 */
	ca->crlf_action = crlf_input_action(ca);

	/*
	 * Determine if we should apply a filter based on this.
	 */
	if (ca->crlf_action == GIT_CRLF_BINARY)
		return 0;

	if (ca->crlf_action == GIT_CRLF_GUESS) {
		int auto_crlf;

		if ((error = git_repository__cvar(
			&auto_crlf, repo, GIT_CVAR_AUTO_CRLF)) < 0)
			return error;

		if (auto_crlf == GIT_AUTO_CRLF_FALSE)
			return 0;
	}

	return 1;
}

static int has_cr_in_index(git_filter_crlf *filter, const char *path)
{
	git_index *index;
	const git_index_entry *entry;
	git_blob *blob;
	const void *blobcontent;
	git_off_t blobsize;
	bool found_cr;

	if (git_repository_index__weakptr(&index, filter->repo) < 0) {
		giterr_clear();
		return false;
	}

	if (!(entry = git_index_get_bypath(index, path, 0)) &&
		!(entry = git_index_get_bypath(index, path, 1)))
		return false;

	if (!S_ISREG(entry->mode)) /* don't crlf filter non-blobs */
		return true;

	if (git_blob_lookup(&blob, filter->repo, &entry->oid) < 0)
		return false;

	blobcontent = git_blob_rawcontent(blob);
	blobsize    = git_blob_rawsize(blob);
	if (!git__is_sizet(blobsize))
		blobsize = (size_t)-1;

	found_cr = (blobcontent != NULL &&
		blobsize > 0 &&
		memchr(blobcontent, '\r', (size_t)blobsize) != NULL);

	git_blob_free(blob);
	return found_cr;
}

static int crlf_apply_to_odb(
	void **out,
	size_t *out_len,
	git_filter_crlf *filter,
	const char *path,
	const void *in,
	size_t in_len)
{
	struct crlf_attrs ca;
	git_buf source = GIT_BUF_INIT, dest = GIT_BUF_INIT;
	int should_apply, error = 0;

	assert(out && out_len && filter && path && in);

	*out = NULL;
	*out_len = 0;

	/* Empty file? Nothing to do */
	if (in_len == 0)
		goto cleanup;

	/* Create a fake git_buf */
	git_buf_attach(&source, (void *)in, in_len, in_len);

	/* Load gitattributes for the path */
	if ((should_apply = crlf_load_attributes(&ca, filter->repo, path)) < 0)
		return should_apply;

	if (!should_apply)
		return 0;

	/* Heuristics to see if we can skip the conversion.
	 * Straight from Core Git.
	 */
	if (ca.crlf_action == GIT_CRLF_AUTO ||
		ca.crlf_action == GIT_CRLF_GUESS) {

		git_buf_text_stats stats;

		/* Check heuristics for binary vs text... */
		if (git_buf_text_gather_stats(&stats, &source, false))
			goto cleanup;

		/*
		 * We're currently not going to even try to convert stuff
		 * that has bare CR characters. Does anybody do that crazy
		 * stuff?
		 */
		if (stats.cr != stats.crlf)
			goto cleanup;

		if (ca.crlf_action == GIT_CRLF_GUESS) {
			/*
			 * If the file in the index has any CR in it, do not convert.
			 * This is the new safer autocrlf handling.
			 */
			if (has_cr_in_index(filter, path))
				goto cleanup;
		}

		if (!stats.cr)
			goto cleanup;
	}

	/* Actually drop the carriage returns */
	if ((error = git_buf_text_crlf_to_lf(&dest, &source)) < 0) {
		if (error == GIT_ENOTFOUND) {
			giterr_clear();
			error = 0;
		}

		goto cleanup;
	}

	*out_len = git_buf_len(&dest);
	*out = git_buf_detach(&dest);

	return 1;

cleanup:
	git_buf_free(&dest);
	return error;
}

static const char *line_ending(struct crlf_attrs *ca)
{
	switch (ca->crlf_action) {
	case GIT_CRLF_BINARY:
	case GIT_CRLF_INPUT:
		return "\n";

	case GIT_CRLF_CRLF:
		return "\r\n";

	case GIT_CRLF_AUTO:
	case GIT_CRLF_TEXT:
	case GIT_CRLF_GUESS:
		break;

	default:
		goto line_ending_error;
	}

	switch (ca->eol) {
	case GIT_EOL_UNSET:
		return GIT_EOL_NATIVE == GIT_EOL_CRLF
			? "\r\n"
			: "\n";

	case GIT_EOL_CRLF:
		return "\r\n";

	case GIT_EOL_LF:
		return "\n";

	default:
		goto line_ending_error;
	}

line_ending_error:
	giterr_set(GITERR_INVALID, "Invalid input to line ending filter");
	return NULL;
}

static int crlf_apply_to_workdir(
	void **out,
	size_t *out_len,
	git_filter_crlf *filter,
	const char *path,
	const void *in,
	size_t in_len)
{
	struct crlf_attrs ca;
	git_buf source = GIT_BUF_INIT, dest = GIT_BUF_INIT;
	const char *workdir_ending = NULL;
	int should_apply, error = 0;

	assert(out && out_len && filter && path && in);

	*out = NULL;
	*out_len = 0;

	/* Empty file? Nothing to do. */
	if (in_len == 0)
		return 0;
		
	/* Load gitattributes for the path */
	if ((should_apply = crlf_load_attributes(&ca, filter->repo, path)) < 0)
		return should_apply;

	if (!should_apply)
		return 0;

	/* Determine proper line ending */
	workdir_ending = line_ending(&ca);

	if (!workdir_ending)
		return 0;

	if (!strcmp("\n", workdir_ending)) /* do nothing for \n ending */
		return 0;

	/* for now, only lf->crlf conversion is supported here */
	assert(!strcmp("\r\n", workdir_ending));

	git_buf_attach(&source, (void *)in, in_len, in_len);

	/* Actually add the carriage returns */
	if ((error = git_buf_text_lf_to_crlf(&dest, &source)) < 0) {

		if (error == GIT_ENOTFOUND) {
			giterr_clear();
			error = 0;
		}

		git_buf_free(&dest);
		return error;
	}

	*out_len = git_buf_len(&dest);
	*out = git_buf_detach(&dest);

	return 1;
}

static int crlf_should_apply(
	git_filter *f,
	const char *path,
	git_filter_mode_t mode)
{
	struct crlf_attrs ca;
	git_filter_crlf *filter = (git_filter_crlf *)f;

	GIT_UNUSED(mode);

	return crlf_load_attributes(&ca, filter->repo, path);
}

static int crlf_apply(
	void **out,
	size_t *out_len,
	git_filter *f,
	const char *path,
	git_filter_mode_t mode,
	const void *in,
	size_t in_len)
{
	git_filter_crlf *filter = (git_filter_crlf *)f;

	return mode == GIT_FILTER_TO_ODB ?
		crlf_apply_to_odb(out, out_len, filter, path, in, in_len) :
		crlf_apply_to_workdir(out, out_len, filter, path, in, in_len);
}

static void crlf_free_buf(void *buf)
{
	git__free(buf);
}

static void crlf_free(git_filter *f)
{
	git_filter_crlf *filter = (git_filter_crlf *)f;

	git__free(filter);
}

int git_filter_crlf_new(git_filter **out, git_repository *repo)
{
	git_filter_crlf *filter;

	if ((filter = git__calloc(1, sizeof(git_filter_crlf))) == NULL) {
		*out = NULL;
		return -1;
	}

	filter->base.version = GIT_FILTER_VERSION;
	filter->base.should_apply = crlf_should_apply;
	filter->base.apply = crlf_apply;
	filter->base.free_buf = crlf_free_buf;
	filter->base.free = crlf_free;
	filter->repo = repo;

	*out = (git_filter *)filter;
	return 0;
}
