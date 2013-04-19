/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_config_backend_h__
#define INCLUDE_sys_git_config_backend_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/config.h"

/**
 * @file git2/sys/config.h
 * @brief Git config backend routines
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Generic backend that implements the interface to
 * access a configuration file
 */
struct git_config_backend {
	unsigned int version;
	struct git_config *cfg;

	/* Open means open the file/database and parse if necessary */
	int (*open)(struct git_config_backend *, unsigned int level);
	int (*get)(const struct git_config_backend *, const char *key, const git_config_entry **entry);
	int (*get_multivar)(struct git_config_backend *, const char *key, const char *regexp, git_config_foreach_cb callback, void *payload);
	int (*set)(struct git_config_backend *, const char *key, const char *value);
	int (*set_multivar)(git_config_backend *cfg, const char *name, const char *regexp, const char *value);
	int (*del)(struct git_config_backend *, const char *key);
	int (*foreach)(struct git_config_backend *, const char *, git_config_foreach_cb callback, void *payload);
	int (*refresh)(struct git_config_backend *);
	void (*free)(struct git_config_backend *);
};
#define GIT_CONFIG_BACKEND_VERSION 1
#define GIT_CONFIG_BACKEND_INIT {GIT_CONFIG_BACKEND_VERSION}

/**
 * Add a generic config file instance to an existing config
 *
 * Note that the configuration object will free the file
 * automatically.
 *
 * Further queries on this config object will access each
 * of the config file instances in order (instances with
 * a higher priority level will be accessed first).
 *
 * @param cfg the configuration to add the file to
 * @param file the configuration file (backend) to add
 * @param level the priority level of the backend
 * @param force if a config file already exists for the given
 *  priority level, replace it
 * @return 0 on success, GIT_EEXISTS when adding more than one file
 *  for a given priority level (and force_replace set to 0), or error code
 */
GIT_EXTERN(int) git_config_add_backend(
	git_config *cfg,
	git_config_backend *file,
	unsigned int level,
	int force);

/** @} */
GIT_END_DECL
#endif
