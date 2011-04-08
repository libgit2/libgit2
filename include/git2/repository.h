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
 * Get the object database behind a Git repository
 *
 * @param repo a repository object
 * @return a pointer to the object db
 */
GIT_EXTERN(git_odb *) git_repository_database(git_repository *repo);

/**
 * Get the Index file of a Git repository
 *
 * This is a cheap operation; the index is only opened on the first call,
 * and subsequent calls only retrieve the previous pointer.
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

/** @} */
GIT_END_DECL
#endif
