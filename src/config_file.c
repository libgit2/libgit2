/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "config_file.h"

#include "config.h"
#include "filebuf.h"
#include "sysdir.h"
#include "buffer.h"
#include "buf_text.h"
#include "git2/config.h"
#include "git2/sys/config.h"
#include "git2/types.h"
#include "strmap.h"
#include "array.h"
#include "config_parse.h"

#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

typedef struct config_entry_list {
	struct config_entry_list *next;
	git_config_entry *entry;
} config_entry_list;

typedef struct git_config_file_iter {
	git_config_iterator parent;
	config_entry_list *head;
} git_config_file_iter;

/* Max depth for [include] directives */
#define MAX_INCLUDE_DEPTH 10

typedef struct {
	git_atomic refcount;
	git_strmap *map;
	config_entry_list *list;
} diskfile_entries;

typedef struct {
	git_config_backend parent;
	/* mutex to coordinate accessing the values */
	git_mutex values_mutex;
	diskfile_entries *entries;
	const git_repository *repo;
	git_config_level_t level;
} diskfile_header;

typedef struct {
	diskfile_header header;

	git_array_t(git_config_parser) readers;

	bool locked;
	git_filebuf locked_buf;
	git_buf locked_content;

	struct config_file file;
} diskfile_backend;

typedef struct {
	diskfile_header header;

	diskfile_backend *snapshot_from;
} diskfile_readonly_backend;

typedef struct {
	const git_repository *repo;
	const char *file_path;
	diskfile_entries *entries;
	git_config_level_t level;
	unsigned int depth;
} diskfile_parse_state;

static int config_read(diskfile_entries *entries, const git_repository *repo, git_config_file *file, git_config_level_t level, int depth);
static int config_write(diskfile_backend *cfg, const char *orig_key, const char *key, const regex_t *preg, const char *value);
static char *escape_value(const char *ptr);

int git_config_file__snapshot(git_config_backend **out, diskfile_backend *in);
static int config_snapshot(git_config_backend **out, git_config_backend *in);

static int config_error_readonly(void)
{
	giterr_set(GITERR_CONFIG, "this backend is read-only");
	return -1;
}

static void config_entry_list_free(config_entry_list *list)
{
	config_entry_list *next;

	while (list != NULL) {
		next = list->next;

		git__free((char*) list->entry->name);
		git__free((char *) list->entry->value);
		git__free(list->entry);
		git__free(list);

		list = next;
	};
}

int git_config_file_normalize_section(char *start, char *end)
{
	char *scan;

	if (start == end)
		return GIT_EINVALIDSPEC;

	/* Validate and downcase range */
	for (scan = start; *scan; ++scan) {
		if (end && scan >= end)
			break;
		if (isalnum(*scan))
			*scan = (char)git__tolower(*scan);
		else if (*scan != '-' || scan == start)
			return GIT_EINVALIDSPEC;
	}

	if (scan == start)
		return GIT_EINVALIDSPEC;

	return 0;
}

static void config_entry_list_append(config_entry_list **list, config_entry_list *entry)
{
	config_entry_list *head = *list;

	if (head) {
		while (head->next != NULL)
			head = head->next;
		head->next = entry;
	} else {
		*list = entry;
	}
}

/* Add or append the new config option */
static int diskfile_entries_append(diskfile_entries *entries, git_config_entry *entry)
{
	git_strmap_iter pos;
	config_entry_list *existing, *var;
	int error = 0;

	var = git__calloc(1, sizeof(config_entry_list));
	GITERR_CHECK_ALLOC(var);
	var->entry = entry;

	pos = git_strmap_lookup_index(entries->map, entry->name);
	if (!git_strmap_valid_index(entries->map, pos)) {
		git_strmap_insert(entries->map, entry->name, var, &error);

		if (error > 0)
			error = 0;
	} else {
		existing = git_strmap_value_at(entries->map, pos);
		config_entry_list_append(&existing, var);
	}

	var = git__calloc(1, sizeof(config_entry_list));
	GITERR_CHECK_ALLOC(var);
	var->entry = entry;
	config_entry_list_append(&entries->list, var);

	return error;
}

static void diskfile_entries_free(diskfile_entries *entries)
{
	config_entry_list *list = NULL, *next;

	if (!entries)
		return;

	if (git_atomic_dec(&entries->refcount) != 0)
		return;

	git_strmap_foreach_value(entries->map, list, config_entry_list_free(list));
	git_strmap_free(entries->map);

	list = entries->list;
	while (list != NULL) {
		next = list->next;
		git__free(list);
		list = next;
	}

	git__free(entries);
}

/**
 * Take the current values map from the backend and increase its
 * refcount. This is its own function to make sure we use the mutex to
 * avoid the map pointer from changing under us.
 */
static diskfile_entries *diskfile_entries_take(diskfile_header *h)
{
	diskfile_entries *entries;

	if (git_mutex_lock(&h->values_mutex) < 0) {
	    giterr_set(GITERR_OS, "failed to lock config backend");
	    return NULL;
	}

	entries = h->entries;
	git_atomic_inc(&entries->refcount);

	git_mutex_unlock(&h->values_mutex);

	return entries;
}

static int diskfile_entries_alloc(diskfile_entries **out)
{
	diskfile_entries *entries;
	int error;

	entries = git__calloc(1, sizeof(diskfile_entries));
	GITERR_CHECK_ALLOC(entries);

	git_atomic_set(&entries->refcount, 1);

	if ((error = git_strmap_alloc(&entries->map)) < 0)
		git__free(entries);
	else
		*out = entries;

	return error;
}

static void config_file_clear(struct config_file *file)
{
	struct config_file *include;
	uint32_t i;

	if (file == NULL)
		return;

	git_array_foreach(file->includes, i, include) {
		config_file_clear(include);
	}
	git_array_clear(file->includes);

	git__free(file->path);
}

static int config_open(git_config_backend *cfg, git_config_level_t level, const git_repository *repo)
{
	int res;
	diskfile_backend *b = (diskfile_backend *)cfg;

	b->header.level = level;
	b->header.repo = repo;

	if ((res = diskfile_entries_alloc(&b->header.entries)) < 0)
		return res;

	if (!git_path_exists(b->file.path))
		return 0;

	if (res < 0 || (res = config_read(b->header.entries, repo, &b->file, level, 0)) < 0) {
		diskfile_entries_free(b->header.entries);
		b->header.entries = NULL;
	}

	return res;
}

static int config_is_modified(int *modified, struct config_file *file)
{
	git_config_file *include;
	git_buf buf = GIT_BUF_INIT;
	git_oid hash;
	uint32_t i;
	int error = 0;

	*modified = 0;

	if ((error = git_futils_readbuffer(&buf, file->path)) < 0)
		goto out;

	if ((error = git_hash_buf(&hash, buf.ptr, buf.size)) < 0)
		goto out;

	if (!git_oid_equal(&hash, &file->checksum)) {
		*modified = 1;
		goto out;
	}

	git_array_foreach(file->includes, i, include) {
		if ((error = config_is_modified(modified, include)) < 0 || *modified)
			goto out;
	}

out:
	git_buf_dispose(&buf);

	return error;
}

static int config_refresh(git_config_backend *cfg)
{
	diskfile_backend *b = (diskfile_backend *)cfg;
	diskfile_entries *entries = NULL, *tmp;
	git_config_file *include;
	int error, modified;
	uint32_t i;

	if (b->header.parent.readonly)
		return config_error_readonly();

	error = config_is_modified(&modified, &b->file);
	if (error < 0 && error != GIT_ENOTFOUND)
		goto out;

	if (!modified)
		return 0;

	if ((error = diskfile_entries_alloc(&entries)) < 0)
		goto out;

	/* Reparse the current configuration */
	git_array_foreach(b->file.includes, i, include) {
		config_file_clear(include);
	}
	git_array_clear(b->file.includes);

	if ((error = config_read(entries, b->header.repo, &b->file, b->header.level, 0)) < 0)
		goto out;

	if ((error = git_mutex_lock(&b->header.values_mutex)) < 0) {
		giterr_set(GITERR_OS, "failed to lock config backend");
		goto out;
	}

	tmp = b->header.entries;
	b->header.entries = entries;
	entries = tmp;

	git_mutex_unlock(&b->header.values_mutex);

out:
	diskfile_entries_free(entries);

	return (error == GIT_ENOTFOUND) ? 0 : error;
}

static void backend_free(git_config_backend *_backend)
{
	diskfile_backend *backend = (diskfile_backend *)_backend;

	if (backend == NULL)
		return;

	config_file_clear(&backend->file);
	diskfile_entries_free(backend->header.entries);
	git_mutex_free(&backend->header.values_mutex);
	git__free(backend);
}

static void config_iterator_free(
	git_config_iterator* iter)
{
	iter->backend->free(iter->backend);
	git__free(iter);
}

static int config_iterator_next(
	git_config_entry **entry,
	git_config_iterator *iter)
{
	git_config_file_iter *it = (git_config_file_iter *) iter;

	if (!it->head)
		return GIT_ITEROVER;

	*entry = it->head->entry;
	it->head = it->head->next;

	return 0;
}

static int config_iterator_new(
	git_config_iterator **iter,
	struct git_config_backend* backend)
{
	diskfile_header *h;
	git_config_file_iter *it;
	git_config_backend *snapshot;
	diskfile_header *bh = (diskfile_header *) backend;
	int error;

	if ((error = config_snapshot(&snapshot, backend)) < 0)
		return error;

	if ((error = snapshot->open(snapshot, bh->level, bh->repo)) < 0)
		return error;

	it = git__calloc(1, sizeof(git_config_file_iter));
	GITERR_CHECK_ALLOC(it);

	h = (diskfile_header *)snapshot;

	it->parent.backend = snapshot;
	it->head = h->entries->list;
	it->parent.next = config_iterator_next;
	it->parent.free = config_iterator_free;

	*iter = (git_config_iterator *) it;

	return 0;
}

static int config_set(git_config_backend *cfg, const char *name, const char *value)
{
	diskfile_backend *b = (diskfile_backend *)cfg;
	diskfile_entries *entries;
	git_strmap *entry_map;
	char *key, *esc_value = NULL;
	khiter_t pos;
	int rval, ret;

	if ((rval = git_config__normalize_name(name, &key)) < 0)
		return rval;

	if ((entries = diskfile_entries_take(&b->header)) == NULL)
		return -1;
	entry_map = entries->map;

	/*
	 * Try to find it in the existing values and update it if it
	 * only has one value.
	 */
	pos = git_strmap_lookup_index(entry_map, key);
	if (git_strmap_valid_index(entry_map, pos)) {
		config_entry_list *existing = git_strmap_value_at(entry_map, pos);

		if (existing->next != NULL) {
			giterr_set(GITERR_CONFIG, "multivar incompatible with simple set");
			ret = -1;
			goto out;
		}

		if (existing->entry->include_depth) {
			giterr_set(GITERR_CONFIG, "modifying included variable is not supported");
			ret = -1;
			goto out;
		}

		/* don't update if old and new values already match */
		if ((!existing->entry->value && !value) ||
			(existing->entry->value && value &&
			 !strcmp(existing->entry->value, value))) {
			ret = 0;
			goto out;
		}
	}

	/* No early returns due to sanity checks, let's write it out and refresh */

	if (value) {
		esc_value = escape_value(value);
		GITERR_CHECK_ALLOC(esc_value);
	}

	if ((ret = config_write(b, name, key, NULL, esc_value)) < 0)
		goto out;

	ret = config_refresh(cfg);

out:
	diskfile_entries_free(entries);
	git__free(esc_value);
	git__free(key);
	return ret;
}

/* release the map containing the entry as an equivalent to freeing it */
static void free_diskfile_entry(git_config_entry *entry)
{
	diskfile_entries *map = (diskfile_entries *) entry->payload;
	diskfile_entries_free(map);
}

/*
 * Internal function that actually gets the value in string form
 */
static int config_get(git_config_backend *cfg, const char *key, git_config_entry **out)
{
	diskfile_header *h = (diskfile_header *)cfg;
	diskfile_entries *entries;
	git_strmap *entry_map;
	khiter_t pos;
	config_entry_list *var;
	int error = 0;

	if (!h->parent.readonly && ((error = config_refresh(cfg)) < 0))
		return error;

	if ((entries = diskfile_entries_take(h)) == NULL)
		return -1;
	entry_map = entries->map;

	pos = git_strmap_lookup_index(entry_map, key);

	/* no error message; the config system will write one */
	if (!git_strmap_valid_index(entry_map, pos)) {
		diskfile_entries_free(entries);
		return GIT_ENOTFOUND;
	}

	var = git_strmap_value_at(entry_map, pos);
	while (var->next)
		var = var->next;

	*out = var->entry;
	(*out)->free = free_diskfile_entry;
	(*out)->payload = entries;

	return error;
}

static int config_set_multivar(
	git_config_backend *cfg, const char *name, const char *regexp, const char *value)
{
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key;
	regex_t preg;
	int result;

	assert(regexp);

	if ((result = git_config__normalize_name(name, &key)) < 0)
		return result;

	result = p_regcomp(&preg, regexp, REG_EXTENDED);
	if (result != 0) {
		giterr_set_regex(&preg, result);
		result = -1;
		goto out;
	}

	/* If we do have it, set call config_write() and reload */
	if ((result = config_write(b, name, key, &preg, value)) < 0)
		goto out;

	result = config_refresh(cfg);

out:
	git__free(key);
	regfree(&preg);

	return result;
}

static int config_delete(git_config_backend *cfg, const char *name)
{
	config_entry_list *var;
	diskfile_backend *b = (diskfile_backend *)cfg;
	diskfile_entries *map;
	git_strmap *entry_map;
	char *key;
	int result;
	khiter_t pos;

	if ((result = git_config__normalize_name(name, &key)) < 0)
		return result;

	if ((map = diskfile_entries_take(&b->header)) == NULL)
		return -1;
	entry_map = b->header.entries->map;

	pos = git_strmap_lookup_index(entry_map, key);
	git__free(key);

	if (!git_strmap_valid_index(entry_map, pos)) {
		diskfile_entries_free(map);
		giterr_set(GITERR_CONFIG, "could not find key '%s' to delete", name);
		return GIT_ENOTFOUND;
	}

	var = git_strmap_value_at(entry_map, pos);
	diskfile_entries_free(map);

	if (var->entry->include_depth) {
		giterr_set(GITERR_CONFIG, "cannot delete included variable");
		return -1;
	}

	if (var->next != NULL) {
		giterr_set(GITERR_CONFIG, "cannot delete multivar with a single delete");
		return -1;
	}

	if ((result = config_write(b, name, var->entry->name, NULL, NULL)) < 0)
		return result;

	return config_refresh(cfg);
}

static int config_delete_multivar(git_config_backend *cfg, const char *name, const char *regexp)
{
	diskfile_backend *b = (diskfile_backend *)cfg;
	diskfile_entries *map;
	git_strmap *entry_map;
	char *key;
	regex_t preg;
	int result;
	khiter_t pos;

	if ((result = git_config__normalize_name(name, &key)) < 0)
		return result;

	if ((map = diskfile_entries_take(&b->header)) == NULL)
		return -1;
	entry_map = b->header.entries->map;

	pos = git_strmap_lookup_index(entry_map, key);

	if (!git_strmap_valid_index(entry_map, pos)) {
		diskfile_entries_free(map);
		git__free(key);
		giterr_set(GITERR_CONFIG, "could not find key '%s' to delete", name);
		return GIT_ENOTFOUND;
	}

	diskfile_entries_free(map);

	result = p_regcomp(&preg, regexp, REG_EXTENDED);
	if (result != 0) {
		giterr_set_regex(&preg, result);
		result = -1;
		goto out;
	}

	if ((result = config_write(b, name, key, &preg, NULL)) < 0)
		goto out;

	result = config_refresh(cfg);

out:
	git__free(key);
	regfree(&preg);
	return result;
}

static int config_snapshot(git_config_backend **out, git_config_backend *in)
{
	diskfile_backend *b = (diskfile_backend *) in;

	return git_config_file__snapshot(out, b);
}

static int config_lock(git_config_backend *_cfg)
{
	diskfile_backend *cfg = (diskfile_backend *) _cfg;
	int error;

	if ((error = git_filebuf_open(&cfg->locked_buf, cfg->file.path, 0, GIT_CONFIG_FILE_MODE)) < 0)
		return error;

	error = git_futils_readbuffer(&cfg->locked_content, cfg->file.path);
	if (error < 0 && error != GIT_ENOTFOUND) {
		git_filebuf_cleanup(&cfg->locked_buf);
		return error;
	}

	cfg->locked = true;
	return 0;

}

static int config_unlock(git_config_backend *_cfg, int success)
{
	diskfile_backend *cfg = (diskfile_backend *) _cfg;
	int error = 0;

	if (success) {
		git_filebuf_write(&cfg->locked_buf, cfg->locked_content.ptr, cfg->locked_content.size);
		error = git_filebuf_commit(&cfg->locked_buf);
	}

	git_filebuf_cleanup(&cfg->locked_buf);
	git_buf_dispose(&cfg->locked_content);
	cfg->locked = false;

	return error;
}

int git_config_file__ondisk(git_config_backend **out, const char *path)
{
	diskfile_backend *backend;

	backend = git__calloc(1, sizeof(diskfile_backend));
	GITERR_CHECK_ALLOC(backend);

	backend->header.parent.version = GIT_CONFIG_BACKEND_VERSION;
	git_mutex_init(&backend->header.values_mutex);

	backend->file.path = git__strdup(path);
	GITERR_CHECK_ALLOC(backend->file.path);
	git_array_init(backend->file.includes);

	backend->header.parent.open = config_open;
	backend->header.parent.get = config_get;
	backend->header.parent.set = config_set;
	backend->header.parent.set_multivar = config_set_multivar;
	backend->header.parent.del = config_delete;
	backend->header.parent.del_multivar = config_delete_multivar;
	backend->header.parent.iterator = config_iterator_new;
	backend->header.parent.snapshot = config_snapshot;
	backend->header.parent.lock = config_lock;
	backend->header.parent.unlock = config_unlock;
	backend->header.parent.free = backend_free;

	*out = (git_config_backend *)backend;

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
	diskfile_backend *backend = (diskfile_backend *)_backend;

	if (backend == NULL)
		return;

	diskfile_entries_free(backend->header.entries);
	git_mutex_free(&backend->header.values_mutex);
	git__free(backend);
}

static int config_readonly_open(git_config_backend *cfg, git_config_level_t level, const git_repository *repo)
{
	diskfile_readonly_backend *b = (diskfile_readonly_backend *) cfg;
	diskfile_backend *src = b->snapshot_from;
	diskfile_header *src_header = &src->header;
	diskfile_entries *entries;
	int error;

	if (!src_header->parent.readonly && (error = config_refresh(&src_header->parent)) < 0)
		return error;

	/* We're just copying data, don't care about the level or repo*/
	GIT_UNUSED(level);
	GIT_UNUSED(repo);

	if ((entries = diskfile_entries_take(src_header)) == NULL)
		return -1;
	b->header.entries = entries;

	return 0;
}

int git_config_file__snapshot(git_config_backend **out, diskfile_backend *in)
{
	diskfile_readonly_backend *backend;

	backend = git__calloc(1, sizeof(diskfile_readonly_backend));
	GITERR_CHECK_ALLOC(backend);

	backend->header.parent.version = GIT_CONFIG_BACKEND_VERSION;
	git_mutex_init(&backend->header.values_mutex);

	backend->snapshot_from = in;

	backend->header.parent.readonly = 1;
	backend->header.parent.version = GIT_CONFIG_BACKEND_VERSION;
	backend->header.parent.open = config_readonly_open;
	backend->header.parent.get = config_get;
	backend->header.parent.set = config_set_readonly;
	backend->header.parent.set_multivar = config_set_multivar_readonly;
	backend->header.parent.del = config_delete_readonly;
	backend->header.parent.del_multivar = config_delete_multivar_readonly;
	backend->header.parent.iterator = config_iterator_new;
	backend->header.parent.lock = config_lock_readonly;
	backend->header.parent.unlock = config_unlock_readonly;
	backend->header.parent.free = backend_readonly_free;

	*out = (git_config_backend *)backend;

	return 0;
}

static int included_path(git_buf *out, const char *dir, const char *path)
{
	/* From the user's home */
	if (path[0] == '~' && path[1] == '/')
		return git_sysdir_expand_global_file(out, &path[1]);

	return git_path_join_unrooted(out, path, dir, NULL);
}

/* Escape the values to write them to the file */
static char *escape_value(const char *ptr)
{
	git_buf buf;
	size_t len;
	const char *esc;

	assert(ptr);

	len = strlen(ptr);
	if (!len)
		return git__calloc(1, sizeof(char));

	if (git_buf_init(&buf, len) < 0)
		return NULL;

	while (*ptr != '\0') {
		if ((esc = strchr(git_config_escaped, *ptr)) != NULL) {
			git_buf_putc(&buf, '\\');
			git_buf_putc(&buf, git_config_escapes[esc - git_config_escaped]);
		} else {
			git_buf_putc(&buf, *ptr);
		}
		ptr++;
	}

	if (git_buf_oom(&buf)) {
		git_buf_dispose(&buf);
		return NULL;
	}

	return git_buf_detach(&buf);
}

static int parse_include(git_config_parser *reader,
	diskfile_parse_state *parse_data, const char *file)
{
	struct config_file *include;
	git_buf path = GIT_BUF_INIT;
	char *dir;
	int result;

	if ((result = git_path_dirname_r(&path, reader->file->path)) < 0)
		return result;

	dir = git_buf_detach(&path);
	result = included_path(&path, dir, file);
	git__free(dir);

	if (result < 0)
		return result;

	include = git_array_alloc(reader->file->includes);
	memset(include, 0, sizeof(*include));
	git_array_init(include->includes);
	include->path = git_buf_detach(&path);

	result = config_read(parse_data->entries, parse_data->repo,
		include, parse_data->level, parse_data->depth+1);

	if (result == GIT_ENOTFOUND) {
		giterr_clear();
		result = 0;
	}

	return result;
}

static int do_match_gitdir(
	int *matches,
	const git_repository *repo,
	const char *cfg_file,
	const char *value,
	bool case_insensitive)
{
	git_buf path = GIT_BUF_INIT;
	int error, fnmatch_flags;

	if (value[0] == '.' && git_path_is_dirsep(value[1])) {
		git_path_dirname_r(&path, cfg_file);
		git_buf_joinpath(&path, path.ptr, value + 2);
	} else if (value[0] == '~' && git_path_is_dirsep(value[1]))
		git_sysdir_expand_global_file(&path, value + 1);
	else if (!git_path_is_absolute(value))
		git_buf_joinpath(&path, "**", value);
	else
		git_buf_sets(&path, value);

	if (git_buf_oom(&path)) {
		error = -1;
		goto out;
	}

	if (git_path_is_dirsep(value[strlen(value) - 1]))
		git_buf_puts(&path, "**");

	fnmatch_flags = FNM_PATHNAME|FNM_LEADING_DIR;
	if (case_insensitive)
		fnmatch_flags |= FNM_IGNORECASE;

	if ((error = p_fnmatch(path.ptr, git_repository_path(repo), fnmatch_flags)) < 0)
		goto out;

	*matches = (error == 0);

out:
	git_buf_dispose(&path);
	return error;
}

static int conditional_match_gitdir(
	int *matches,
	const git_repository *repo,
	const char *cfg_file,
	const char *value)
{
	return do_match_gitdir(matches, repo, cfg_file, value, false);
}

static int conditional_match_gitdir_i(
	int *matches,
	const git_repository *repo,
	const char *cfg_file,
	const char *value)
{
	return do_match_gitdir(matches, repo, cfg_file, value, true);
}

static const struct {
	const char *prefix;
	int (*matches)(int *matches, const git_repository *repo, const char *cfg, const char *value);
} conditions[] = {
	{ "gitdir:", conditional_match_gitdir },
	{ "gitdir/i:", conditional_match_gitdir_i }
};

static int parse_conditional_include(git_config_parser *reader,
	diskfile_parse_state *parse_data, const char *section, const char *file)
{
	char *condition;
	size_t i;
	int error = 0, matches;

	if (!parse_data->repo)
		return 0;

	condition = git__substrdup(section + strlen("includeIf."),
				   strlen(section) - strlen("includeIf.") - strlen(".path"));

	for (i = 0; i < ARRAY_SIZE(conditions); i++) {
		if (git__prefixcmp(condition, conditions[i].prefix))
			continue;

		if ((error = conditions[i].matches(&matches,
						   parse_data->repo,
						   parse_data->file_path,
						   condition + strlen(conditions[i].prefix))) < 0)
			break;

		if (matches)
			error = parse_include(reader, parse_data, file);

		break;
	}

	git__free(condition);
	return error;
}

static int read_on_variable(
	git_config_parser *reader,
	const char *current_section,
	char *var_name,
	char *var_value,
	const char *line,
	size_t line_len,
	void *data)
{
	diskfile_parse_state *parse_data = (diskfile_parse_state *)data;
	git_buf buf = GIT_BUF_INIT;
	git_config_entry *entry;
	int result = 0;

	GIT_UNUSED(line);
	GIT_UNUSED(line_len);

	git__strtolower(var_name);
	git_buf_printf(&buf, "%s.%s", current_section, var_name);
	git__free(var_name);

	if (git_buf_oom(&buf)) {
		git__free(var_value);
		return -1;
	}

	entry = git__calloc(1, sizeof(git_config_entry));
	GITERR_CHECK_ALLOC(entry);
	entry->name = git_buf_detach(&buf);
	entry->value = var_value;
	entry->level = parse_data->level;
	entry->include_depth = parse_data->depth;

	if ((result = diskfile_entries_append(parse_data->entries, entry)) < 0)
		return result;

	result = 0;

	/* Add or append the new config option */
	if (!git__strcmp(entry->name, "include.path"))
		result = parse_include(reader, parse_data, entry->value);
	else if (!git__prefixcmp(entry->name, "includeif.") &&
	         !git__suffixcmp(entry->name, ".path"))
		result = parse_conditional_include(reader, parse_data,
						   entry->name, entry->value);


	return result;
}

static int config_read(
	diskfile_entries *entries,
	const git_repository *repo,
	git_config_file *file,
	git_config_level_t level,
	int depth)
{
	diskfile_parse_state parse_data;
	git_config_parser reader;
	git_buf contents = GIT_BUF_INIT;
	int error;

	if (depth >= MAX_INCLUDE_DEPTH) {
		giterr_set(GITERR_CONFIG, "maximum config include depth reached");
		return -1;
	}

	if ((error = git_futils_readbuffer(&contents, file->path)) < 0)
		goto out;

	git_parse_ctx_init(&reader.ctx, contents.ptr, contents.size);

	if ((error = git_hash_buf(&file->checksum, contents.ptr, contents.size)) < 0)
		goto out;

	/* Initialize the reading position */
	reader.file = file;
	git_parse_ctx_init(&reader.ctx, contents.ptr, contents.size);

	/* If the file is empty, there's nothing for us to do */
	if (!reader.ctx.content || *reader.ctx.content == '\0')
		goto out;

	parse_data.repo = repo;
	parse_data.file_path = file->path;
	parse_data.entries = entries;
	parse_data.level = level;
	parse_data.depth = depth;

	error = git_config_parse(&reader, NULL, read_on_variable, NULL, NULL, &parse_data);

out:
	git_buf_dispose(&contents);
	return error;
}

static int write_section(git_buf *fbuf, const char *key)
{
	int result;
	const char *dot;
	git_buf buf = GIT_BUF_INIT;

	/* All of this just for [section "subsection"] */
	dot = strchr(key, '.');
	git_buf_putc(&buf, '[');
	if (dot == NULL) {
		git_buf_puts(&buf, key);
	} else {
		char *escaped;
		git_buf_put(&buf, key, dot - key);
		escaped = escape_value(dot + 1);
		GITERR_CHECK_ALLOC(escaped);
		git_buf_printf(&buf, " \"%s\"", escaped);
		git__free(escaped);
	}
	git_buf_puts(&buf, "]\n");

	if (git_buf_oom(&buf))
		return -1;

	result = git_buf_put(fbuf, git_buf_cstr(&buf), buf.size);
	git_buf_dispose(&buf);

	return result;
}

static const char *quotes_for_value(const char *value)
{
	const char *ptr;

	if (value[0] == ' ' || value[0] == '\0')
		return "\"";

	for (ptr = value; *ptr; ++ptr) {
		if (*ptr == ';' || *ptr == '#')
			return "\"";
	}

	if (ptr[-1] == ' ')
		return "\"";

	return "";
}

struct write_data {
	git_buf *buf;
	git_buf buffered_comment;
	unsigned int in_section : 1,
		preg_replaced : 1;
	const char *orig_section;
	const char *section;
	const char *orig_name;
	const char *name;
	const regex_t *preg;
	const char *value;
};

static int write_line_to(git_buf *buf, const char *line, size_t line_len)
{
	int result = git_buf_put(buf, line, line_len);

	if (!result && line_len && line[line_len-1] != '\n')
		result = git_buf_printf(buf, "\n");

	return result;
}

static int write_line(struct write_data *write_data, const char *line, size_t line_len)
{
	return write_line_to(write_data->buf, line, line_len);
}

static int write_value(struct write_data *write_data)
{
	const char *q;
	int result;

	q = quotes_for_value(write_data->value);
	result = git_buf_printf(write_data->buf,
		"\t%s = %s%s%s\n", write_data->orig_name, q, write_data->value, q);

	/* If we are updating a single name/value, we're done.  Setting `value`
	 * to `NULL` will prevent us from trying to write it again later (in
	 * `write_on_section`) if we see the same section repeated.
	 */
	if (!write_data->preg)
		write_data->value = NULL;

	return result;
}

static int write_on_section(
	git_config_parser *reader,
	const char *current_section,
	const char *line,
	size_t line_len,
	void *data)
{
	struct write_data *write_data = (struct write_data *)data;
	int result = 0;

	GIT_UNUSED(reader);

	/* If we were previously in the correct section (but aren't anymore)
	 * and haven't written our value (for a simple name/value set, not
	 * a multivar), then append it to the end of the section before writing
	 * the new one.
	 */
	if (write_data->in_section && !write_data->preg && write_data->value)
		result = write_value(write_data);

	write_data->in_section = strcmp(current_section, write_data->section) == 0;

	/*
	 * If there were comments just before this section, dump them as well.
	 */
	if (!result) {
		result = git_buf_put(write_data->buf, write_data->buffered_comment.ptr, write_data->buffered_comment.size);
		git_buf_clear(&write_data->buffered_comment);
	}

	if (!result)
		result = write_line(write_data, line, line_len);

	return result;
}

static int write_on_variable(
	git_config_parser *reader,
	const char *current_section,
	char *var_name,
	char *var_value,
	const char *line,
	size_t line_len,
	void *data)
{
	struct write_data *write_data = (struct write_data *)data;
	bool has_matched = false;
	int error;

	GIT_UNUSED(reader);
	GIT_UNUSED(current_section);

	/*
	 * If there were comments just before this variable, let's dump them as well.
	 */
	if ((error = git_buf_put(write_data->buf, write_data->buffered_comment.ptr, write_data->buffered_comment.size)) < 0)
		return error;

	git_buf_clear(&write_data->buffered_comment);

	/* See if we are to update this name/value pair; first examine name */
	if (write_data->in_section &&
		strcasecmp(write_data->name, var_name) == 0)
		has_matched = true;

	/* If we have a regex to match the value, see if it matches */
	if (has_matched && write_data->preg != NULL)
		has_matched = (regexec(write_data->preg, var_value, 0, NULL, 0) == 0);

	git__free(var_name);
	git__free(var_value);

	/* If this isn't the name/value we're looking for, simply dump the
	 * existing data back out and continue on.
	 */
	if (!has_matched)
		return write_line(write_data, line, line_len);

	write_data->preg_replaced = 1;

	/* If value is NULL, we are deleting this value; write nothing. */
	if (!write_data->value)
		return 0;

	return write_value(write_data);
}

static int write_on_comment(git_config_parser *reader, const char *line, size_t line_len, void *data)
{
	struct write_data *write_data;

	GIT_UNUSED(reader);

	write_data = (struct write_data *)data;
	return write_line_to(&write_data->buffered_comment, line, line_len);
}

static int write_on_eof(
	git_config_parser *reader, const char *current_section, void *data)
{
	struct write_data *write_data = (struct write_data *)data;
	int result = 0;

	GIT_UNUSED(reader);

	/*
	 * If we've buffered comments when reaching EOF, make sure to dump them.
	 */
	if ((result = git_buf_put(write_data->buf, write_data->buffered_comment.ptr, write_data->buffered_comment.size)) < 0)
		return result;

	/* If we are at the EOF and have not written our value (again, for a
	 * simple name/value set, not a multivar) then we have never seen the
	 * section in question and should create a new section and write the
	 * value.
	 */
	if ((!write_data->preg || !write_data->preg_replaced) && write_data->value) {
		/* write the section header unless we're already in it */
		if (!current_section || strcmp(current_section, write_data->section))
			result = write_section(write_data->buf, write_data->orig_section);

		if (!result)
			result = write_value(write_data);
	}

	return result;
}

/*
 * This is pretty much the parsing, except we write out anything we don't have
 */
static int config_write(diskfile_backend *cfg, const char *orig_key, const char *key, const regex_t *preg, const char* value)
{
	int result;
	char *orig_section, *section, *orig_name, *name, *ldot;
	git_filebuf file = GIT_FILEBUF_INIT;
	git_buf buf = GIT_BUF_INIT, contents = GIT_BUF_INIT;
	git_config_parser reader;
	struct write_data write_data;

	memset(&reader, 0, sizeof(reader));
	reader.file = &cfg->file;

	if (cfg->locked) {
		result = git_buf_puts(&contents, git_buf_cstr(&cfg->locked_content));
	} else {
		/* Lock the file */
		if ((result = git_filebuf_open(
			     &file, cfg->file.path, GIT_FILEBUF_HASH_CONTENTS, GIT_CONFIG_FILE_MODE)) < 0) {
			git_buf_dispose(&contents);
			return result;
		}

		/* We need to read in our own config file */
		result = git_futils_readbuffer(&contents, cfg->file.path);
	}

	/* Initialise the reading position */
	if (result == 0 || result == GIT_ENOTFOUND) {
		git_parse_ctx_init(&reader.ctx, contents.ptr, contents.size);
	} else {
		git_filebuf_cleanup(&file);
		return -1; /* OS error when reading the file */
	}

	ldot = strrchr(key, '.');
	name = ldot + 1;
	section = git__strndup(key, ldot - key);
	GITERR_CHECK_ALLOC(section);

	ldot = strrchr(orig_key, '.');
	orig_name = ldot + 1;
	orig_section = git__strndup(orig_key, ldot - orig_key);
	GITERR_CHECK_ALLOC(orig_section);

	write_data.buf = &buf;
	git_buf_init(&write_data.buffered_comment, 0);
	write_data.orig_section = orig_section;
	write_data.section = section;
	write_data.in_section = 0;
	write_data.preg_replaced = 0;
	write_data.orig_name = orig_name;
	write_data.name = name;
	write_data.preg = preg;
	write_data.value = value;

	result = git_config_parse(&reader,
		write_on_section,
		write_on_variable,
		write_on_comment,
		write_on_eof,
		&write_data);
	git__free(section);
	git__free(orig_section);
	git_buf_dispose(&write_data.buffered_comment);

	if (result < 0) {
		git_filebuf_cleanup(&file);
		goto done;
	}

	if (cfg->locked) {
		size_t len = buf.asize;
		/* Update our copy with the modified contents */
		git_buf_dispose(&cfg->locked_content);
		git_buf_attach(&cfg->locked_content, git_buf_detach(&buf), len);
	} else {
		git_filebuf_write(&file, git_buf_cstr(&buf), git_buf_len(&buf));
		result = git_filebuf_commit(&file);
	}

done:
	git_buf_dispose(&buf);
	git_buf_dispose(&contents);
	git_parse_ctx_clear(&reader.ctx);
	return result;
}
