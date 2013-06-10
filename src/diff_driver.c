/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"

#include "git2/attr.h"

#include "diff.h"
#include "diff_patch.h"
#include "diff_driver.h"
#include "strmap.h"
#include "pool.h"
#include "map.h"
#include "buf_text.h"

typedef enum {
	DIFF_DRIVER_AUTO = 0,
	DIFF_DRIVER_FALSE = 1,
	DIFF_DRIVER_TRUE = 2,
	DIFF_DRIVER_NAMED = 3,
} git_diff_driver_t;

enum {
	DIFF_CONTEXT_FIND_NORMAL = 0,
	DIFF_CONTEXT_FIND_ICASE = (1 << 0),
	DIFF_CONTEXT_FIND_EXT = (1 << 1),
};

/* data for finding function context for a given file type */
struct git_diff_driver {
	git_diff_driver_t type;
	git_strarray fn_patterns;
	int binary;
};

struct git_diff_driver_registry {
	git_strmap *drivers;
	git_pool strings;
};

static git_diff_driver global_drivers[3] = {
	{ DIFF_DRIVER_AUTO, { NULL, 0 }, -1 },
	{ DIFF_DRIVER_FALSE, { NULL, 0 }, 1 },
	{ DIFF_DRIVER_TRUE, { NULL, 0 }, 0 },
};

git_diff_driver_registry *git_diff_driver_registry_new()
{
	return git__calloc(1, sizeof(git_diff_driver_registry));
}

void git_diff_driver_registry_free(git_diff_driver_registry *reg)
{
	git__free(reg);
}

int git_diff_driver_lookup(
	git_diff_driver **out, git_repository *repo, const char *path)
{
	const char *value;

	assert(out);

	if (!repo || !path || !strlen(path))
		goto use_auto;

	if (git_attr_get(&value, repo, 0, path, "diff") < 0)
		return -1;

	if (GIT_ATTR_FALSE(value)) {
		*out = &global_drivers[DIFF_DRIVER_FALSE];
		return 0;
	}

	else if (GIT_ATTR_TRUE(value)) {
		*out = &global_drivers[DIFF_DRIVER_TRUE];
		return 0;
	}

	/* otherwise look for driver information in config and build driver */

use_auto:
	*out = &global_drivers[DIFF_DRIVER_AUTO];
	return 0;
}

void git_diff_driver_free(git_diff_driver *driver)
{
	GIT_UNUSED(driver);
	/* do nothing for now */
}

int git_diff_driver_is_binary(git_diff_driver *driver)
{
	return driver ? driver->binary : -1;
}

int git_diff_driver_content_is_binary(
	git_diff_driver *driver, const char *content, size_t content_len)
{
	const git_buf search = { (char *)content, 0, min(content_len, 4000) };

	GIT_UNUSED(driver);

	/* TODO: provide encoding / binary detection callbacks that can
	 * be UTF-8 aware, etc.  For now, instead of trying to be smart,
	 * let's just use the simple NUL-byte detection that core git uses.
	 */

	/* previously was: if (git_buf_text_is_binary(&search)) */
	if (git_buf_text_contains_nul(&search))
		return 1;

	return 0;
}

static long diff_context_find(
	const char *line,
	long line_len,
	char *out,
	long out_size,
	void *payload)
{
	git_diff_driver *driver = payload;
	const char *scan;

	GIT_UNUSED(driver);

	if (line_len > 0 && line[line_len - 1] == '\n')
		line_len--;
	if (line_len > 0 && line[line_len - 1] == '\r')
		line_len--;
	if (!line_len)
		return -1;

	if (!git__isalpha(*line) && *line != '_' && *line != '$')
		return -1;

	for (scan = &line[line_len-1]; scan > line && git__isspace(*scan); --scan)
		/* search backward for non-space */;
	line_len = scan - line;

	if (line_len >= out_size)
		line_len = out_size - 1;

	memcpy(out, line, line_len);
	out[line_len] = '\0';

	return line_len;
}

git_diff_find_context_fn git_diff_driver_find_content_fn(git_diff_driver *driver)
{
	GIT_UNUSED(driver);
	return diff_context_find;
}

