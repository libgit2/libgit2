/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_config_h__
#define INCLUDE_config_h__

#include "git2.h"
#include "git2/config.h"
#include "vector.h"
#include "repository.h"

#define GIT_CONFIG_FILENAME_SYSTEM "gitconfig"
#define GIT_CONFIG_FILENAME_GLOBAL ".gitconfig"
#define GIT_CONFIG_FILENAME_XDG    "config"

#define GIT_CONFIG_FILENAME_INREPO "config"
#define GIT_CONFIG_FILE_MODE 0666

struct git_config {
	git_refcount rc;
	git_vector files;
};

extern int git_config_find_global_r(git_buf *global_config_path);
extern int git_config_find_xdg_r(git_buf *system_config_path);
extern int git_config_find_system_r(git_buf *system_config_path);

extern int git_config_rename_section(
	git_repository *repo,
	const char *old_section_name,	/* eg "branch.dummy" */
	const char *new_section_name);	/* NULL to drop the old section */

/**
 * Create a configuration file backend for ondisk files
 *
 * These are the normal `.gitconfig` files that Core Git
 * processes. Note that you first have to add this file to a
 * configuration object before you can query it for configuration
 * variables.
 *
 * @param out the new backend
 * @param path where the config file is located
 */
extern int git_config_file__ondisk(struct git_config_backend **out, const char *path);

#endif
