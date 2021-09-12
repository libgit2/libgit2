/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "buffer.h"
#include "common.h"
#include "diff_generate.h"

#include "git2/email.h"
#include "git2/patch.h"
#include "git2/version.h"

/*
 * Git uses a "magic" timestamp to indicate that an email message
 * is from `git format-patch` (or our equivalent).
 */
#define EMAIL_TIMESTAMP "Mon Sep 17 00:00:00 2001"

GIT_INLINE(int) include_prefix(
	size_t patch_count,
	git_email_create_options *opts)
{
	return ((!opts->subject_prefix || *opts->subject_prefix) ||
	        (opts->flags & GIT_EMAIL_CREATE_ALWAYS_NUMBER) != 0 ||
	        opts->reroll_number ||
		(patch_count > 1 && !(opts->flags & GIT_EMAIL_CREATE_OMIT_NUMBERS)));
}

static int append_prefix(
	git_buf *out,
	size_t patch_idx,
	size_t patch_count,
	git_email_create_options *opts)
{
	const char *subject_prefix = opts->subject_prefix ?
		opts->subject_prefix : "PATCH";

	if (!include_prefix(patch_count, opts))
		return 0;

	git_buf_putc(out, '[');

	if (*subject_prefix)
		git_buf_puts(out, subject_prefix);

	if (opts->reroll_number) {
		if (*subject_prefix)
			git_buf_putc(out, ' ');

		git_buf_printf(out, "v%" PRIuZ, opts->reroll_number);
	}

	if ((opts->flags & GIT_EMAIL_CREATE_ALWAYS_NUMBER) != 0 ||
	    (patch_count > 1 && !(opts->flags & GIT_EMAIL_CREATE_OMIT_NUMBERS))) {
		size_t start_number = opts->start_number ?
			opts->start_number : 1;

		if (*subject_prefix || opts->reroll_number)
			git_buf_putc(out, ' ');

		git_buf_printf(out, "%" PRIuZ "/%" PRIuZ,
		               patch_idx + (start_number - 1),
		               patch_count + (start_number - 1));
	}

	git_buf_puts(out, "] ");

	return git_buf_oom(out) ? -1 : 0;
}

static int append_subject(
	git_buf *out,
	git_commit *commit,
	size_t patch_idx,
	size_t patch_count,
	git_email_create_options *opts)
{
	int error;

	if ((error = git_buf_puts(out, "Subject: ")) < 0 ||
	    (error = append_prefix(out, patch_idx, patch_count, opts)) < 0 ||
	    (error = git_buf_puts(out, git_commit_summary(commit))) < 0 ||
	    (error = git_buf_putc(out, '\n')) < 0)
		return error;

	return 0;
}

static int append_header(
	git_buf *out,
	git_commit *commit,
	size_t patch_idx,
	size_t patch_count,
	git_email_create_options *opts)
{
	const git_signature *author = git_commit_author(commit);
	char id[GIT_OID_HEXSZ];
	char date[GIT_DATE_RFC2822_SZ];
	int error;

	if ((error = git_oid_fmt(id, git_commit_id(commit))) < 0 ||
	    (error = git_buf_printf(out, "From %.*s %s\n", GIT_OID_HEXSZ, id, EMAIL_TIMESTAMP)) < 0 ||
	    (error = git_buf_printf(out, "From: %s <%s>\n", author->name, author->email)) < 0 ||
	    (error = git__date_rfc2822_fmt(date, sizeof(date), &author->when)) < 0 ||
	    (error = git_buf_printf(out, "Date: %s\n", date)) < 0 ||
	    (error = append_subject(out, commit, patch_idx, patch_count, opts)) < 0)
		return error;

	if ((error = git_buf_putc(out, '\n')) < 0)
		return error;

	return 0;
}

static int append_body(git_buf *out, git_commit *commit)
{
	const char *body = git_commit_body(commit);
	size_t body_len;
	int error;

	if (!body)
		return 0;

	body_len = strlen(body);

	if ((error = git_buf_puts(out, body)) < 0)
		return error;

	if (body_len && body[body_len - 1] != '\n')
		error = git_buf_putc(out, '\n');

	return error;
}

static int append_diffstat(git_buf *out, git_diff *diff)
{
	git_diff_stats *stats = NULL;
	unsigned int format_flags;
	int error;

	format_flags = GIT_DIFF_STATS_FULL | GIT_DIFF_STATS_INCLUDE_SUMMARY;

	if ((error = git_diff_get_stats(&stats, diff)) == 0 &&
	    (error = git_diff_stats_to_buf(out, stats, format_flags, 0)) == 0)
		error = git_buf_putc(out, '\n');

	git_diff_stats_free(stats);
	return error;
}

static int append_patches(git_buf *out, git_diff *diff)
{
	size_t i, deltas;
	int error = 0;

	deltas = git_diff_num_deltas(diff);

	for (i = 0; i < deltas; ++i) {
		git_patch *patch = NULL;

		if ((error = git_patch_from_diff(&patch, diff, i)) >= 0)
			error = git_patch_to_buf(out, patch);

		git_patch_free(patch);

		if (error < 0)
			break;
	}

	return error;
}

int git_email_create_from_commit(
	git_buf *out,
	git_commit *commit,
	const git_email_create_options *given_opts)
{
	git_diff *diff = NULL;
	git_email_create_options opts = GIT_EMAIL_CREATE_OPTIONS_INIT;
	git_repository *repo;
	int error = 0;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(commit);

	GIT_ERROR_CHECK_VERSION(given_opts,
		GIT_EMAIL_CREATE_OPTIONS_VERSION,
		"git_email_create_options");

	if (given_opts)
		memcpy(&opts, given_opts, sizeof(git_email_create_options));

	git_buf_sanitize(out);
	git_buf_clear(out);

	repo = git_commit_owner(commit);

	if ((error = git_diff__commit(&diff, repo, commit, &opts.diff_opts)) == 0 &&
	    (error = append_header(out, commit, 1, 1, &opts)) == 0 &&
	    (error = append_body(out, commit)) == 0 &&
	    (error = git_buf_puts(out, "---\n")) == 0 &&
	    (error = append_diffstat(out, diff)) == 0 &&
	    (error = append_patches(out, diff)) == 0)
		error = git_buf_puts(out, "--\nlibgit2 " LIBGIT2_VERSION "\n\n");

	git_diff_free(diff);
	return error;
}
