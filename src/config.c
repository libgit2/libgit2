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
#include "git2/config_backend.h"
#include "vector.h"

#include <ctype.h>

typedef struct {
	git_config_backend *backend;
	int priority;
} backend_internal;

int git_config_open_bare(git_config **out, const char *path)
{
	git_config_backend *backend = NULL;
	git_config *cfg = NULL;
	int error = GIT_SUCCESS;

	error = git_config_new(&cfg);
	if (error < GIT_SUCCESS)
		goto error;

	error = git_config_backend_file(&backend, path);
	if (error < GIT_SUCCESS)
		goto error;

	error = git_config_add_backend(cfg, backend, 1);
	if (error < GIT_SUCCESS)
		goto error;

	error = backend->open(backend);
	if (error < GIT_SUCCESS)
		goto error;

	*out = cfg;

	return error;

 error:
	if(backend)
		backend->free(backend);

	return error;
}

void git_config_free(git_config *cfg)
{
	unsigned int i;
	git_config_backend *backend;
	backend_internal *internal;

	for(i = 0; i < cfg->backends.length; ++i){
		internal = git_vector_get(&cfg->backends, i);
		backend = internal->backend;
		backend->free(backend);
		free(internal);
	}

	git_vector_free(&cfg->backends);
	free(cfg);
}

static int config_backend_cmp(const void *a, const void *b)
{
	const backend_internal *bk_a = *(const backend_internal **)(a);
	const backend_internal *bk_b = *(const backend_internal **)(b);

	return bk_b->priority - bk_a->priority;
}

int git_config_new(git_config **out)
{
	git_config *cfg;

	cfg = git__malloc(sizeof(git_config));
	if (cfg == NULL)
		return GIT_ENOMEM;

	memset(cfg, 0x0, sizeof(git_config));

	if (git_vector_init(&cfg->backends, 3, config_backend_cmp) < 0) {
		free(cfg);
		return GIT_ENOMEM;
	}

	*out = cfg;

	return GIT_SUCCESS;
}

int git_config_add_backend(git_config *cfg, git_config_backend *backend, int priority)
{
	backend_internal *internal;

	assert(cfg && backend);

	internal = git__malloc(sizeof(backend_internal));
	if (internal == NULL)
		return GIT_ENOMEM;

	internal->backend = backend;
	internal->priority = priority;

	if (git_vector_insert(&cfg->backends, internal) < 0) {
		free(internal);
		return GIT_ENOMEM;
	}

	git_vector_sort(&cfg->backends);
	internal->backend->cfg = cfg;

	return GIT_SUCCESS;
}

/*
 * Loop over all the variables
 */

int git_config_foreach(git_config *cfg, int (*fn)(const char *, void *), void *data)
{
	int ret = GIT_SUCCESS;
	unsigned int i;
	backend_internal *internal;
	git_config_backend *backend;

	for(i = 0; i < cfg->backends.length && ret == 0; ++i) {
		internal = git_vector_get(&cfg->backends, i);
		backend = internal->backend;
		ret = backend->foreach(backend, fn, data);
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
	char str_value[5]; /* Most numbers should fit in here */
	int buf_len = sizeof(str_value), ret;
	char *help_buf = NULL;

	if ((ret = snprintf(str_value, buf_len, "%ld", value)) >= buf_len - 1){
		/* The number is too large, we need to allocate more memory */
		buf_len = ret + 1;
		help_buf = git__malloc(buf_len);
		snprintf(help_buf, buf_len, "%ld", value);
		ret = git_config_set_string(cfg, name, help_buf);
		free(help_buf);
	} else {
		ret = git_config_set_string(cfg, name, str_value);
	}

	return ret;
}

int git_config_set_int(git_config *cfg, const char *name, int value)
{
	return git_config_set_long(cfg, name, value);
}

int git_config_set_bool(git_config *cfg, const char *name, int value)
{
	const char *str_value;

	if (value == 0)
		str_value = "false";
	else
		str_value = "true";

	return git_config_set_string(cfg, name, str_value);
}

int git_config_set_string(git_config *cfg, const char *name, const char *value)
{
	backend_internal *internal;
	git_config_backend *backend;

	assert(cfg->backends.length > 0);

	internal = git_vector_get(&cfg->backends, 0);
	backend = internal->backend;

	return backend->set(backend, name, value);
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
		return ret;

	ret = git__strtol32(&num, value, &num_end, 0);
	if (ret < GIT_SUCCESS)
		return ret;

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
		return GIT_EINVALIDTYPE;
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
		return error;

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

	return error;
}

int git_config_get_string(git_config *cfg, const char *name, const char **out)
{
	backend_internal *internal;
	git_config_backend *backend;

	assert(cfg->backends.length > 0);

	internal = git_vector_get(&cfg->backends, 0);
	backend = internal->backend;

	return backend->get(backend, name, out);
}

