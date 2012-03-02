/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "hashtable.h"
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
	if (cfg == NULL)
		return GIT_ENOMEM;

	memset(cfg, 0x0, sizeof(git_config));

	if (git_vector_init(&cfg->files, 3, config_backend_cmp) < 0) {
		git__free(cfg);
		return GIT_ENOMEM;
	}

	*out = cfg;
	GIT_REFCOUNT_INC(cfg);
	return GIT_SUCCESS;
}

int git_config_add_file_ondisk(git_config *cfg, const char *path, int priority)
{
	git_config_file *file = NULL;
	int error;

	error = git_config_file__ondisk(&file, path);
	if (error < GIT_SUCCESS)
		return error;

	error = git_config_add_file(cfg, file, priority);
	if (error < GIT_SUCCESS) {
		/*
		 * free manually; the file is not owned by the config
		 * instance yet and will not be freed on cleanup
		 */
		file->free(file);
		return error;
	}

	return GIT_SUCCESS;
}

int git_config_open_ondisk(git_config **cfg, const char *path)
{
	int error;

	error = git_config_new(cfg);
	if (error < GIT_SUCCESS)
		return error;

	error = git_config_add_file_ondisk(*cfg, path, 1);
	if (error < GIT_SUCCESS)
		git_config_free(*cfg);

	return error;
}

int git_config_add_file(git_config *cfg, git_config_file *file, int priority)
{
	file_internal *internal;
	int error;

	assert(cfg && file);

	if ((error = file->open(file)) < GIT_SUCCESS)
		return git__throw(error, "Failed to open config file");

	internal = git__malloc(sizeof(file_internal));
	if (internal == NULL)
		return GIT_ENOMEM;

	internal->file = file;
	internal->priority = priority;

	if (git_vector_insert(&cfg->files, internal) < 0) {
		git__free(internal);
		return GIT_ENOMEM;
	}

	git_vector_sort(&cfg->files);
	internal->file->cfg = cfg;

	return GIT_SUCCESS;
}

/*
 * Loop over all the variables
 */

int git_config_foreach(git_config *cfg, int (*fn)(const char *, const char *, void *), void *data)
{
	int ret = GIT_SUCCESS;
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

	if (cfg->files.length == 0)
		return git__throw(GIT_EINVALIDARGS, "Cannot delete variable; no files open in the `git_config` instance");

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

	if (cfg->files.length == 0)
		return git__throw(GIT_EINVALIDARGS, "Cannot set variable value; no files open in the `git_config` instance");

	internal = git_vector_get(&cfg->files, 0);
	file = internal->file;

	return file->set(file, name, value);
}

static int parse_bool(int *out, const char *value)
{
	/* A missing value means true */
	if (value == NULL) {
		*out = 1;
		return GIT_SUCCESS;
	}

	if (!strcasecmp(value, "true") ||
		!strcasecmp(value, "yes") ||
		!strcasecmp(value, "on")) {
		*out = 1;
		return GIT_SUCCESS;
	}
	if (!strcasecmp(value, "false") ||
		!strcasecmp(value, "no") ||
		!strcasecmp(value, "off")) {
		*out = 0;
		return GIT_SUCCESS;
	}

	return GIT_EINVALIDTYPE;
}

static int parse_int64(int64_t *out, const char *value)
{
	const char *num_end;
	int64_t num;

	if (git__strtol64(&num, value, &num_end, 0) < 0)
		return GIT_EINVALIDTYPE;

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
			return GIT_EINVALIDTYPE;

		/* fallthrough */

	case '\0':
		*out = num;
		return 0;

	default:
		return GIT_EINVALIDTYPE;
	}
}

static int parse_int32(int32_t *out, const char *value)
{
	int64_t tmp;
	int32_t truncate;

	if (parse_int64(&tmp, value) < 0)
		return GIT_EINVALIDTYPE;

	truncate = tmp & 0xFFFFFFFF;
	if (truncate != tmp)
		return GIT_EOVERFLOW;

	*out = truncate;
	return 0;
}

/***********
 * Getters
 ***********/
int git_config_get_mapped(git_config *cfg, const char *name, git_cvar_map *maps, size_t map_n, int *out)
{
	size_t i;
	const char *value;
	int error;

	error = git_config_get_string(cfg, name, &value);
	if (error < GIT_SUCCESS)
		return error;

	for (i = 0; i < map_n; ++i) {
		git_cvar_map *m = maps + i;

		switch (m->cvar_type) {
			case GIT_CVAR_FALSE:
			case GIT_CVAR_TRUE: {
				int bool_val;

				if (parse_bool(&bool_val, value) == 0 && 
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
		}
	}

	return git__throw(GIT_ENOTFOUND,
		"Failed to map the '%s' config variable with a valid value", name);
}

int git_config_get_int64(git_config *cfg, const char *name, int64_t *out)
{
	const char *value;
	int ret;

	ret = git_config_get_string(cfg, name, &value);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to retrieve value for '%s'", name);

	if (parse_int64(out, value) < 0)
		return git__throw(GIT_EINVALIDTYPE, "Failed to parse '%s' as an integer", value);

	return GIT_SUCCESS;
}

int git_config_get_int32(git_config *cfg, const char *name, int32_t *out)
{
	const char *value;
	int error;

	error = git_config_get_string(cfg, name, &value);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to get value for %s", name);

	error = parse_int32(out, value);
	if (error < GIT_SUCCESS)
		return git__throw(GIT_EINVALIDTYPE, "Failed to parse '%s' as a 32-bit integer", value);
	
	return GIT_SUCCESS;
}

int git_config_get_bool(git_config *cfg, const char *name, int *out)
{
	const char *value;
	int error = GIT_SUCCESS;

	error = git_config_get_string(cfg, name, &value);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to get value for %s", name);

	if (parse_bool(out, value) == 0)
		return GIT_SUCCESS;

	if (parse_int32(out, value) == 0) {
		*out = !!(*out);
		return GIT_SUCCESS;
	}

	return git__throw(GIT_EINVALIDTYPE, "Failed to parse '%s' as a boolean value", value);
}

int git_config_get_string(git_config *cfg, const char *name, const char **out)
{
	file_internal *internal;
	git_config_file *file;
	int error = GIT_ENOTFOUND;
	unsigned int i;

	if (cfg->files.length == 0)
		return git__throw(GIT_EINVALIDARGS, "Cannot get variable value; no files open in the `git_config` instance");

	for (i = 0; i < cfg->files.length; ++i) {
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		if ((error = file->get(file, name, out)) == GIT_SUCCESS)
			return GIT_SUCCESS;
	}

	return git__throw(error, "Config value '%s' not found", name);
}

int git_config_get_multivar(git_config *cfg, const char *name, const char *regexp,
			    int (*fn)(const char *value, void *data), void *data)
{
	file_internal *internal;
	git_config_file *file;
	int error = GIT_ENOTFOUND;
	unsigned int i;


	if (cfg->files.length == 0)
		return git__throw(GIT_EINVALIDARGS, "Cannot get variable value; no files open in the `git_config` instance");

	/*
	 * This loop runs the "wrong" way 'round because we need to
	 * look at every value from the most general to most specific
	 */
	for (i = cfg->files.length; i > 0; --i) {
		internal = git_vector_get(&cfg->files, i - 1);
		file = internal->file;
		error = file->get_multivar(file, name, regexp, fn, data);
		if (error < GIT_SUCCESS && error != GIT_ENOTFOUND)
			git__rethrow(error, "Failed to get multivar");
	}

	return GIT_SUCCESS;
}

int git_config_set_multivar(git_config *cfg, const char *name, const char *regexp, const char *value)
{
	file_internal *internal;
	git_config_file *file;
	int error = GIT_ENOTFOUND;
	unsigned int i;

	for (i = cfg->files.length; i > 0; --i) {
		internal = git_vector_get(&cfg->files, i - 1);
		file = internal->file;
		error = file->set_multivar(file, name, regexp, value);
		if (error < GIT_SUCCESS && error != GIT_ENOTFOUND)
			git__rethrow(error, "Failed to replace multivar");
	}

	return GIT_SUCCESS;
}

int git_config_find_global_r(git_buf *path)
{
	return git_futils_find_global_file(path, GIT_CONFIG_FILENAME);
}

int git_config_find_global(char *global_config_path)
{
	git_buf path  = GIT_BUF_INIT;
	int     error = git_config_find_global_r(&path);

	if (error == GIT_SUCCESS) {
		if (path.size > GIT_PATH_MAX)
			error = git__throw(GIT_ESHORTBUFFER, "Path is too long");
		else
			git_buf_copy_cstr(global_config_path, GIT_PATH_MAX, &path);
	}

	git_buf_free(&path);

	return error;
}

int git_config_find_system_r(git_buf *path)
{
	return git_futils_find_system_file(path, GIT_CONFIG_FILENAME_SYSTEM);
}

int git_config_find_system(char *system_config_path)
{
	git_buf path  = GIT_BUF_INIT;
	int     error = git_config_find_system_r(&path);

	if (error == GIT_SUCCESS) {
		if (path.size > GIT_PATH_MAX)
			error = git__throw(GIT_ESHORTBUFFER, "Path is too long");
		else
			git_buf_copy_cstr(system_config_path, GIT_PATH_MAX, &path);
	}

	git_buf_free(&path);

	return error;
}

int git_config_open_global(git_config **out)
{
	int error;
	char global_path[GIT_PATH_MAX];

	if ((error = git_config_find_global(global_path)) < GIT_SUCCESS)
		return error;

	return git_config_open_ondisk(out, global_path);
}

