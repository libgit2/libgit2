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
#ifndef INCLUDE_git_repository_h__
#define INCLUDE_git_repository_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/repository.h
 * @brief Git repository management routines
 * @defgroup git_repository Git repository management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Open a git repository.
 *
 * The 'path' argument must point to an existing git repository
 * folder, e.g.
 *
 *		/path/to/my_repo/.git/	(normal repository)
 *							objects/
 *							index
 *							HEAD
 *
 *		/path/to/bare_repo/		(bare repository)
 *						objects/
 *						index
 *						HEAD
 *
 *	The method will automatically detect if 'path' is a normal
 *	or bare repository or fail is 'path' is neither.
 *
 * @param repository pointer to the repo which will be opened
 * @param path the path to the repository
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_repository_open(git_repository **repository, const char *path);


/**
 * Open a git repository by manually specifying all its paths
 *
 * @param repository pointer to the repo which will be opened
 *
 * @param git_dir The full path to the repository folder
 *		e.g. a '.git' folder for live repos, any folder for bare
 *		Equivalent to $GIT_DIR.
 *		Cannot be NULL.
 *
 * @param git_object_directory The full path to the ODB folder.
 *		the folder where all the loose and packed objects are stored
 *		Equivalent to $GIT_OBJECT_DIRECTORY.
 *		If NULL, "$GIT_DIR/objects/" is assumed.
 *
 * @param git_index_file The full path to the index (dircache) file
 *		Equivalent to $GIT_INDEX_FILE.
 *		If NULL, "$GIT_DIR/index" is assumed.
 *
 * @param git_work_tree The full path to the working tree of the repository,
 *		if the repository is not bare.
 *		Equivalent to $GIT_WORK_TREE.
 *		If NULL, the repository is assumed to be bare.
 *
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_repository_open2(git_repository **repository,
		const char *git_dir,
		const char *git_object_directory,
		const char *git_index_file,
		const char *git_work_tree);


/**
 * Open a git repository by manually specifying its paths and
 * the object database it will use.
 *
 * @param repository pointer to the repo which will be opened
 *
 * @param git_dir The full path to the repository folder
 *		e.g. a '.git' folder for live repos, any folder for bare
 *		Equivalent to $GIT_DIR.
 *		Cannot be NULL.
 *
 * @param object_database A pointer to a git_odb created & initialized
 *		by the user (e.g. with custom backends). This object database
 *		will be owned by the repository and will be automatically free'd.
 *		It should not be manually free'd by the user, or this
 *		git_repository object will become invalid.
 *
 * @param git_index_file The full path to the index (dircache) file
 *		Equivalent to $GIT_INDEX_FILE.
 *		If NULL, "$GIT_DIR/index" is assumed.
 *
 * @param git_work_tree The full path to the working tree of the repository,
 *		if the repository is not bare.
 *		Equivalent to $GIT_WORK_TREE.
 *		If NULL, the repository is assumed to be bare.
 *
 * @return 0 on success; error code otherwise
 */

GIT_EXTERN(int) git_repository_open3(git_repository **repository,
		const char *git_dir,
		git_odb *object_database,
		const char *git_index_file,
		const char *git_work_tree);

/**
 * Look for a git repository and copy its path in the given buffer. The lookup start
 * from base_path and walk across parent directories if nothing has been found. The
 * lookup ends when the first repository is found, or when reaching a directory
 * referenced in ceiling_dirs or when the filesystem changes (in case across_fs
 * is true).
 *
 * The method will automatically detect if the repository is bare (if there is
 * a repository).
 *
 * @param repository_path The user allocated buffer which will contain the found path.
 *
 * @param size repository_path size
 *
 * @param start_path The base path where the lookup starts.
 *
 * @param across_fs If true, then the lookup will not stop when a filesystem device change
 * is detected while exploring parent directories.
 *
 * @param ceiling_dirs A GIT_PATH_LIST_SEPARATOR separated list of absolute symbolic link
 * free paths. The lookup will stop when any of this paths is reached. Note that the
 * lookup always performs on start_path no matter start_path appears in ceiling_dirs
 * ceiling_dirs might be NULL (which is equivalent to an empty string)
 *
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_repository_discover(char *repository_path, size_t size, const char *start_path, int across_fs, const char *ceiling_dirs);

/**
 * Get the object database behind a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the object db
 */
GIT_EXTERN(git_odb *) git_repository_database(git_repository *repo);

/**
 * Open the Index file of a Git repository
 *
 * This returns a new and unique `git_index` object representing the
 * active index for the repository.
 *
 * This method may be called more than once (e.g. on different threads).
 *
 * Each returned `git_index` object is independent and suffers no race
 * conditions: synchronization is done at the FS level.
 *
 * Each returned `git_index` object must be manually freed by the user,
 * using `git_index_free`.
 *
 * @param index Pointer where to store the index
 * @param repo a repository object
 * @return 0 on success; error code if the index could not be opened
 */
GIT_EXTERN(int) git_repository_index(git_index **index, git_repository *repo);

/**
 * Free a previously allocated repository
 *
 * Note that after a repository is free'd, all the objects it has spawned
 * will still exist until they are manually closed by the user
 * with `git_object_close`, but accessing any of the attributes of
 * an object without a backing repository will result in undefined
 * behavior
 *
 * @param repo repository handle to close. If NULL nothing occurs.
 */
GIT_EXTERN(void) git_repository_free(git_repository *repo);

/**
 * Creates a new Git repository in the given folder.
 *
 * TODO:
 *	- Reinit the repository
 *	- Create config files
 *
 * @param repo_out pointer to the repo which will be created or reinitialized
 * @param path the path to the repository
 * @param is_bare if true, a Git repository without a working directory is created
 *		at the pointed path. If false, provided path will be considered as the working
 *		directory into which the .git directory will be created.
 *
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare);

/**
 * Check if a repository's HEAD is detached
 *
 * A repository's HEAD is detached when it points directly to a commit
 * instead of a branch.
 *
 * @param repo Repo to test
 * @return 1 if HEAD is detached, 0 if i'ts not; error code if there
 * was an error.
 */
GIT_EXTERN(int) git_repository_head_detached(git_repository *repo);

/**
 * Check if the current branch is an orphan
 *
 * An orphan branch is one named from HEAD but which doesn't exist in
 * the refs namespace, because it doesn't have any commit to point to.
 *
 * @param repo Repo to test
 * @return 1 if the current branch is an orphan, 0 if it's not; error
 * code if therewas an error
 */
GIT_EXTERN(int) git_repository_head_orphan(git_repository *repo);

/**
 * Check if a repository is empty
 *
 * An empty repository has just been initialized and contains
 * no commits.
 *
 * @param repo Repo to test
 * @return 1 if the repository is empty, 0 if it isn't, error code
 * if the repository is corrupted
 */
GIT_EXTERN(int) git_repository_is_empty(git_repository *repo);

/**
 * Internal path identifiers for a repository
 */
typedef enum {
	GIT_REPO_PATH,
	GIT_REPO_PATH_INDEX,
	GIT_REPO_PATH_ODB,
	GIT_REPO_PATH_WORKDIR
} git_repository_pathid;

/**
 * Get one of the paths to the repository
 *
 * Possible values for `id`:
 *
 *	GIT_REPO_PATH: return the path to the repository
 *	GIT_REPO_PATH_INDEX: return the path to the index
 *	GIT_REPO_PATH_ODB: return the path to the ODB
 *	GIT_REPO_PATH_WORKDIR: return the path to the working
 *		directory
 *
 * @param repo a repository object
 * @param id The ID of the path to return
 * @return absolute path of the requested id
 */
GIT_EXTERN(const char *) git_repository_path(git_repository *repo, git_repository_pathid id);

/**
 * Check if a repository is bare
 *
 * @param repo Repo to test
 * @return 1 if the repository is empty, 0 otherwise.
 */
GIT_EXTERN(int) git_repository_is_bare(git_repository *repo);

/**
 * Retrieve the relevant configuration for a repository
 *
 * By default he returned `git_config` instance contains a single
 * configuration file, the `.gitconfig' file that may be found
 * inside the repository.
 *
 * If the `user_config_path` variable is not NULL, the given config
 * file will be also included in the configuration set. On most UNIX
 * systems, this file may be found on `$HOME/.gitconfig`.
 *
 * If the `system_config_path` variable is not NULL, the given config
 * file will be also included in the configuration set. On most UNIX
 * systems, this file may be found on `$PREFIX/etc/gitconfig`.
 *
 * The resulting `git_config` instance will query the files in the following
 * order:
 *
 *	- Repository configuration file
 *	- User configuration file
 *	- System configuration file
 *
 * The method will fail if any of the passed config files cannot be
 * found or accessed.
 *
 * The returned `git_config` instance is owned by the caller and must
 * be manually free'd once it's no longer on use.
 *
 * @param out the repository's configuration
 * @param repo the repository for which to get the config
 * @param user_config_path Path to the user config file
 * @param system_config_path Path to the system-wide config file
 */
GIT_EXTERN(int) git_repository_config(git_config **out,
	git_repository *repo,
	const char *user_config_path,
	const char *system_config_path);

/** @} */
GIT_END_DECL
#endif
