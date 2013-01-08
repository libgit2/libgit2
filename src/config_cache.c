/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "fileops.h"
#include "config.h"
#include "git2/config.h"
#include "vector.h"
#include "filter.h"
#include "repository.h"

struct map_data {
	const char *cvar_name;
	git_cvar_map *maps;
	size_t map_count;
	int default_value;
};

/*
 *	core.eol
 *		Sets the line ending type to use in the working directory for
 *	files that have the text property set. Alternatives are lf, crlf
 *	and native, which uses the platform's native line ending. The default
 *	value is native. See gitattributes(5) for more information on
 *	end-of-line conversion. 
 */
static git_cvar_map _cvar_map_eol[] = {
	{GIT_CVAR_FALSE, NULL, GIT_EOL_UNSET},
	{GIT_CVAR_STRING, "lf", GIT_EOL_LF},
	{GIT_CVAR_STRING, "crlf", GIT_EOL_CRLF},
	{GIT_CVAR_STRING, "native", GIT_EOL_NATIVE}
};

/*
 *	core.autocrlf
 *		Setting this variable to "true" is almost the same as setting 
 *	the text attribute to "auto" on all files except that text files are
 *	not guaranteed to be normalized: files that contain CRLF in the
 *	repository will not be touched. Use this setting if you want to have
 *	CRLF line endings in your working directory even though the repository
 *	does not have normalized line endings. This variable can be set to input,
 *	in which case no output conversion is performed.
 */
static git_cvar_map _cvar_map_autocrlf[] = {
	{GIT_CVAR_FALSE, NULL, GIT_AUTO_CRLF_FALSE},
	{GIT_CVAR_TRUE, NULL, GIT_AUTO_CRLF_TRUE},
	{GIT_CVAR_STRING, "input", GIT_AUTO_CRLF_INPUT}
};

static struct map_data _cvar_maps[] = {
	{"core.autocrlf", _cvar_map_autocrlf, ARRAY_SIZE(_cvar_map_autocrlf), GIT_AUTO_CRLF_DEFAULT},
	{"core.eol", _cvar_map_eol, ARRAY_SIZE(_cvar_map_eol), GIT_EOL_DEFAULT}
};

int git_repository__cvar(int *out, git_repository *repo, git_cvar_cached cvar)
{
	*out = repo->cvar_cache[(int)cvar];

	if (*out == GIT_CVAR_NOT_CACHED) {
		struct map_data *data = &_cvar_maps[(int)cvar];
		git_config *config;
		int error;

		error = git_repository_config__weakptr(&config, repo);
		if (error < 0)
			return error;

		error = git_config_get_mapped(out,
			config, data->cvar_name, data->maps, data->map_count);

		if (error == GIT_ENOTFOUND)
			*out = data->default_value;

		else if (error < 0)
			return error;

		repo->cvar_cache[(int)cvar] = *out;
	}

	return 0;
}

void git_repository__cvar_cache_clear(git_repository *repo)
{
	int i;

	for (i = 0; i < GIT_CVAR_CACHE_MAX; ++i)
		repo->cvar_cache[i] = GIT_CVAR_NOT_CACHED;
}

