/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "config.h"
#include "git2/config.h"
#include "vector.h"
#if GIT_WIN32
# include <windows.h>
#endif

#include <ctype.h>

typedef struct {
	git_refcount rc;

	git_config_file *file;
	unsigned int level;
} file_internal;

static void file_internal_free(file_internal *internal)
{
	git_config_file *file;

	file = internal->file;
	file->free(file);
	git__free(internal);
}

static void config_free(git_config *cfg)
{
	unsigned int i;
	file_internal *internal;

	for(i = 0; i < cfg->files.length; ++i){
		internal = git_vector_get(&cfg->files, i);
		GIT_REFCOUNT_DEC(internal, file_internal_free);
	}

	git_vector_free(&cfg->files);
	git__free(cfg);
}

void git_config_free(git_config *cfg)
{
	if (cfg == NULL)
		return;

	GIT_REFCOUNT_DEC(cfg, config_free);
}

static int config_backend_cmp(const void *a, const void *b)
{
	const file_internal *bk_a = (const file_internal *)(a);
	const file_internal *bk_b = (const file_internal *)(b);

	return bk_b->level - bk_a->level;
}

int git_config_new(git_config **out)
{
	git_config *cfg;

	cfg = git__malloc(sizeof(git_config));
	GITERR_CHECK_ALLOC(cfg);

	memset(cfg, 0x0, sizeof(git_config));

	if (git_vector_init(&cfg->files, 3, config_backend_cmp) < 0) {
		git__free(cfg);
		return -1;
	}

	*out = cfg;
	GIT_REFCOUNT_INC(cfg);
	return 0;
}

int git_config_add_file_ondisk(
	git_config *cfg,
	const char *path,
	unsigned int level,
	int force)
{
	git_config_file *file = NULL;
	int res;

	if (git_config_file__ondisk(&file, path) < 0)
		return -1;

	if ((res = git_config_add_file(cfg, file, level, force)) < 0) {
		/*
		 * free manually; the file is not owned by the config
		 * instance yet and will not be freed on cleanup
		 */
		file->free(file);
		return res;
	}

	return 0;
}

int git_config_open_ondisk(git_config **cfg, const char *path)
{
	if (git_config_new(cfg) < 0)
		return -1;

	if (git_config_add_file_ondisk(*cfg, path, GIT_CONFIG_LEVEL_LOCAL, 0) < 0) {
		git_config_free(*cfg);
		return -1;
	}

	return 0;
}

static int find_internal_file_by_level(
	file_internal **internal_out,
	git_config *cfg,
	int level)
{
	int pos = -1;
	file_internal *internal;
	unsigned int i;

	assert(cfg->files.length);

	/* when passing GIT_CONFIG_HIGHEST_LEVEL, the idea is to get the config file
	 * which has the highest level. As config files are stored in a vector
	 * sorted by decreasing order of level, getting the file at position 0
	 * will do the job.
	 */
	if (level == GIT_CONFIG_HIGHEST_LEVEL) {
		pos = 0;
	} else {
		git_vector_foreach(&cfg->files, i, internal) {
			if (internal->level == (unsigned int)level)
				pos = i;
		}
	}

	if (pos == -1) {
		giterr_set(GITERR_CONFIG,
			"No config file exists for the given level '%i'", level);
		return GIT_ENOTFOUND;
	}

	*internal_out = git_vector_get(&cfg->files, pos);

	return 0;
}

static int duplicate_level(void **old_raw, void *new_raw)
{
	file_internal **old = (file_internal **)old_raw;

	GIT_UNUSED(new_raw);

	giterr_set(GITERR_CONFIG, "A file with the same level (%i) has already been added to the config", (*old)->level);
	return GIT_EEXISTS;
}

static void try_remove_existing_file_internal(
	git_config *cfg,
	unsigned int level)
{
	int pos = -1;
	file_internal *internal;
	unsigned int i;

	git_vector_foreach(&cfg->files, i, internal) {
		if (internal->level == level)
			pos = i;
	}

	if (pos == -1)
		return;

	internal = git_vector_get(&cfg->files, pos);

	if (git_vector_remove(&cfg->files, pos) < 0)
		return;

	GIT_REFCOUNT_DEC(internal, file_internal_free);
}

static int git_config__add_internal(
	git_config *cfg,
	file_internal *internal,
	unsigned int level,
	int force)
{
	int result;

	/* delete existing config file for level if it exists */
	if (force)
		try_remove_existing_file_internal(cfg, level);

	if ((result = git_vector_insert_sorted(&cfg->files,
			internal, &duplicate_level)) < 0)
		return result;

	git_vector_sort(&cfg->files);
	internal->file->cfg = cfg;

	GIT_REFCOUNT_INC(internal);

	return 0;
}

int git_config_open_level(
    git_config **cfg_out,
    git_config *cfg_parent,
    unsigned int level)
{
	git_config *cfg;
	file_internal *internal;
	int res;

	if ((res = find_internal_file_by_level(&internal, cfg_parent, level)) < 0)
		return res;

	if ((res = git_config_new(&cfg)) < 0)
		return res;

	if ((res = git_config__add_internal(cfg, internal, level, true)) < 0) {
		git_config_free(cfg);
		return res;
	}

	*cfg_out = cfg;

	return 0;
}

int git_config_add_file(
	git_config *cfg,
	git_config_file *file,
	unsigned int level,
	int force)
{
	file_internal *internal;
	int result;

	assert(cfg && file);

	if ((result = file->open(file, level)) < 0)
		return result;

	internal = git__malloc(sizeof(file_internal));
	GITERR_CHECK_ALLOC(internal);

	memset(internal, 0x0, sizeof(file_internal));

	internal->file = file;
	internal->level = level;

	if ((result = git_config__add_internal(cfg, internal, level, force)) < 0) {
		git__free(internal);
		return result;
	}

	return 0;
}

/*
 * Loop over all the variables
 */

int git_config_foreach(
	git_config *cfg, int (*fn)(const git_config_entry *, void *), void *data)
{
	return git_config_foreach_match(cfg, NULL, fn, data);
}

int git_config_foreach_match(
	git_config *cfg,
	const char *regexp,
	int (*fn)(const git_config_entry *, void *),
	void *data)
{
	int ret = 0;
	unsigned int i;
	file_internal *internal;
	git_config_file *file;

	for (i = 0; i < cfg->files.length && ret == 0; ++i) {
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		ret = file->foreach(file, regexp, fn, data);
	}

	return ret;
}

int git_config_delete(git_config *cfg, const char *name)
{
	git_config_file *file;
	file_internal *internal;

	internal = git_vector_get(&cfg->files, 0);
	file = internal->file;

	return file->del(file, name);
}

/**************
 * Setters
 **************/

int git_config_set_int64(git_config *cfg, const char *name, int64_t value)
{
	char str_value[32]; /* All numbers should fit in here */
	p_snprintf(str_value, sizeof(str_value), "%" PRId64, value);
	return git_config_set_string(cfg, name, str_value);
}

int git_config_set_int32(git_config *cfg, const char *name, int32_t value)
{
	return git_config_set_int64(cfg, name, (int64_t)value);
}

int git_config_set_bool(git_config *cfg, const char *name, int value)
{
	return git_config_set_string(cfg, name, value ? "true" : "false");
}

int git_config_set_string(git_config *cfg, const char *name, const char *value)
{
	git_config_file *file;
	file_internal *internal;

	internal = git_vector_get(&cfg->files, 0);
	file = internal->file;

	return file->set(file, name, value);
}

/***********
 * Getters
 ***********/
int git_config_get_mapped(
	int *out,
	git_config *cfg,
	const char *name,
	git_cvar_map *maps,
	size_t map_n)
{
	const char *value;
	int ret;

	if ((ret = git_config_get_string(&value, cfg, name)) < 0)
		return ret;

	return git_config_lookup_map_value(out, maps, map_n, value);
}

int git_config_get_int64(int64_t *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	if ((ret = git_config_get_string(&value, cfg, name)) < 0)
		return ret;

	return git_config_parse_int64(out, value);
}

int git_config_get_int32(int32_t *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	if ((ret = git_config_get_string(&value, cfg, name)) < 0)
		return ret;

	return git_config_parse_int32(out, value);
}

int git_config_get_bool(int *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	if ((ret = git_config_get_string(&value, cfg, name)) < 0)
		return ret;

	return git_config_parse_bool(out, value);
}

static int get_string_at_file(const char **out, git_config_file *file, const char *name)
{
	const git_config_entry *entry;
	int res;

	*out = NULL;

	res = file->get(file, name, &entry);
	if (res != GIT_ENOTFOUND)
		*out = entry->value;

	return res;
}

int git_config_get_string(const char **out, git_config *cfg, const char *name)
{
	file_internal *internal;
	unsigned int i;

	assert(cfg->files.length);

	git_vector_foreach(&cfg->files, i, internal) {
		int res = get_string_at_file(out, internal->file, name);

		if (res != GIT_ENOTFOUND)
			return res;
	}

	return GIT_ENOTFOUND;
}

int git_config_get_config_entry(const git_config_entry **out, git_config *cfg, const char *name)
{
	file_internal *internal;
	unsigned int i;

	assert(cfg->files.length);

	*out = NULL;

	git_vector_foreach(&cfg->files, i, internal) {
		git_config_file *file = internal->file;
		int ret = file->get(file, name, out);
		if (ret != GIT_ENOTFOUND)
			return ret;
	}

	return GIT_ENOTFOUND;
}

int git_config_get_multivar(git_config *cfg, const char *name, const char *regexp,
			    int (*fn)(const git_config_entry *entry, void *data), void *data)
{
	file_internal *internal;
	git_config_file *file;
	int ret = GIT_ENOTFOUND;
	size_t i;

	assert(cfg->files.length);

	/*
	 * This loop runs the "wrong" way 'round because we need to
	 * look at every value from the most general to most specific
	 */
	for (i = cfg->files.length; i > 0; --i) {
		internal = git_vector_get(&cfg->files, i - 1);
		file = internal->file;
		ret = file->get_multivar(file, name, regexp, fn, data);
		if (ret < 0 && ret != GIT_ENOTFOUND)
			return ret;
	}

	return 0;
}

int git_config_set_multivar(git_config *cfg, const char *name, const char *regexp, const char *value)
{
	git_config_file *file;
	file_internal *internal;

	internal = git_vector_get(&cfg->files, 0);
	file = internal->file;

	return file->set_multivar(file, name, regexp, value);
}

int git_config_find_global_r(git_buf *path)
{
	int error = git_futils_find_global_file(path, GIT_CONFIG_FILENAME);

	return error;
}

int git_config_find_xdg_r(git_buf *path)
{
	int error = git_futils_find_global_file(path, GIT_CONFIG_FILENAME_ALT);

	return error;
}

int git_config_find_global(char *global_config_path, size_t length)
{
	git_buf path  = GIT_BUF_INIT;
	int     ret = git_config_find_global_r(&path);

	if (ret < 0) {
		git_buf_free(&path);
		return ret;
	}

	if (path.size >= length) {
		git_buf_free(&path);
		giterr_set(GITERR_NOMEMORY,
			"Path is to long to fit on the given buffer");
		return -1;
	}

	git_buf_copy_cstr(global_config_path, length, &path);
	git_buf_free(&path);
	return 0;
}

int git_config_find_xdg(char *xdg_config_path, size_t length)
{
	git_buf path  = GIT_BUF_INIT;
	int ret = git_config_find_xdg_r(&path);

	if (ret < 0) {
		git_buf_free(&path);
		return ret;
	}

	if (path.size >= length) {
		git_buf_free(&path);
		giterr_set(GITERR_NOMEMORY,
			"Path is to long to fit on the given buffer");
		return -1;
	}

	git_buf_copy_cstr(xdg_config_path, length, &path);
	git_buf_free(&path);
	return 0;
}

int git_config_find_system_r(git_buf *path)
{
	return git_futils_find_system_file(path, GIT_CONFIG_FILENAME_SYSTEM);
}

int git_config_find_system(char *system_config_path, size_t length)
{
	git_buf path  = GIT_BUF_INIT;
	int     ret = git_config_find_system_r(&path);

	if (ret < 0) {
		git_buf_free(&path);
		return ret;
	}

	if (path.size >= length) {
		git_buf_free(&path);
		giterr_set(GITERR_NOMEMORY,
			"Path is to long to fit on the given buffer");
		return -1;
	}

	git_buf_copy_cstr(system_config_path, length, &path);
	git_buf_free(&path);
	return 0;
}

int git_config_open_default(git_config **out)
{
	int error;
	git_config *cfg = NULL;
	git_buf buf = GIT_BUF_INIT;

	error = git_config_new(&cfg);

	if (!error && !git_config_find_global_r(&buf))
		error = git_config_add_file_ondisk(cfg, buf.ptr,
			GIT_CONFIG_LEVEL_GLOBAL, 0);

	if (!error && !git_config_find_xdg_r(&buf))
		error = git_config_add_file_ondisk(cfg, buf.ptr,
			GIT_CONFIG_LEVEL_XDG, 0);

	if (!error && !git_config_find_system_r(&buf))
		error = git_config_add_file_ondisk(cfg, buf.ptr,
			GIT_CONFIG_LEVEL_SYSTEM, 0);

	git_buf_free(&buf);

	if (error && cfg) {
		git_config_free(cfg);
		cfg = NULL;
	}

	*out = cfg;

	return error;
}

/***********
 * Parsers
 ***********/
int git_config_lookup_map_value(
	int *out,
	git_cvar_map *maps,
	size_t map_n,
	const char *value)
{
	size_t i;

	if (!value)
		goto fail_parse;

	for (i = 0; i < map_n; ++i) {
		git_cvar_map *m = maps + i;

		switch (m->cvar_type) {
		case GIT_CVAR_FALSE:
		case GIT_CVAR_TRUE: {
			int bool_val;

			if (git__parse_bool(&bool_val, value) == 0 &&
				bool_val == (int)m->cvar_type) {
				*out = m->map_value;
				return 0;
			}
			break;
		}

		case GIT_CVAR_INT32:
			if (git_config_parse_int32(out, value) == 0)
				return 0;
			break;

		case GIT_CVAR_STRING:
			if (strcasecmp(value, m->str_match) == 0) {
				*out = m->map_value;
				return 0;
			}
			break;
		}
	}

fail_parse:
	giterr_set(GITERR_CONFIG, "Failed to map '%s'", value);
	return -1;
}

int git_config_parse_bool(int *out, const char *value)
{
	if (git__parse_bool(out, value) == 0)
		return 0;

	if (git_config_parse_int32(out, value) == 0) {
		*out = !!(*out);
		return 0;
	}

	giterr_set(GITERR_CONFIG, "Failed to parse '%s' as a boolean value", value);
	return -1;
}

int git_config_parse_int64(int64_t *out, const char *value)
{
	const char *num_end;
	int64_t num;

	if (git__strtol64(&num, value, &num_end, 0) < 0)
		goto fail_parse;

	switch (*num_end) {
	case 'g':
	case 'G':
		num *= 1024;
		/* fallthrough */

	case 'm':
	case 'M':
		num *= 1024;
		/* fallthrough */

	case 'k':
	case 'K':
		num *= 1024;

		/* check that that there are no more characters after the
		 * given modifier suffix */
		if (num_end[1] != '\0')
			return -1;

		/* fallthrough */

	case '\0':
		*out = num;
		return 0;

	default:
		goto fail_parse;
	}

fail_parse:
	giterr_set(GITERR_CONFIG, "Failed to parse '%s' as an integer", value);
	return -1;
}

int git_config_parse_int32(int32_t *out, const char *value)
{
	int64_t tmp;
	int32_t truncate;

	if (git_config_parse_int64(&tmp, value) < 0)
		goto fail_parse;

	truncate = tmp & 0xFFFFFFFF;
	if (truncate != tmp)
		goto fail_parse;

	*out = truncate;
	return 0;

fail_parse:
	giterr_set(GITERR_CONFIG, "Failed to parse '%s' as a 32-bit integer", value);
	return -1;
}
