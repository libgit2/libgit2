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
#include <stdarg.h>

#include "git2/object.h"

#include "common.h"
#include "repository.h"
#include "commit.h"
#include "tag.h"
#include "blob.h"
#include "fileops.h"

#include "refs.h"

#define GIT_OBJECTS_INFO_DIR GIT_OBJECTS_DIR "info/"
#define GIT_OBJECTS_PACK_DIR GIT_OBJECTS_DIR "pack/"

#define GIT_BRANCH_MASTER "master"

typedef struct {
	char *path_repository;
	unsigned is_bare:1, has_been_reinit:1;
} repo_init;

/*
 * Git repository open methods
 *
 * Open a repository object from its path
 */
static int assign_repository_dirs(
		git_repository *repo,
		const char *git_dir,
		const char *git_object_directory,
		const char *git_index_file,
		const char *git_work_tree)
{
	char path_aux[GIT_PATH_MAX];
	size_t git_dir_path_len;
	int error = GIT_SUCCESS;

	assert(repo);

	if (git_dir == NULL)
		return GIT_ENOTFOUND;

	error = gitfo_prettify_dir_path(path_aux, sizeof(path_aux), git_dir);
	if (error < GIT_SUCCESS)
		return error;

	git_dir_path_len = strlen(path_aux);

	/* store GIT_DIR */
	repo->path_repository = git__strdup(path_aux);
	if (repo->path_repository == NULL)
		return GIT_ENOMEM;

	/* path to GIT_OBJECT_DIRECTORY */
	if (git_object_directory == NULL)
		git__joinpath(path_aux, repo->path_repository, GIT_OBJECTS_DIR);
	else {
		error = gitfo_prettify_dir_path(path_aux, sizeof(path_aux), git_object_directory);
		if (error < GIT_SUCCESS)
			return error;
	}

	/* Store GIT_OBJECT_DIRECTORY */
	repo->path_odb = git__strdup(path_aux);
	if (repo->path_odb == NULL)
		return GIT_ENOMEM;

	/* path to GIT_WORK_TREE */
	if (git_work_tree == NULL)
		repo->is_bare = 1;
	else {
		error = gitfo_prettify_dir_path(path_aux, sizeof(path_aux), git_work_tree);
		if (error < GIT_SUCCESS)
			return error;

		/* Store GIT_WORK_TREE */
		repo->path_workdir = git__strdup(path_aux);
		if (repo->path_workdir == NULL)
			return GIT_ENOMEM;

		/* Path to GIT_INDEX_FILE */
		if (git_index_file == NULL)
			git__joinpath(path_aux, repo->path_repository, GIT_INDEX_FILE);
		else {
			error = gitfo_prettify_file_path(path_aux, sizeof(path_aux), git_index_file);
			if (error < GIT_SUCCESS)
				return error;
		}

		/* store GIT_INDEX_FILE */
		repo->path_index = git__strdup(path_aux);
		if (repo->path_index == NULL)
			return GIT_ENOMEM;
	}
	
	return GIT_SUCCESS;
}

static int check_repository_dirs(git_repository *repo)
{
	char path_aux[GIT_PATH_MAX];

	if (gitfo_isdir(repo->path_repository) < GIT_SUCCESS)
		return GIT_ENOTAREPO;

	/* Ensure GIT_OBJECT_DIRECTORY exists */
	if (gitfo_isdir(repo->path_odb) < GIT_SUCCESS)
		return GIT_ENOTAREPO;

	/* Ensure HEAD file exists */
	git__joinpath(path_aux, repo->path_repository, GIT_HEAD_FILE);
	if (gitfo_exists(path_aux) < 0)
		return GIT_ENOTAREPO;

	return GIT_SUCCESS;
}

static int guess_repository_dirs(git_repository *repo, const char *repository_path)
{
	char buffer[GIT_PATH_MAX];
	const char *path_work_tree = NULL;

	/* Git directory name */
	if (git__basename_r(buffer, sizeof(buffer), repository_path) < 0)
		return GIT_EINVALIDPATH;

	if (strcmp(buffer, DOT_GIT) == 0) {
		/* Path to working dir */
		if (git__dirname_r(buffer, sizeof(buffer), repository_path) < 0)
			return GIT_EINVALIDPATH;
		path_work_tree = buffer;
	}

	return assign_repository_dirs(repo, repository_path, NULL, NULL, path_work_tree);
}

static git_repository *repository_alloc()
{
	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	git_cache_init(&repo->objects, GIT_DEFAULT_CACHE_SIZE, &git_object__free);

	if (git_repository__refcache_init(&repo->references) < GIT_SUCCESS) {
		free(repo);
		return NULL;
	}

	return repo;
}

static int init_odb(git_repository *repo)
{
	return git_odb_open(&repo->db, repo->path_odb);
}

int git_repository_open3(git_repository **repo_out,
		const char *git_dir,
		git_odb *object_database,
		const char *git_index_file,
		const char *git_work_tree)
{
	git_repository *repo;
	int error = GIT_SUCCESS;

	assert(repo_out);

	if (object_database == NULL)
		return GIT_ERROR;

	repo = repository_alloc();
	if (repo == NULL)
		return GIT_ENOMEM;

	error = assign_repository_dirs(repo, 
			git_dir, 
			NULL,
			git_index_file,
			git_work_tree);

	if (error < GIT_SUCCESS)
		goto cleanup;

	error = check_repository_dirs(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	repo->db = object_database;

	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	return error;
}


int git_repository_open2(git_repository **repo_out,
		const char *git_dir,
		const char *git_object_directory,
		const char *git_index_file,
		const char *git_work_tree)
{
	git_repository *repo;
	int error = GIT_SUCCESS;

	assert(repo_out);

	repo = repository_alloc();
	if (repo == NULL)
		return GIT_ENOMEM;

	error = assign_repository_dirs(repo,
			git_dir, 
			git_object_directory,
			git_index_file,
			git_work_tree);

	if (error < GIT_SUCCESS)
		goto cleanup;

	error = check_repository_dirs(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = init_odb(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	return error;
}

int git_repository_open(git_repository **repo_out, const char *path)
{
	git_repository *repo;
	int error = GIT_SUCCESS;

	assert(repo_out && path);

	repo = repository_alloc();
	if (repo == NULL)
		return GIT_ENOMEM;

	error = guess_repository_dirs(repo, path);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = check_repository_dirs(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = init_odb(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	return error;
}

void git_repository_free(git_repository *repo)
{
	if (repo == NULL)
		return;

	git_cache_free(&repo->objects);
	git_repository__refcache_free(&repo->references);

	free(repo->path_workdir);
	free(repo->path_index);
	free(repo->path_repository);
	free(repo->path_odb);

	if (repo->db != NULL)
		git_odb_close(repo->db);

	if (repo->index != NULL)
		git_index_free(repo->index);

	free(repo);
}

int git_repository_index(git_index **index_out, git_repository *repo)
{
	int error;

	assert(index_out && repo);

	if (repo->index == NULL) {
		error = git_index_open_inrepo(&repo->index, repo);
		if (error < GIT_SUCCESS)
			return error;

		assert(repo->index != NULL);
	}

	*index_out = repo->index;
	return GIT_SUCCESS;
}

git_odb *git_repository_database(git_repository *repo)
{
	assert(repo);
	return repo->db;
}

static int repo_init_reinit(repo_init *results)
{
	/* TODO: reinit the repository */
	results->has_been_reinit = 1;
	return GIT_ENOTIMPLEMENTED;
}

static int repo_init_createhead(git_repository *repo)
{
	git_reference *head_reference;
	return  git_reference_create_symbolic(&head_reference, repo, GIT_HEAD_FILE, GIT_REFS_HEADS_MASTER_FILE);
}

static int repo_init_check_head_existence(char * repository_path)
{
	char temp_path[GIT_PATH_MAX];

	git__joinpath(temp_path, repository_path, GIT_HEAD_FILE);
	return gitfo_exists(temp_path);
}

static int repo_init_structure(repo_init *results)
{
	const int mode = 0755; /* or 0777 ? */

	char temp_path[GIT_PATH_MAX];
	char *git_dir = results->path_repository;

	if (gitfo_mkdir_recurs(git_dir, mode))
		return GIT_ERROR;

	/* Creates the '/objects/info/' directory */
	git__joinpath(temp_path, git_dir, GIT_OBJECTS_INFO_DIR);
	if (gitfo_mkdir_recurs(temp_path, mode) < GIT_SUCCESS)
		return GIT_ERROR;

	/* Creates the '/objects/pack/' directory */
	git__joinpath(temp_path, git_dir, GIT_OBJECTS_PACK_DIR);
	if (gitfo_mkdir(temp_path, mode))
		return GIT_ERROR;

	/* Creates the '/refs/heads/' directory */
	git__joinpath(temp_path, git_dir, GIT_REFS_HEADS_DIR);
	if (gitfo_mkdir_recurs(temp_path, mode))
		return GIT_ERROR;

	/* Creates the '/refs/tags/' directory */
	git__joinpath(temp_path, git_dir, GIT_REFS_TAGS_DIR);
	if (gitfo_mkdir(temp_path, mode))
		return GIT_ERROR;

	/* TODO: what's left? templates? */

	return GIT_SUCCESS;
}

static int repo_init_find_dir(repo_init *results, const char* path)
{
	char temp_path[GIT_PATH_MAX];
	int error = GIT_SUCCESS;

	error = gitfo_prettify_dir_path(temp_path, sizeof(temp_path), path);
	if (error < GIT_SUCCESS)
		return error;

	if (!results->is_bare) {
		git__joinpath(temp_path, temp_path, GIT_DIR);
	}

	results->path_repository = git__strdup(temp_path);
	if (results->path_repository == NULL)
		return GIT_ENOMEM;

	return GIT_SUCCESS;
}

int git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare)
{
	int error = GIT_SUCCESS;
	git_repository *repo = NULL;
	repo_init results;
	
	assert(repo_out && path);

	results.path_repository = NULL;
	results.is_bare = is_bare;

	error = repo_init_find_dir(&results, path);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (!repo_init_check_head_existence(results.path_repository))
		return repo_init_reinit(&results);

	error = repo_init_structure(&results);
	if (error < GIT_SUCCESS)
		goto cleanup;

	repo = repository_alloc();
	if (repo == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = guess_repository_dirs(repo, results.path_repository);
	if (error < GIT_SUCCESS)
		goto cleanup;

	assert(repo->is_bare == is_bare);

	error = init_odb(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = repo_init_createhead(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* should never fail */
	assert(check_repository_dirs(repo) == GIT_SUCCESS);

	free(results.path_repository);
	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	free(results.path_repository);
	git_repository_free(repo);
	return error;
}

int git_repository_is_empty(git_repository *repo)
{
	git_reference *head, *branch;
	int error;

	error = git_reference_lookup(&head, repo, "HEAD");
	if (error < GIT_SUCCESS)
		return error;

	if (git_reference_type(head) != GIT_REF_SYMBOLIC)
		return GIT_EOBJCORRUPTED;

	return git_reference_resolve(&branch, head) == GIT_SUCCESS ? 0 : 1;
}

const char *git_repository_path(git_repository *repo)
{
	assert(repo);
	return repo->path_repository;
}

const char *git_repository_workdir(git_repository *repo)
{
	assert(repo);
	return repo->path_workdir;
}
