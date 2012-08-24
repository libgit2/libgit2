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
	git_config_file *file;
	int priority;
} file_internal;

static void config_free(git_config *cfg)
{
	unsigned int i;
	git_config_file *file;
	file_internal *internal;

	for(i = 0; i < cfg->files.length; ++i){
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		file->free(file);
		git__free(internal);
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

	return bk_b->priority - bk_a->priority;
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

int git_config_add_file_ondisk(git_config *cfg, const char *path, int priority)
{
	git_config_file *file = NULL;

	if (git_config_file__ondisk(&file, path) < 0)
		return -1;

	if (git_config_add_file(cfg, file, priority) < 0) {
		/*
		 * free manually; the file is not owned by the config
		 * instance yet and will not be freed on cleanup
		 */
		file->free(file);
		return -1;
	}

	return 0;
}

int git_config_open_ondisk(git_config **cfg, const char *path)
{
	if (git_config_new(cfg) < 0)
		return -1;

	if (git_config_add_file_ondisk(*cfg, path, 1) < 0) {
		git_config_free(*cfg);
		return -1;
	}

	return 0;
}

int git_config_add_file(git_config *cfg, git_config_file *file, int priority)
{
	file_internal *internal;
	int result;

	assert(cfg && file);

	if ((result = file->open(file)) < 0)
		return result;

	internal = git__malloc(sizeof(file_internal));
	GITERR_CHECK_ALLOC(internal);

	internal->file = file;
	internal->priority = priority;

	if (git_vector_insert(&cfg->files, internal) < 0) {
		git__free(internal);
		return -1;
	}

	git_vector_sort(&cfg->files);
	internal->file->cfg = cfg;

	return 0;
}

/*
 * Loop over all the variables
 */

int git_config_foreach(git_config *cfg, int (*fn)(const char *, const char *, void *), void *data)
{
	int ret = 0;
	unsigned int i;
	file_internal *internal;
	git_config_file *file;

	for(i = 0; i < cfg->files.length && ret == 0; ++i) {
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		ret = file->foreach(file, fn, data);
	}

	return ret;
}

int git_config_delete(git_config *cfg, const char *name)
{
	file_internal *internal;
	git_config_file *file;

	assert(cfg->files.length);

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
	file_internal *internal;
	git_config_file *file;

	assert(cfg->files.length);

	internal = git_vector_get(&cfg->files, 0);
	file = internal->file;

	return file->set(file, name, value);
}

static int parse_int64(int64_t *out, const char *value)
{
	const char *num_end;
	int64_t num;

	if (git__strtol64(&num, value, &num_end, 0) < 0)
		return -1;

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
		return -1;
	}
}

static int parse_int32(int32_t *out, const char *value)
{
	int64_t tmp;
	int32_t truncate;

	if (parse_int64(&tmp, value) < 0)
		return -1;

	truncate = tmp & 0xFFFFFFFF;
	if (truncate != tmp)
		return -1;

	*out = truncate;
	return 0;
}

/***********
 * Getters
 ***********/
int git_config_lookup_map_value(
	git_cvar_map *maps, size_t map_n, const char *value, int *out)
{
	size_t i;

	if (!value)
		return GIT_ENOTFOUND;

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
			if (parse_int32(out, value) == 0)
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

	return GIT_ENOTFOUND;
}

int git_config_get_mapped(
	int *out,
	git_config *cfg,
	const char *name,
	git_cvar_map *maps,
	size_t map_n)
{
	const char *value;
	int ret;

	ret = git_config_get_string(&value, cfg, name);
	if (ret < 0)
		return ret;

	if (!git_config_lookup_map_value(maps, map_n, value, out))
		return 0;

	giterr_set(GITERR_CONFIG,
		"Failed to map the '%s' config variable with a valid value", name);
	return -1;
}

int git_config_get_int64(int64_t *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	ret = git_config_get_string(&value, cfg, name);
	if (ret < 0)
		return ret;

	if (parse_int64(out, value) < 0) {
		giterr_set(GITERR_CONFIG, "Failed to parse '%s' as an integer", value);
		return -1;
	}

	return 0;
}

int git_config_get_int32(int32_t *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	ret = git_config_get_string(&value, cfg, name);
	if (ret < 0)
		return ret;

	if (parse_int32(out, value) < 0) {
		giterr_set(GITERR_CONFIG, "Failed to parse '%s' as a 32-bit integer", value);
		return -1;
	}

	return 0;
}

int git_config_get_bool(int *out, git_config *cfg, const char *name)
{
	const char *value;
	int ret;

	ret = git_config_get_string(&value, cfg, name);
	if (ret < 0)
		return ret;

	if (git__parse_bool(out, value) == 0)
		return 0;

	if (parse_int32(out, value) == 0) {
		*out = !!(*out);
		return 0;
	}

	giterr_set(GITERR_CONFIG, "Failed to parse '%s' as a boolean value", value);
	return -1;
}

int git_config_get_string(const char **out, git_config *cfg, const char *name)
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

	giterr_set(GITERR_CONFIG, "Config variable '%s' not found", name);
	return GIT_ENOTFOUND;
}

int git_config_get_multivar(git_config *cfg, const char *name, const char *regexp,
			    int (*fn)(const char *value, void *data), void *data)
{
	file_internal *internal;
	git_config_file *file;
	int ret = GIT_ENOTFOUND;
	unsigned int i;

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
	file_internal *internal;
	git_config_file *file;
	int ret = GIT_ENOTFOUND;
	unsigned int i;

	for (i = cfg->files.length; i > 0; --i) {
		internal = git_vector_get(&cfg->files, i - 1);
		file = internal->file;
		ret = file->set_multivar(file, name, regexp, value);
		if (ret < 0 && ret != GIT_ENOTFOUND)
			return ret;
	}

	return 0;
}

int git_config_find_global_r(git_buf *path)
{
	return git_futils_find_global_file(path, GIT_CONFIG_FILENAME);
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

int git_config_open_global(git_config **out)
{
	int error;
	git_buf path = GIT_BUF_INIT;

	if ((error = git_config_find_global_r(&path)) < 0)
		return error;

	error = git_config_open_ondisk(out, git_buf_cstr(&path));
	git_buf_free(&path);

	return error;
}

