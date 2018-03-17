/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/mailmap.h"

#include "blob.h"
#include "commit.h"
#include "git2/common.h"
#include "git2/repository.h"
#include "git2/revparse.h"
#include "git2/sys/commit.h"

/**
 * Helper type and methods for the mailmap parser
 */
typedef struct char_range {
	const char *p;
	size_t len;
} char_range;

static const char_range NULL_RANGE = {0};

/* Split a range at the first instance of 'c'. Returns whether 'c' was found */
static bool range_split(
	char_range range,
	char c,
	char_range *before,
	char_range *after)
{
	const char *off;

	*before = *after = NULL_RANGE;
	before->p = range.p;
	off = memchr(range.p, c, range.len);
	if (!off) {
		before->len = range.len;
		return false;
	}

	before->len = off - range.p;
	after->p = off + 1;
	after->len = (range.p + range.len) - after->p;
	return true;
}

/* Trim whitespace from the beginning and end of the range */
static void range_trim(char_range *range) {
	while (range->len > 0 && git__isspace(range->p[0])) {
		++range->p;
		--range->len;
	}
	while (range->len > 0 && git__isspace(range->p[range->len - 1]))
		--range->len;
}

/**
 * If `buf` is not NULL, copies range into it with a '\0', and bumps buf.
 * If `size` is not NULL, adds the number of bytes to be written to it.
 * returns a pointer to the copied string, or NULL.
 */
static const char *range_copyz(char **buf, size_t *size, char_range src)
{
	char *s = NULL;
	if (src.p == NULL)
		return NULL;

	if (size)
		*size += src.len + 1;

	if (buf) {
		s = *buf;
		memcpy(s, src.p, src.len);
		s[src.len] = '\0';
		*buf += src.len + 1;
	}
	return s;
}

struct git_mailmap {
	git_vector entries;
};

/**
 * Parse a single entry out of a mailmap file.
 * Advances the `file` range past the parsed entry.
 */
static int git_mailmap_parse_single(
	char_range *file,
	bool *found,
	char_range *real_name,
	char_range *real_email,
	char_range *replace_name,
	char_range *replace_email)
{
	char_range line, comment, name_a, email_a, name_b, email_b;
	bool two_emails = false;

	*found = false;
	*real_name = NULL_RANGE;
	*real_email = NULL_RANGE;
	*replace_name = NULL_RANGE;
	*replace_email = NULL_RANGE;

	while (file->len > 0) {
		/* Get the line, and remove any comments */
		range_split(*file, '\n', &line, file);
		range_split(line, '#', &line, &comment);

		/* Skip blank lines */
		range_trim(&line);
		if (line.len == 0)
			continue;

		/* Get the first name and email */
		if (!range_split(line, '<', &name_a, &line))
			return -1; /* garbage in line */
		if (!range_split(line, '>', &email_a, &line))
			return -1; /* unfinished <> pair */

		/* Get an optional second name and/or email */
		two_emails = range_split(line, '<', &name_b, &line);
		if (two_emails && !range_split(line, '>', &email_b, &line))
			return -1; /* unfinished <> pair */

		/* Trim whitespace from around names */
		range_trim(&name_a);
		range_trim(&name_b);

		*found = true;
		if (name_a.len > 0)
			*real_name = name_a;

		if (two_emails) {
			if (email_a.len > 0)
				*real_email = email_a;
			*replace_email = email_b;

			if (name_b.len > 0)
				*replace_name = name_b;
		} else {
			*replace_email = email_a;
		}
		break;
	}

	return 0;
}

int git_mailmap_parse(
	git_mailmap **mailmap,
	const char *data,
	size_t size)
{
	char_range file = { data, size };
	git_mailmap_entry* entry = NULL;
	int error = 0;

	if (memchr(data, '\0', size) != NULL)
		return -1; /* data may not contain '\0's */

	*mailmap = git__calloc(1, sizeof(git_mailmap));
	if (!*mailmap)
		return -1;

	/* XXX: Is it worth it to precompute the size? */
	error = git_vector_init(&(*mailmap)->entries, 0, NULL);
	if (error < 0)
		goto cleanup;

	while (file.len > 0) {
		bool found = false;
		char_range real_name, real_email, replace_name, replace_email;
		size_t size = 0;
		char *buf = NULL;

		error = git_mailmap_parse_single(
			&file, &found,
			&real_name, &real_email,
			&replace_name, &replace_email);
		if (error < 0 || !found) {
			error = 0;
			continue;
		}

		/* Compute how much space we'll need to store our entry */
		size = sizeof(git_mailmap_entry);
		range_copyz(NULL, &size, real_name);
		range_copyz(NULL, &size, real_email);
		range_copyz(NULL, &size, replace_name);
		range_copyz(NULL, &size, replace_email);

		entry = git__malloc(size);
		if (!entry) {
			error = -1;
			goto cleanup;
		}
		entry->version = GIT_MAILMAP_ENTRY_VERSION;

		buf = (char*)(entry + 1);
		entry->real_name = range_copyz(&buf, NULL, real_name);
		entry->real_email = range_copyz(&buf, NULL, real_email);
		entry->replace_name = range_copyz(&buf, NULL, replace_name);
		entry->replace_email = range_copyz(&buf, NULL, replace_email);
		assert(buf == ((char*)entry) + size);

		error = git_vector_insert(&(*mailmap)->entries, entry);
		if (error < 0)
			goto cleanup;
		entry = NULL;
	}

cleanup:
	git__free(entry);
	if (error < 0)
		git_mailmap_free(*mailmap);
		*mailmap = NULL;
	}
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
	const char **name_out,
	const char **email_out,
	const git_mailmap *mailmap,
	const char *name,
	const char *email)
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
	const git_mailmap *mailmap,
	const char *name,
	const char *email)
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

static int git_mailmap_from_bare_repo(
	git_mailmap **mailmap,
	git_repository *repo)
{
	git_reference *head = NULL;
	git_object *tree = NULL;
	git_blob *blob = NULL;
	const char *content = NULL;
	git_off_t size = 0;
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
		(git_object **) &blob,
		treeish,
		".mailmap",
		GIT_OBJ_BLOB);
	if (error < 0)
		goto cleanup;

	content = git_blob_rawcontent(blob);
	size = git_blob_rawsize(blob);

	error = git_mailmap_parse(mailmap, content, size);
	if (error < 0)
		goto cleanup;

cleanup:
	git_reference_free(head);
	git_object_free(tree);
	git_blob_free(blob);

	return error;
}

static int git_mailmap_from_workdir_repo(
	git_mailmap **mailmap,
	git_repository *repo)
{
	git_buf path = GIT_BUF_INIT;
	git_buf data = GIT_BUF_INIT;
	int error;

	assert(!git_repository_is_bare(repo));

	/* In non-bare repositories, .mailmap should be read from the workdir */
	error = git_buf_joinpath(&path, git_repository_workdir(repo), ".mailmap");
	if (error < 0)
		goto cleanup;

	error = git_futils_readbuffer(&data, git_buf_cstr(&path));
	if (error < 0)
		goto cleanup;

	error = git_mailmap_parse(mailmap, data.ptr, data.size);
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
		return git_mailmap_from_bare_repo(mailmap, repo);
	else
		return git_mailmap_from_workdir_repo(mailmap, repo);
}
