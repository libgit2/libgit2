/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "strmap.h"

int git_strmap_next(
	const char **key,
	void **data,
	git_strmap_iter* iter,
	git_strmap *map)
{
	if (!map)
		return GIT_ERROR;

	while (*iter != git_strmap_end(map)) {
		if (!(git_strmap_has_data(map, *iter))) {
			++(*iter);
			continue;
		}

		*key = git_strmap_key(map, *iter);
		*data = git_strmap_value_at(map, *iter);

		++(*iter);

		return GIT_OK;
	}

	return GIT_ITEROVER;
}
