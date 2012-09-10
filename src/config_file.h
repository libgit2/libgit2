/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_config_file_h__
#define INCLUDE_config_file_h__

#include "git2/config.h"

GIT_INLINE(int) git_config_file_open(git_config_file *cfg, unsigned int level)
{
	return cfg->open(cfg, level);
}

GIT_INLINE(void) git_config_file_free(git_config_file *cfg)
{
	cfg->free(cfg);
}

GIT_INLINE(int) git_config_file_get_string(
	const git_config_entry **out, git_config_file *cfg, const char *name)
{
	return cfg->get(cfg, name, out);
}

GIT_INLINE(int) git_config_file_set_string(
	git_config_file *cfg, const char *name, const char *value)
{
	return cfg->set(cfg, name, value);
}

GIT_INLINE(int) git_config_file_delete(
	git_config_file *cfg, const char *name)
{
	return cfg->del(cfg, name);
}

GIT_INLINE(int) git_config_file_foreach(
	git_config_file *cfg,
	int (*fn)(const git_config_entry *entry, void *data),
	void *data)
{
	return cfg->foreach(cfg, NULL, fn, data);
}

GIT_INLINE(int) git_config_file_foreach_match(
	git_config_file *cfg,
	const char *regexp,
	int (*fn)(const git_config_entry *entry, void *data),
	void *data)
{
	return cfg->foreach(cfg, regexp, fn, data);
}

#endif

