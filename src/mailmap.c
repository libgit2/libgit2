/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/mailmap.h"

#include "blob.h"
#include "commit.h"
#include "parse.h"
#include "git2/common.h"
#include "git2/repository.h"
#include "git2/revparse.h"
#include "git2/sys/commit.h"

#define MAILMAP_FILE ".mailmap"

struct git_mailmap {
	git_vector entries;
};

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

/* Parse a single entry from a mailmap file.
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

	/* Parse the real name */
	git_parse_advance_ws(ctx);
	if (advance_until(&start, &len, ctx, '<') < 0)
		return -1;

	git_buf_attach_notowned(real_name, start, len);
	git_buf_rtrim(real_name);

	/* If this is the last email in the line, this is the email to replace,
	 * otherwise, it's the real email. */
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

int git_mailmap_from_buffer(git_mailmap **out, git_buf *buf)
{
	int error;
	git_mailmap *mm;
	size_t entry_size;
	char *entry_data;
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

	/* Create our mailmap object */
	mm = git__calloc(1, sizeof(git_mailmap));
	GITERR_CHECK_ALLOC(mm);

	error = git_vector_init(&mm->entries, 0, NULL);
	if (error < 0)
		goto cleanup;

	/* Run the parser */
	while (ctx.remain_len > 0) {
		error = parse_mailmap_entry(
			&real_name, &real_email, &replace_name, &replace_email, &ctx);
		if (error < 0) {
			error = 0; /* Skip lines which don't contain a valid entry */
			git_parse_advance_line(&ctx);
			continue;
		}

		GITERR_CHECK_ALLOC_ADD5(
			&entry_size, sizeof(git_mailmap_entry) + 4 /* 4x'\0' */,
			real_name.size, real_email.size,
			replace_name.size, replace_email.size);
		entry = git__calloc(1, entry_size);
		GITERR_CHECK_ALLOC(entry);

		entry->version = GIT_MAILMAP_ENTRY_VERSION;

		/* Copy strings into the buffer following entry */
		entry_data = (char *)(entry + 1);
		if (real_name.size > 0) {
			memcpy(entry_data, real_name.ptr, real_name.size);
			entry->real_name = entry_data;
			entry_data += real_name.size + 1; /* advance past null from calloc */
		}
		if (real_email.size > 0) {
			memcpy(entry_data, real_email.ptr, real_email.size);
			entry->real_email = entry_data;
			entry_data += real_email.size + 1;
		}
		if (replace_name.size > 0) {
			memcpy(entry_data, replace_name.ptr, replace_name.size);
			entry->replace_name = entry_data;
			entry_data += replace_name.size + 1;
		}
		/* replace_email is always non-null */
		memcpy(entry_data, replace_email.ptr, replace_email.size);
		entry->replace_email = entry_data;

		error = git_vector_insert(&mm->entries, entry);
		if (error < 0)
			goto cleanup;
		entry = NULL;
	}

	/* fill in *out, and make sure we don't free our mailmap */
	*out = mm;
	mm = NULL;

cleanup:
	git__free(entry);
	git_mailmap_free(mm);

	/* We never allocate data in these buffers, but better safe than sorry */
	git_buf_free(&real_name);
	git_buf_free(&real_email);
	git_buf_free(&replace_name);
	git_buf_free(&replace_email);
	return error;
}

void git_mailmap_free(git_mailmap *mailmap)
{
	if (!mailmap)
		return;

	git_vector_free_deep(&mailmap->entries);
	git__free(mailmap);
}

void git_mailmap_resolve(
	const char **name_out, const char **email_out,
	const git_mailmap *mailmap,
	const char *name, const char *email)
{
	const git_mailmap_entry *entry = NULL;
	assert(name && email);

	*name_out = name;
	*email_out = email;

	if (!mailmap)
		return;

	entry = git_mailmap_entry_lookup(mailmap, name, email);
	if (entry) {
		if (entry->real_name)
			*name_out = entry->real_name;
		if (entry->real_email)
			*email_out = entry->real_email;
	}
}

const git_mailmap_entry *git_mailmap_entry_lookup(
	const git_mailmap *mailmap, const char *name, const char *email)
{
	size_t i;
	git_mailmap_entry *entry;
	assert(name && email);

	if (!mailmap)
		return NULL;

	git_vector_foreach(&mailmap->entries, i, entry) {
		if (!git__strcmp(email, entry->replace_email) &&
		    (!entry->replace_name || !git__strcmp(name, entry->replace_name))) {
			return entry;
		}
	}

	return NULL;
}

const git_mailmap_entry *git_mailmap_entry_byindex(
	const git_mailmap *mailmap, size_t idx)
{
	if (mailmap)
		return git_vector_get(&mailmap->entries, idx);
	return NULL;
}

size_t git_mailmap_entry_count(const git_mailmap *mailmap)
{
	if (mailmap)
		return git_vector_length(&mailmap->entries);
	return 0;
}

static int mailmap_from_bare_repo(git_mailmap **mailmap, git_repository *repo)
{
	git_reference *head = NULL;
	git_object *tree = NULL;
	git_blob *blob = NULL;
	git_buf content = GIT_BUF_INIT;
	int error;

	assert(git_repository_is_bare(repo));

	/* In bare repositories, fall back to reading from HEAD's tree */
	error = git_repository_head(&head, repo);
	if (error < 0)
		goto cleanup;

	error = git_reference_peel(&tree, head, GIT_OBJ_TREE);
	if (error < 0)
		goto cleanup;

	error = git_object_lookup_bypath(
		(git_object **) &blob, tree, MAILMAP_FILE, GIT_OBJ_BLOB);
	if (error < 0)
		goto cleanup;

	error = git_blob_filtered_content(&content, blob, MAILMAP_FILE, false);
	if (error < 0)
		goto cleanup;

	error = git_mailmap_from_buffer(mailmap, &content);
	if (error < 0)
		goto cleanup;

cleanup:
	git_buf_free(&content);
	git_blob_free(blob);
	git_object_free(tree);
	git_reference_free(head);

	return error;
}

static int mailmap_from_workdir_repo(git_mailmap **mailmap, git_repository *repo)
{
	git_buf path = GIT_BUF_INIT;
	git_buf data = GIT_BUF_INIT;
	int error;

	assert(!git_repository_is_bare(repo));

	/* In non-bare repositories, .mailmap should be read from the workdir */
	error = git_buf_joinpath(&path, git_repository_workdir(repo), MAILMAP_FILE);
	if (error < 0)
		goto cleanup;

	error = git_futils_readbuffer(&data, git_buf_cstr(&path));
	if (error < 0)
		goto cleanup;

	error = git_mailmap_from_buffer(mailmap, &data);
	if (error < 0)
		goto cleanup;

cleanup:
	git_buf_free(&path);
	git_buf_free(&data);

	return error;
}

int git_mailmap_from_repo(git_mailmap **mailmap, git_repository *repo)
{
	assert(mailmap && repo);

	*mailmap = NULL;

	if (git_repository_is_bare(repo))
		return mailmap_from_bare_repo(mailmap, repo);
	else
		return mailmap_from_workdir_repo(mailmap, repo);
}
