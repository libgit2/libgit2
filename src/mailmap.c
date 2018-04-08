/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "mailmap.h"

#include "common.h"
#include "path.h"
#include "repository.h"
#include "git2/config.h"
#include "git2/revparse.h"
#include "blob.h"
#include "parse.h"

#define MM_FILE ".mailmap"
#define MM_FILE_CONFIG "mailmap.file"
#define MM_BLOB_CONFIG "mailmap.blob"
#define MM_BLOB_DEFAULT "HEAD:" MM_FILE

static void mailmap_entry_free(git_mailmap_entry *entry)
{
	if (!entry)
		return;

	git__free(entry->real_name);
	git__free(entry->real_email);
	git__free(entry->replace_name);
	git__free(entry->replace_email);
	git__free(entry);
}

/*
 * First we sort by replace_email, then replace_name (if present).
 * Entries with names are greater than entries without.
 */
static int mailmap_entry_cmp(const void *a_raw, const void *b_raw)
{
	const git_mailmap_entry *a = (const git_mailmap_entry *)a_raw;
	const git_mailmap_entry *b = (const git_mailmap_entry *)b_raw;
	int cmp;

	assert(a && b && a->replace_email && b->replace_email);

	cmp = git__strcmp(a->replace_email, b->replace_email);
	if (cmp)
		return cmp;

	/* NULL replace_names are less than not-NULL ones */
	if (a->replace_name == NULL || b->replace_name == NULL)
		return (int)(a->replace_name != NULL) - (int)(b->replace_name != NULL);

	return git__strcmp(a->replace_name, b->replace_name);
}

/* Replace the old entry with the new on duplicate. */
static int mailmap_entry_replace(void **old_raw, void *new_raw)
{
	mailmap_entry_free((git_mailmap_entry *)*old_raw);
	*old_raw = new_raw;
	return GIT_EEXISTS;
}

/* Check if we're at the end of line, w/ comments */
static bool is_eol(git_parse_ctx *ctx)
{
	char c;
	return git_parse_peek(&c, ctx, GIT_PARSE_PEEK_SKIP_WHITESPACE) < 0 || c == '#';
}

static int advance_until(
	const char **start, size_t *len, git_parse_ctx *ctx, char needle)
{
	*start = ctx->line;
	while (ctx->line_len > 0 && *ctx->line != '#' && *ctx->line != needle)
		git_parse_advance_chars(ctx, 1);

	if (ctx->line_len == 0 || *ctx->line == '#')
		return -1; /* end of line */

	*len = ctx->line - *start;
	git_parse_advance_chars(ctx, 1); /* advance past needle */
	return 0;
}

/*
 * Parse a single entry from a mailmap file.
 *
 * The output git_bufs will be non-owning, and should be copied before being
 * persisted.
 */
static int parse_mailmap_entry(
	git_buf *real_name, git_buf *real_email,
	git_buf *replace_name, git_buf *replace_email,
	git_parse_ctx *ctx)
{
	const char *start;
	size_t len;

	git_buf_clear(real_name);
	git_buf_clear(real_email);
	git_buf_clear(replace_name);
	git_buf_clear(replace_email);

	git_parse_advance_ws(ctx);
	if (is_eol(ctx))
		return -1; /* blank line */

	/* Parse the real name */
	if (advance_until(&start, &len, ctx, '<') < 0)
		return -1;

	git_buf_attach_notowned(real_name, start, len);
	git_buf_rtrim(real_name);

	/*
	 * If this is the last email in the line, this is the email to replace,
	 * otherwise, it's the real email.
	 */
	if (advance_until(&start, &len, ctx, '>') < 0)
		return -1;

	/* If we aren't at the end of the line, parse a second name and email */
	if (!is_eol(ctx)) {
		git_buf_attach_notowned(real_email, start, len);

		git_parse_advance_ws(ctx);
		if (advance_until(&start, &len, ctx, '<') < 0)
			return -1;
		git_buf_attach_notowned(replace_name, start, len);
		git_buf_rtrim(replace_name);

		if (advance_until(&start, &len, ctx, '>') < 0)
			return -1;
	}

	git_buf_attach_notowned(replace_email, start, len);

	if (!is_eol(ctx))
		return -1;

	return 0;
}

int git_mailmap_new(git_mailmap **out)
{
	int error;
	git_mailmap *mm = git__calloc(1, sizeof(git_mailmap));
	GITERR_CHECK_ALLOC(mm);

	error = git_vector_init(&mm->entries, 0, mailmap_entry_cmp);
	if (error < 0) {
		git__free(mm);
		return error;
	}
	*out = mm;
	return 0;
}

void git_mailmap_free(git_mailmap *mm)
{
	size_t idx;
	git_mailmap_entry *entry;
	if (!mm)
		return;

	git_vector_foreach(&mm->entries, idx, entry)
		mailmap_entry_free(entry);
	git__free(mm);
}

int git_mailmap_add_entry(
	git_mailmap *mm, const char *real_name, const char *real_email,
	const char *replace_name, const char *replace_email)
{
	int error;
	git_mailmap_entry *entry = git__calloc(1, sizeof(git_mailmap_entry));
	GITERR_CHECK_ALLOC(entry);

	assert(mm && replace_email && *replace_email);

	if (real_name && *real_name) {
		entry->real_name = git__strdup(real_name);
		GITERR_CHECK_ALLOC(entry->real_name);
	}
	if (real_email && *real_email) {
		entry->real_email = git__strdup(real_email);
		GITERR_CHECK_ALLOC(entry->real_email);
	}
	if (replace_name && *replace_name) {
		entry->replace_name = git__strdup(replace_name);
		GITERR_CHECK_ALLOC(entry->replace_name);
	}
	entry->replace_email = git__strdup(replace_email);
	GITERR_CHECK_ALLOC(entry->replace_email);

	error = git_vector_insert_sorted(&mm->entries, entry, mailmap_entry_replace);
	if (error < 0 && error != GIT_EEXISTS)
		mailmap_entry_free(entry);

	return error;
}

int git_mailmap_add_buffer(git_mailmap *mm, const git_buf *buf)
{
	int error;
	git_mailmap_entry *entry = NULL;
	git_parse_ctx ctx;

	/* Scratch buffers containing the real parsed names & emails */
	git_buf real_name = GIT_BUF_INIT;
	git_buf real_email = GIT_BUF_INIT;
	git_buf replace_name = GIT_BUF_INIT;
	git_buf replace_email = GIT_BUF_INIT;

	if (git_buf_contains_nul(buf))
		return -1;

	git_parse_ctx_init(&ctx, buf->ptr, buf->size);

	/* Run the parser */
	while (ctx.remain_len > 0) {
		error = parse_mailmap_entry(
			&real_name, &real_email, &replace_name, &replace_email, &ctx);
		if (error < 0) {
			error = 0; /* Skip lines which don't contain a valid entry */
			git_parse_advance_line(&ctx);
			continue; /* TODO: warn */
		}

		entry = git__calloc(1, sizeof(git_mailmap_entry));
		GITERR_CHECK_ALLOC(entry);

		if (real_name.size > 0) {
			entry->real_name = git__substrdup(real_name.ptr, real_name.size);
			GITERR_CHECK_ALLOC(entry->real_name);
		}
		if (real_email.size > 0) {
			entry->real_email = git__substrdup(real_email.ptr, real_email.size);
			GITERR_CHECK_ALLOC(entry->real_email);
		}
		if (replace_name.size > 0) {
			entry->replace_name = git__substrdup(replace_name.ptr, replace_name.size);
			GITERR_CHECK_ALLOC(entry->replace_name);
		}
		entry->replace_email = git__substrdup(replace_email.ptr, replace_email.size);
		GITERR_CHECK_ALLOC(entry->replace_email);

		error = git_vector_insert_sorted(
			&mm->entries, entry, mailmap_entry_replace);
		if (error < 0 && error != GIT_EEXISTS)
			goto cleanup;

		entry = NULL;
		error = 0;
	}

cleanup:
	mailmap_entry_free(entry);

	/* We never allocate data in these buffers, but better safe than sorry */
	git_buf_free(&real_name);
	git_buf_free(&real_email);
	git_buf_free(&replace_name);
	git_buf_free(&replace_email);
	return error;
}

int git_mailmap_from_buffer(git_mailmap **out, const git_buf *buffer)
{
	int error = git_mailmap_new(out);
	if (error < 0)
		return error;

	error = git_mailmap_add_buffer(*out, buffer);
	if (error < 0) {
		git_mailmap_free(*out);
		*out = NULL;
	}
	return error;
}

static int mailmap_add_blob(
	git_mailmap *mm, git_repository *repo, const char *spec)
{
	git_object *object = NULL;
	git_blob *blob = NULL;
	git_buf content = GIT_BUF_INIT;
	int error;

	assert(mm && repo);

	error = git_revparse_single(&object, repo, spec);
	if (error < 0)
		goto cleanup;

	error = git_object_peel((git_object **)&blob, object, GIT_OBJ_BLOB);
	if (error < 0)
		goto cleanup;

	error = git_blob__getbuf(&content, blob);
	if (error < 0)
		goto cleanup;

	error = git_mailmap_add_buffer(mm, &content);
	if (error < 0)
		goto cleanup;

cleanup:
	git_buf_free(&content);
	git_blob_free(blob);
	git_object_free(object);
	return error;
}

static int mailmap_add_file_ondisk(
	git_mailmap *mm, const char *path, git_repository *repo)
{
	const char *base = repo ? git_repository_workdir(repo) : NULL;
	git_buf fullpath = GIT_BUF_INIT;
	git_buf content = GIT_BUF_INIT;
	int error;

	error = git_path_join_unrooted(&fullpath, path, base, NULL);
	if (error < 0)
		goto cleanup;

	error = git_futils_readbuffer(&content, fullpath.ptr);
	if (error < 0)
		goto cleanup;

	error = git_mailmap_add_buffer(mm, &content);
	if (error < 0)
		goto cleanup;

cleanup:
	git_buf_free(&fullpath);
	git_buf_free(&content);
	return error;
}

/* NOTE: Only expose with an error return, currently never errors */
static void mailmap_add_from_repository(git_mailmap *mm, git_repository *repo)
{
	git_config *config = NULL;
	git_buf spec_buf = GIT_BUF_INIT;
	git_buf path_buf = GIT_BUF_INIT;
	const char *spec = NULL;
	const char *path = NULL;

	assert(mm && repo);

	/* If we're in a bare repo, default blob to 'HEAD:.mailmap' */
	if (repo->is_bare)
		spec = MM_BLOB_DEFAULT;

	/* Try to load 'mailmap.file' and 'mailmap.blob' cfgs from the repo */
	if (git_repository_config(&config, repo) == 0) {
		if (git_config_get_string_buf(&spec_buf, config, MM_BLOB_CONFIG) == 0)
			spec = spec_buf.ptr;
		if (git_config_get_path(&path_buf, config, MM_FILE_CONFIG) == 0)
			path = path_buf.ptr;
	}

	/*
	 * Load mailmap files in order, overriding previous entries with new ones.
	 *  1. The '.mailmap' file in the repository's workdir root,
	 *  2. The blob described by the 'mailmap.blob' config (default HEAD:.mailmap),
	 *  3. The file described by the 'mailmap.file' config.
	 *
	 * We ignore errors from these loads, as these files may not exist, or may
	 * contain invalid information, and we don't want to report that error.
	 *
	 * XXX: Warn?
	 */
	if (!repo->is_bare)
		mailmap_add_file_ondisk(mm, MM_FILE, repo);
	if (spec != NULL)
		mailmap_add_blob(mm, repo, spec);
	if (path != NULL)
		mailmap_add_file_ondisk(mm, path, repo);

	git_buf_free(&spec_buf);
	git_buf_free(&path_buf);
	git_config_free(config);
}

int git_mailmap_from_repository(git_mailmap **out, git_repository *repo)
{
	int error = git_mailmap_new(out);
	if (error < 0)
		return error;
	mailmap_add_from_repository(*out, repo);
	return 0;
}

const git_mailmap_entry *git_mailmap_entry_lookup(
	const git_mailmap *mm, const char *name, const char *email)
{
	int error;
	ssize_t fallback = -1;
	size_t idx;
	git_mailmap_entry *entry;
	git_mailmap_entry needle = { NULL, NULL, NULL, (char *)email };

	assert(email);

	if (!mm)
		return NULL;

	/*
	 * We want to find the place to start looking. so we do a binary search for
	 * the "fallback" nameless entry. If we find it, we advance past it and record
	 * the index.
	 */
	error = git_vector_bsearch(&idx, (git_vector *)&mm->entries, &needle);
	if (error >= 0)
		fallback = idx++;
	else if (error != GIT_ENOTFOUND)
		return NULL;

	/* do a linear search for an exact match */
	for (; idx < git_vector_length(&mm->entries); ++idx) {
		entry = git_vector_get(&mm->entries, idx);

		if (git__strcmp(entry->replace_email, email))
			break; /* it's a different email, so we're done looking */

		assert(entry->replace_name); /* should be specific */
		if (!name || !git__strcmp(entry->replace_name, name))
			return entry;
	}

	if (fallback < 0)
		return NULL; /* no fallback */
	return git_vector_get(&mm->entries, fallback);
}

int git_mailmap_resolve(
	const char **real_name, const char **real_email,
	const git_mailmap *mailmap,
	const char *name, const char *email)
{
	const git_mailmap_entry *entry = NULL;
	assert(name && email);

	*real_name = name;
	*real_email = email;

	if ((entry = git_mailmap_entry_lookup(mailmap, name, email))) {
		if (entry->real_name)
			*real_name = entry->real_name;
		if (entry->real_email)
			*real_email = entry->real_email;
	}
	return 0;
}
