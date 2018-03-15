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

struct mailmap_entry {
	char* to_name;
	char* to_email;
	char* from_name;
	char* from_email;
};

struct git_mailmap {
	git_vector lines;
};

// Returns -1 on failure, length of the string scanned successfully on success,
// guaranteed to be less that `length`.
ssize_t parse_name_and_email(
	const char *line,
	size_t length,
	const char** name,
	size_t* name_len,
	const char** email,
	size_t* email_len,
	bool allow_empty_email)
{
	const char* email_start;
	const char* email_end;
	const char* name_start;
	const char* name_end;

	email_start = memchr(line, '<', length);
	if (!email_start)
		return -1;
	email_end = memchr(email_start, '>', length - (email_start - line));
	if (!email_end)
		return -1;
	assert(email_end > email_start);

	*email_len = email_end - email_start - 1;
	*email = email_start + 1;
	if (*email == email_end && !allow_empty_email)
		return -1;

	// Now look for the name.
	name_start = line;
	while (name_start < email_start && isspace(*name_start))
		++name_start;

	*name = name_start;

	name_end = email_start;
	while (name_end > name_start && isspace(*(name_end - 1)))
		name_end--;

	assert(name_end >= name_start);
	*name_len = name_end - name_start;

	return email_end - line;
}

static void git_mailmap_parse_line(
	git_mailmap* mailmap,
	const char* contents,
	size_t size)
{
	struct mailmap_entry* entry;

	const char* to_name;
	size_t to_name_length;

	const char* to_email;
	size_t to_email_length;

	const char* from_name;
	size_t from_name_length;

	const char* from_email;
	size_t from_email_length;

	ssize_t ret;

	if (!size)
		return;
	if (contents[0] == '#')
		return;

	ret = parse_name_and_email(
		contents,
		size,
		&to_name,
		&to_name_length,
		&to_email,
		&to_email_length,
		false);
	if (ret < 0)
		return;

	ret = parse_name_and_email(
		contents + ret + 1,
		size - ret - 1,
		&from_name,
		&from_name_length,
		&from_email,
		&from_email_length,
		true);
	if (ret < 0)
		return;

	entry = git__malloc(sizeof(struct mailmap_entry));

	entry->to_name = git__strndup(to_name, to_name_length);
	entry->to_email = git__strndup(to_email, to_email_length);
	entry->from_name = git__strndup(from_name, from_name_length);
	entry->from_email = git__strndup(from_email, from_email_length);

	printf("%s <%s> \"%s\" <%s>\n",
		entry->to_name,
		entry->to_email,
		entry->from_name,
		entry->from_email);

	git_vector_insert(&mailmap->lines, entry);
}

static void git_mailmap_parse(
	git_mailmap* mailmap,
	const char* contents,
	size_t size)
{
	size_t start = 0;
	size_t i;
	for (i = 0; i < size; ++i) {
		if (contents[i] != '\n')
			continue;
		git_mailmap_parse_line(mailmap, contents + start, i - start);
		start = i + 1;
	}
}

int git_mailmap_create(git_mailmap** mailmap, git_repository* repo)
{
	git_commit* head = NULL;
	git_blob* mailmap_blob = NULL;
	git_off_t size = 0;
	const char* contents = NULL;
	int ret;

	*mailmap = git__malloc(sizeof(struct git_mailmap));
	git_vector_init(&(*mailmap)->lines, 0, NULL);

	ret = git_revparse_single((git_object **)&head, repo, "HEAD");
	if (ret)
		goto error;

	ret = git_object_lookup_bypath(
			(git_object**) &mailmap_blob,
			(const git_object*) head,
			".mailmap",
			GIT_OBJ_BLOB);
	if (ret)
		goto error;

	contents = git_blob_rawcontent(mailmap_blob);
	size = git_blob_rawsize(mailmap_blob);

	git_mailmap_parse(*mailmap, contents, size);

	return 0;

error:
	assert(ret);

	if (mailmap_blob)
		git_blob_free(mailmap_blob);
	if (head)
		git_commit_free(head);
	git_mailmap_free(*mailmap);
	return ret;
}

void git_mailmap_free(struct git_mailmap* mailmap)
{
	size_t i;
	struct mailmap_entry* line;
	git_vector_foreach(&mailmap->lines, i, line) {
		git__free((char*)line->to_name);
		git__free((char*)line->to_email);
		git__free((char*)line->from_name);
		git__free((char*)line->from_email);
		git__free(line);
	}

	git_vector_clear(&mailmap->lines);
	git__free(mailmap);
}
