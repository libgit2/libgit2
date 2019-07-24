/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "config.h"

#include "config_entries.h"

typedef struct {
	git_config_backend parent;
	git_mutex values_mutex;
	git_config_entries *entries;
	git_config_backend *source;
} config_snapshot_backend;

static int config_error_readonly(void)
{
	git_error_set(GIT_ERROR_CONFIG, "this backend is read-only");
	return -1;
}

static int config_iterator_new_readonly(
	git_config_iterator **iter,
	struct git_config_backend *backend)
{
	config_snapshot_backend *b = GIT_CONTAINER_OF(backend, config_snapshot_backend, parent);
	git_config_entries *entries = NULL;
	int error;

	if ((error = git_config_entries_dup(&entries, b->entries)) < 0 ||
	    (error = git_config_entries_iterator_new(iter, entries)) < 0)
		goto out;

out:
	/* Let iterator delete duplicated entries when it's done */
	git_config_entries_free(entries);
	return error;
}

/* release the map containing the entry as an equivalent to freeing it */
static void free_diskfile_entry(git_config_entry *entry)
{
	git_config_entries *entries = (git_config_entries *) entry->payload;
	git_config_entries_free(entries);
}

static int config_get_readonly(git_config_backend *cfg, const char *key, git_config_entry **out)
{
	config_snapshot_backend *b = GIT_CONTAINER_OF(cfg, config_snapshot_backend, parent);
	git_config_entries *entries = NULL;
	git_config_entry *entry;
	int error = 0;

	if (git_mutex_lock(&b->values_mutex) < 0) {
	    git_error_set(GIT_ERROR_OS, "failed to lock config backend");
	    return -1;
	}

	entries = b->entries;
	git_config_entries_incref(entries);
	git_mutex_unlock(&b->values_mutex);

	if ((error = (git_config_entries_get(&entry, entries, key))) < 0) {
		git_config_entries_free(entries);
		return error;
	}

	entry->free = free_diskfile_entry;
	entry->payload = entries;
	*out = entry;

	return 0;
}

static int config_set_readonly(git_config_backend *cfg, const char *name, const char *value)
{
	GIT_UNUSED(cfg);
	GIT_UNUSED(name);
	GIT_UNUSED(value);

	return config_error_readonly();
}

static int config_set_multivar_readonly(
	git_config_backend *cfg, const char *name, const char *regexp, const char *value)
{
	GIT_UNUSED(cfg);
	GIT_UNUSED(name);
	GIT_UNUSED(regexp);
	GIT_UNUSED(value);

	return config_error_readonly();
}

static int config_delete_multivar_readonly(git_config_backend *cfg, const char *name, const char *regexp)
{
	GIT_UNUSED(cfg);
	GIT_UNUSED(name);
	GIT_UNUSED(regexp);

	return config_error_readonly();
}

static int config_delete_readonly(git_config_backend *cfg, const char *name)
{
	GIT_UNUSED(cfg);
	GIT_UNUSED(name);

	return config_error_readonly();
}

static int config_lock_readonly(git_config_backend *_cfg)
{
	GIT_UNUSED(_cfg);

	return config_error_readonly();
}

static int config_unlock_readonly(git_config_backend *_cfg, int success)
{
	GIT_UNUSED(_cfg);
	GIT_UNUSED(success);

	return config_error_readonly();
}

static void backend_readonly_free(git_config_backend *_backend)
{
	config_snapshot_backend *backend = GIT_CONTAINER_OF(_backend, config_snapshot_backend, parent);

	if (backend == NULL)
		return;

	git_config_entries_free(backend->entries);
	git_mutex_free(&backend->values_mutex);
	git__free(backend);
}

static int config_readonly_open(git_config_backend *cfg, git_config_level_t level, const git_repository *repo)
{
	config_snapshot_backend *b = GIT_CONTAINER_OF(cfg, config_snapshot_backend, parent);
	git_config_entries *entries = NULL;
	git_config_iterator *it = NULL;
	git_config_entry *entry;
	int error;

	/* We're just copying data, don't care about the level or repo*/
	GIT_UNUSED(level);
	GIT_UNUSED(repo);

	if ((error = git_config_entries_new(&entries)) < 0 ||
	    (error = b->source->iterator(&it, b->source)) < 0)
		goto out;

	while ((error = git_config_next(&entry, it)) == 0)
		if ((error = git_config_entries_dup_entry(entries, entry)) < 0)
			goto out;

	if (error < 0) {
		if (error != GIT_ITEROVER)
			goto out;
		error = 0;
	}

	b->entries = entries;

out:
	git_config_iterator_free(it);
	if (error)
		git_config_entries_free(entries);
	return error;
}

int git_config_backend_snapshot(git_config_backend **out, git_config_backend *source)
{
	config_snapshot_backend *backend;

	backend = git__calloc(1, sizeof(config_snapshot_backend));
	GIT_ERROR_CHECK_ALLOC(backend);

	backend->parent.version = GIT_CONFIG_BACKEND_VERSION;
	git_mutex_init(&backend->values_mutex);

	backend->source = source;

	backend->parent.readonly = 1;
	backend->parent.version = GIT_CONFIG_BACKEND_VERSION;
	backend->parent.open = config_readonly_open;
	backend->parent.get = config_get_readonly;
	backend->parent.set = config_set_readonly;
	backend->parent.set_multivar = config_set_multivar_readonly;
	backend->parent.del = config_delete_readonly;
	backend->parent.del_multivar = config_delete_multivar_readonly;
	backend->parent.iterator = config_iterator_new_readonly;
	backend->parent.lock = config_lock_readonly;
	backend->parent.unlock = config_unlock_readonly;
	backend->parent.free = backend_readonly_free;

	*out = &backend->parent;

	return 0;
}
