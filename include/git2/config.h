/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_config_h__
#define INCLUDE_git_config_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/config.h
 * @brief Git config management routines
 * @defgroup git_config Git config management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Generic backend that implements the interface to
 * access a configuration file
 */
struct git_config_file {
	struct git_config *cfg;

	/* Open means open the file/database and parse if necessary */
	int (*open)(struct git_config_file *);
	int (*get)(struct git_config_file *, const char *key, const char **value);
	int (*set)(struct git_config_file *, const char *key, const char *value);
	int (*delete)(struct git_config_file *, const char *key);
	int (*foreach)(struct git_config_file *, int (*fn)(const char *, const char *, void *), void *data);
	void (*free)(struct git_config_file *);
};

/**
 * Locate the path to the global configuration file
 *
 * The user or global configuration file is usually
 * located in `$HOME/.gitconfig`.
 *
 * This method will try to guess the full path to that
 * file, if the file exists. The returned path
 * may be used on any `git_config` call to load the
 * global configuration file.
 *
 * @param global_config_path Buffer of GIT_PATH_MAX length to store the path
 * @return GIT_SUCCESS if a global configuration file has been
 *	found. Its path will be stored in `buffer`.
 */
GIT_EXTERN(int) git_config_find_global(char *global_config_path);

/**
 * Locate the path to the system configuration file
 *
 * If /etc/gitconfig doesn't exist, it will look for
 * %PROGRAMFILES%\Git\etc\gitconfig.

 * @param system_config_path Buffer of GIT_PATH_MAX length to store the path
 * @return GIT_SUCCESS if a system configuration file has been
 *	found. Its path will be stored in `buffer`.
 */
GIT_EXTERN(int) git_config_find_system(char *system_config_path);

/**
 * Open the global configuration file
 *
 * Utility wrapper that calls `git_config_find_global`
 * and opens the located file, if it exists.
 *
 * @param out Pointer to store the config instance
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_open_global(git_config **out);

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
GIT_EXTERN(int) git_config_file__ondisk(struct git_config_file **out, const char *path);

/**
 * Allocate a new configuration object
 *
 * This object is empty, so you have to add a file to it before you
 * can do anything with it.
 *
 * @param out pointer to the new configuration
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_new(git_config **out);

/**
 * Add a generic config file instance to an existing config
 *
 * Note that the configuration object will free the file
 * automatically.
 *
 * Further queries on this config object will access each
 * of the config file instances in order (instances with
 * a higher priority will be accessed first).
 *
 * @param cfg the configuration to add the file to
 * @param file the configuration file (backend) to add
 * @param priority the priority the backend should have
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_add_file(git_config *cfg, git_config_file *file, int priority);

/**
 * Add an on-disk config file instance to an existing config
 *
 * The on-disk file pointed at by `path` will be opened and
 * parsed; it's expected to be a native Git config file following
 * the default Git config syntax (see man git-config).
 *
 * Note that the configuration object will free the file
 * automatically.
 *
 * Further queries on this config object will access each
 * of the config file instances in order (instances with
 * a higher priority will be accessed first).
 *
 * @param cfg the configuration to add the file to
 * @param path path to the configuration file (backend) to add
 * @param priority the priority the backend should have
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_add_file_ondisk(git_config *cfg, const char *path, int priority);


/**
 * Create a new config instance containing a single on-disk file
 *
 * This method is a simple utility wrapper for the following sequence
 * of calls:
 *	- git_config_new
 *	- git_config_add_file_ondisk
 *
 * @param cfg The configuration instance to create
 * @param path Path to the on-disk file to open
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_open_ondisk(git_config **cfg, const char *path);

/**
 * Free the configuration and its associated memory and files
 *
 * @param cfg the configuration to free
 */
GIT_EXTERN(void) git_config_free(git_config *cfg);

/**
 * Get the value of an integer config variable.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param out pointer to the variable where the value should be stored
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_get_int32(git_config *cfg, const char *name, int32_t *out);

/**
 * Get the value of a long integer config variable.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param out pointer to the variable where the value should be stored
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_get_int64(git_config *cfg, const char *name, int64_t *out);

/**
 * Get the value of a boolean config variable.
 *
 * This function uses the usual C convention of 0 being false and
 * anything else true.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param out pointer to the variable where the value should be stored
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_get_bool(git_config *cfg, const char *name, int *out);

/**
 * Get the value of a string config variable.
 *
 * The string is owned by the variable and should not be freed by the
 * user.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param out pointer to the variable's value
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_get_string(git_config *cfg, const char *name, const char **out);

/**
 * Set the value of an integer config variable.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param value Integer value for the variable
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_set_int32(git_config *cfg, const char *name, int32_t value);

/**
 * Set the value of a long integer config variable.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param value Long integer value for the variable
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_set_int64(git_config *cfg, const char *name, int64_t value);

/**
 * Set the value of a boolean config variable.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param value the value to store
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_set_bool(git_config *cfg, const char *name, int value);

/**
 * Set the value of a string config variable.
 *
 * A copy of the string is made and the user is free to use it
 * afterwards.
 *
 * @param cfg where to look for the variable
 * @param name the variable's name
 * @param value the string to store.
 * @return GIT_SUCCESS or an error code
 */
GIT_EXTERN(int) git_config_set_string(git_config *cfg, const char *name, const char *value);

/**
 * Delete a config variable
 *
 * @param cfg the configuration
 * @param name the variable to delete
 */
GIT_EXTERN(int) git_config_delete(git_config *cfg, const char *name);

/**
 * Perform an operation on each config variable.
 *
 * The callback receives the normalized name and value of each variable
 * in the config backend, and the data pointer passed to this function.
 * As soon as one of the callback functions returns something other than 0,
 * this function returns that value.
 *
 * @param cfg where to get the variables from
 * @param callback the function to call on each variable
 * @param payload the data to pass to the callback
 * @return GIT_SUCCESS or the return value of the callback which didn't return 0
 */
GIT_EXTERN(int) git_config_foreach(
	git_config *cfg,
	int (*callback)(const char *var_name, const char *value, void *payload),
	void *payload);

/** @} */
GIT_END_DECL
#endif
