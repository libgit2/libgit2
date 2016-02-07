/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "vector.h"
#include "global.h"
#include "merge.h"
#include "git2/merge.h"
#include "git2/sys/merge.h"

static const char *merge_driver_name__text = "text";
static const char *merge_driver_name__union = "union";
static const char *merge_driver_name__binary = "binary";

struct merge_driver_registry {
	git_vector drivers;
};

typedef struct {
	git_merge_driver *driver;
	int initialized;
	char name[GIT_FLEX_ARRAY];
} git_merge_driver_entry;

static struct merge_driver_registry *merge_driver_registry = NULL;

static int merge_driver_apply(
	git_merge_driver *self,
	void **payload,
	const char **path_out,
	uint32_t *mode_out,
	git_buf *merged_out,
	const git_merge_driver_source *src)
{
	git_merge_file_options file_opts = GIT_MERGE_FILE_OPTIONS_INIT;
	git_merge_file_result result = {0};
	int error;

	GIT_UNUSED(self);

	if (src->file_opts)
		memcpy(&file_opts, src->file_opts, sizeof(git_merge_file_options));

	file_opts.favor = (git_merge_file_favor_t) *payload;

	if ((error = git_merge_file_from_index(&result, src->repo,
		src->ancestor, src->ours, src->theirs, &file_opts)) < 0)
		goto done;

	if (!result.automergeable &&
		!(file_opts.flags & GIT_MERGE_FILE_FAVOR__CONFLICTED)) {
		error = GIT_EMERGECONFLICT;
		goto done;
	}

	*path_out = git_merge_file__best_path(
		src->ancestor ? src->ancestor->path : NULL,
		src->ours ? src->ours->path : NULL,
		src->theirs ? src->theirs->path : NULL);

	*mode_out = git_merge_file__best_mode(
		src->ancestor ? src->ancestor->mode : 0,
		src->ours ? src->ours->mode : 0,
		src->theirs ? src->theirs->mode : 0);

	merged_out->ptr = (char *)result.ptr;
	merged_out->size = result.len;
	merged_out->asize = result.len;
	result.ptr = NULL;

done:
	git_merge_file_result_free(&result);
	return error;
}

static int merge_driver_text_check(
	git_merge_driver *self,
	void **payload,
	const char *name,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(self);
	GIT_UNUSED(name);
	GIT_UNUSED(src);

	*payload = (void *)GIT_MERGE_FILE_FAVOR_NORMAL;
	return 0;
}

static int merge_driver_union_check(
	git_merge_driver *self,
	void **payload,
	const char *name,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(self);
	GIT_UNUSED(name);
	GIT_UNUSED(src);

	*payload = (void *)GIT_MERGE_FILE_FAVOR_UNION;
	return 0;
}

static int merge_driver_binary_apply(
	git_merge_driver *self,
	void **payload,
	const char **path_out,
	uint32_t *mode_out,
	git_buf *merged_out,
	const git_merge_driver_source *src)
{
	GIT_UNUSED(self);
	GIT_UNUSED(payload);
	GIT_UNUSED(path_out);
	GIT_UNUSED(mode_out);
	GIT_UNUSED(merged_out);
	GIT_UNUSED(src);

	return GIT_EMERGECONFLICT;
}

static int merge_driver_entry_cmp(const void *a, const void *b)
{
	const git_merge_driver_entry *entry_a = a;
	const git_merge_driver_entry *entry_b = b;

	return strcmp(entry_a->name, entry_b->name);
}

static int merge_driver_entry_search(const void *a, const void *b)
{
	const char *name_a = a;
	const git_merge_driver_entry *entry_b = b;

	return strcmp(name_a, entry_b->name);
}

static void merge_driver_registry_shutdown(void)
{
	struct merge_driver_registry *reg;
	git_merge_driver_entry *entry;
	size_t i;

	if ((reg = git__swap(merge_driver_registry, NULL)) == NULL)
		return;

	git_vector_foreach(&reg->drivers, i, entry) {
		if (entry && entry->driver->shutdown)
			entry->driver->shutdown(entry->driver);

		git__free(entry);
	}

	git_vector_free(&reg->drivers);
	git__free(reg);
}

git_merge_driver git_merge_driver__normal = {
	GIT_MERGE_DRIVER_VERSION,
	NULL,
	NULL,
	NULL,
	merge_driver_apply
};

git_merge_driver git_merge_driver__text = {
	GIT_MERGE_DRIVER_VERSION,
	NULL,
	NULL,
	merge_driver_text_check,
	merge_driver_apply
};

git_merge_driver git_merge_driver__union = {
	GIT_MERGE_DRIVER_VERSION,
	NULL,
	NULL,
	merge_driver_union_check,
	merge_driver_apply
};

git_merge_driver git_merge_driver__binary = {
	GIT_MERGE_DRIVER_VERSION,
	NULL,
	NULL,
	NULL,
	merge_driver_binary_apply
};

static int merge_driver_registry_initialize(void)
{
	struct merge_driver_registry *reg;
	int error = 0;

	if (merge_driver_registry)
		return 0;

	reg = git__calloc(1, sizeof(struct merge_driver_registry));
	GITERR_CHECK_ALLOC(reg);

	if ((error = git_vector_init(&reg->drivers, 3, merge_driver_entry_cmp)) < 0)
		goto done;
	
	reg = git__compare_and_swap(&merge_driver_registry, NULL, reg);

	if (reg != NULL)
		goto done;

	git__on_shutdown(merge_driver_registry_shutdown);

	if ((error = git_merge_driver_register(
			merge_driver_name__text, &git_merge_driver__text)) < 0 ||
		(error = git_merge_driver_register(
			merge_driver_name__union, &git_merge_driver__union)) < 0 ||
		(error = git_merge_driver_register(
			merge_driver_name__binary, &git_merge_driver__binary)) < 0)
		goto done;

done:
	if (error < 0)
		merge_driver_registry_shutdown();

	return error;
}

int git_merge_driver_register(const char *name, git_merge_driver *driver)
{
	git_merge_driver_entry *entry;

	assert(name && driver);

	if (merge_driver_registry_initialize() < 0)
		return -1;

	entry = git__calloc(1, sizeof(git_merge_driver_entry) + strlen(name) + 1);
	GITERR_CHECK_ALLOC(entry);

	strcpy(entry->name, name);
	entry->driver = driver;

	return git_vector_insert_sorted(
		&merge_driver_registry->drivers, entry, NULL);
}

int git_merge_driver_unregister(const char *name)
{
	git_merge_driver_entry *entry;
	size_t pos;
	int error;

	if ((error = git_vector_search2(&pos, &merge_driver_registry->drivers,
		merge_driver_entry_search, name)) < 0)
		return error;

	entry = git_vector_get(&merge_driver_registry->drivers, pos);
	git_vector_remove(&merge_driver_registry->drivers, pos);

	if (entry->initialized && entry->driver->shutdown) {
		entry->driver->shutdown(entry->driver);
		entry->initialized = false;
	}

	git__free(entry);

	return 0;
}

git_merge_driver *git_merge_driver_lookup(const char *name)
{
	git_merge_driver_entry *entry;
	size_t pos;
	int error;

	/* If we've decided the merge driver to use internally - and not
	 * based on user configuration (in merge_driver_name_for_path)
	 * then we can use a hardcoded name instead of looking it up in
	 * the vector.
	 */
	if (name == merge_driver_name__text)
		return &git_merge_driver__text;
	else if (name == merge_driver_name__binary)
		return &git_merge_driver__binary;

	if (merge_driver_registry_initialize() < 0)
		return NULL;

	error = git_vector_search2(&pos, &merge_driver_registry->drivers,
		merge_driver_entry_search, name);

	if (error == GIT_ENOTFOUND)
		return NULL;

	entry = git_vector_get(&merge_driver_registry->drivers, pos);

	if (!entry->initialized) {
		if (entry->driver->initialize &&
			(error = entry->driver->initialize(entry->driver)) < 0)
			return NULL;

		entry->initialized = 1;
	}

	return entry->driver;
}

static int merge_driver_name_for_path(
	const char **out,
	git_repository *repo,
	const char *path,
	const char *default_driver)
{
	const char *value;
	int error;

	*out = NULL;

	if ((error = git_attr_get(&value, repo, 0, path, "merge")) < 0)
		return error;

	/* set: use the built-in 3-way merge driver ("text") */
	if (GIT_ATTR_TRUE(value))
		*out = merge_driver_name__text;

	/* unset: do not merge ("binary") */
	else if (GIT_ATTR_FALSE(value))
		*out = merge_driver_name__binary;

	else if (GIT_ATTR_UNSPECIFIED(value) && default_driver)
		*out = default_driver;

	else if (GIT_ATTR_UNSPECIFIED(value))
		*out = merge_driver_name__text;

	else
		*out = value;

	return 0;
}


GIT_INLINE(git_merge_driver *) merge_driver_lookup_with_wildcard(
	const char *name)
{
	git_merge_driver *driver = git_merge_driver_lookup(name);

	if (driver == NULL)
		driver = git_merge_driver_lookup("*");

	return driver;
}

int git_merge_driver_for_source(
	git_merge_driver **driver_out,
	void **data_out,
	const git_merge_driver_source *src)
{
	const char *path, *driver_name;
	git_merge_driver *driver;
	void *data = NULL;
	int error = 0;

	path = git_merge_file__best_path(
		src->ancestor ? src->ancestor->path : NULL,
		src->ours ? src->ours->path : NULL,
		src->theirs ? src->theirs->path : NULL);

	if ((error = merge_driver_name_for_path(
			&driver_name, src->repo, path, src->default_driver)) < 0)
		return error;

	driver = merge_driver_lookup_with_wildcard(driver_name);

	if (driver && driver->check) {
		error = driver->check(driver, &data, driver_name, src);

		if (error == GIT_PASSTHROUGH)
			driver = &git_merge_driver__text;
		else if (error == GIT_EMERGECONFLICT)
			driver = &git_merge_driver__binary;
		else
			goto done;
	}

	error = 0;
	data = NULL;

	if (driver->check)
		error = driver->check(driver, &data, driver_name, src);

	/* the text and binary drivers must succeed their check */
	assert(error == 0);

done:
	*driver_out = driver;
	*data_out = data;
	return error;
}

