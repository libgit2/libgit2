/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "fileops.h"
#include "hashtable.h"
#include "config.h"
#include "git2/config.h"
#include "vector.h"

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

	for(i = 0; i < cfg->files.length; ++i){
		internal = git_vector_get(&cfg->files, i);
		file = internal->file;
		file->free(file);
		free(internal);
	}

	git_vector_free(&cfg->files);
	free(cfg);
}

static int config_backend_cmp(const void *a, const void *b)
{
	const file_internal *bk_a = *(const file_internal **)(a);
	const file_internal *bk_b = *(const file_internal **)(b);

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
		free(cfg);
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
		return git__throw(error, "Failed to open config file");

	internal = git__malloc(sizeof(file_internal));
	if (internal == NULL)
		return GIT_ENOMEM;

	internal->file = file;
	internal->priority = priority;

	if (git_vector_insert(&cfg->files, internal) < 0) {
		free(internal);
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


/**************
 * Setters
 **************/

/*
 * Internal function to actually set the string value of a variable
 */

int git_config_set_long(git_config *cfg, const char *name, long int value)
{
	char str_value[32]; /* All numbers should fit in here */
	snprintf(str_value, sizeof(str_value), "%ld", value);
	return git_config_set_string(cfg, name, str_value);
}

int git_config_set_int(git_config *cfg, const char *name, int value)
{
	return git_config_set_long(cfg, name, value);
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

int git_config_get_long(git_config *cfg, const char *name, long int *out)
{
	const char *value, *num_end;
	int ret;
	long int num;

	ret = git_config_get_string(cfg, name, &value);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to get value for %s", name);

	ret = git__strtol32(&num, value, &num_end, 0);
	if (ret < GIT_SUCCESS)
		return git__rethrow(ret, "Failed to get value for %s", name);

	switch (*num_end) {
	case '\0':
		break;
	case 'k':
	case 'K':
		num *= 1024;
		break;
	case 'm':
	case 'M':
		num *= 1024 * 1024;
		break;
	case 'g':
	case 'G':
		num *= 1024 * 1024 * 1024;
		break;
	default:
		return git__throw(GIT_EINVALIDTYPE, "Failed to get value for %s. Value is of invalid type", name);
	}

	*out = num;

	return GIT_SUCCESS;
}

int git_config_get_int(git_config *cfg, const char *name, int *out)
{
	long int tmp;
	int ret;

	ret = git_config_get_long(cfg, name, &tmp);

	*out = (int) tmp;

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
	error = git_config_get_int(cfg, name, out);
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

int git_config_open_global(git_config **out)
{
	int error;
	char global_path[GIT_PATH_MAX];

	if ((error = git_config_find_global(global_path)) < GIT_SUCCESS)
		return error;

	return git_config_open_ondisk(out, global_path);
}

