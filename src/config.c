/*
 * Copyright (C) 2009-2011 the libgit2 contributors
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

void git_config_free(git_config *cfg)
{
	unsigned int i;
	git_config_file *file;
	file_internal *internal;

	if (cfg == NULL)
		return;

	for(i = 0; i < cfg->files.length; ++i){
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		file->free(file);
		git__free(internal);
	}

	git_vector_free(&cfg->files);
	git__free(cfg);
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
		return git__rethrow(error, "Failed to open config file");

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
	return git_config_set_string(cfg, name, NULL);
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

/***********
 * Getters
 ***********/

int git_config_get_int64(git_config *cfg, const char *name, int64_t *out)
{
	const char *value, *num_end;
	int ret;
	int64_t num;

	ret = git_config_get_string(cfg, name, &value);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to retrieve value for '%s'", name);

	ret = git__strtol64(&num, value, &num_end, 0);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to convert value for '%s'", name);

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
			return git__throw(GIT_EINVALIDTYPE,
				"Failed to get value for '%s'. Invalid type suffix", name);

		/* fallthrough */

	case '\0':
		*out = num;
		return GIT_SUCCESS;

	default:
		return git__throw(GIT_EINVALIDTYPE,
			"Failed to get value for '%s'. Value is of invalid type", name);
	}
}

int git_config_get_int32(git_config *cfg, const char *name, int32_t *out)
{
	int64_t tmp_long;
	int32_t tmp_int;
	int ret;

	ret = git_config_get_int64(cfg, name, &tmp_long);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to convert value for '%s'", name);
	
	tmp_int = tmp_long & 0xFFFFFFFF;
	if (tmp_int != tmp_long)
		return git__throw(GIT_EOVERFLOW, "Value for '%s' is too large", name);

	*out = tmp_int;

	return ret;
}

int git_config_get_bool(git_config *cfg, const char *name, int *out)
{
	const char *value;
	int error = GIT_SUCCESS;

	error = git_config_get_string(cfg, name, &value);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to get value for %s", name);

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

	/* Try to parse it as an integer */
	error = git_config_get_int32(cfg, name, out);
	if (error == GIT_SUCCESS)
		*out = !!(*out);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to get value for %s", name);
	return error;
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

int git_config_find_global(char *global_config_path)
{
	const char *home;

	home = getenv("HOME");

#ifdef GIT_WIN32
	if (home == NULL)
		home = getenv("USERPROFILE");
#endif

	if (home == NULL)
		return git__throw(GIT_EOSERR, "Failed to open global config file. Cannot locate the user's home directory");

	git_path_join(global_config_path, home, GIT_CONFIG_FILENAME);

	if (git_futils_exists(global_config_path) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to open global config file. The file does not exist");

	return GIT_SUCCESS;
}



#if GIT_WIN32
static int win32_find_system(char *system_config_path)
{
	const wchar_t *query = L"%PROGRAMFILES%\\Git\\etc\\gitconfig";
	wchar_t *apphome_utf16;
	char *apphome_utf8;
	DWORD size, ret;

	size = ExpandEnvironmentStringsW(query, NULL, 0);
	/* The function gave us the full size of the buffer in chars, including NUL */
	apphome_utf16 = git__malloc(size * sizeof(wchar_t));
	if (apphome_utf16 == NULL)
		return GIT_ENOMEM;

	ret = ExpandEnvironmentStringsW(query, apphome_utf16, size);
	if (ret != size)
		return git__throw(GIT_ERROR, "Failed to expand environment strings");

	if (_waccess(apphome_utf16, F_OK) < 0) {
		git__free(apphome_utf16);
		return GIT_ENOTFOUND;
	}

	apphome_utf8 = gitwin_from_utf16(apphome_utf16);
	git__free(apphome_utf16);

	if (strlen(apphome_utf8) >= GIT_PATH_MAX) {
		git__free(apphome_utf8);
		return git__throw(GIT_ESHORTBUFFER, "Path is too long");
	}

	strcpy(system_config_path, apphome_utf8);
	git__free(apphome_utf8);
	return GIT_SUCCESS;
}
#endif

int git_config_find_system(char *system_config_path)
{
	const char *etc = "/etc/gitconfig";

	if (git_futils_exists(etc) == GIT_SUCCESS) {
		memcpy(system_config_path, etc, strlen(etc) + 1);
		return GIT_SUCCESS;
	}

#if GIT_WIN32
	return win32_find_system(system_config_path);
#else
	return GIT_ENOTFOUND;
#endif
}

int git_config_open_global(git_config **out)
{
	int error;
	char global_path[GIT_PATH_MAX];

	if ((error = git_config_find_global(global_path)) < GIT_SUCCESS)
		return error;

	return git_config_open_ondisk(out, global_path);
}

