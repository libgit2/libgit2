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

static const int OBJECT_TABLE_SIZE = 32;

typedef struct {
	char *path_repository;
	unsigned is_bare:1, has_been_reinit:1;
} repo_init;

/*
 * Hash table methods
 *
 * Callbacks for the ODB cache, implemented
 * as a hash table
 */
uint32_t object_table_hash(const void *key, int hash_id)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
	memcpy(&r, id->id + (hash_id * sizeof(uint32_t)), sizeof(r));
	return r;
}

/*
 * Git repository open methods
 *
 * Open a repository object from its path
 */
static int assign_repository_DIRs(git_repository *repo,
		const char *git_dir,
		const char *git_object_directory,
		const char *git_index_file,
		const char *git_work_tree,
		int is_repo_being_created)
{
	char path_aux[GIT_PATH_MAX];
	size_t git_dir_path_len;
	int error = GIT_SUCCESS;

	assert(repo);

	if (git_dir == NULL)
		return GIT_ENOTFOUND;

	error = gitfo_prettify_dir_path(path_aux, git_dir);
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
		error = gitfo_prettify_dir_path(path_aux, git_object_directory);
		if (error < GIT_SUCCESS)
			return error;
	}

	/* Ensure GIT_OBJECT_DIRECTORY exists */
	if (gitfo_isdir(path_aux) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	/* Store GIT_OBJECT_DIRECTORY */
	repo->path_odb = git__strdup(path_aux);
	if (repo->path_odb == NULL)
		return GIT_ENOMEM;

	if (!is_repo_being_created) {
		/* Ensure HEAD file exists */
		git__joinpath(path_aux, repo->path_repository, GIT_HEAD_FILE);
		if (gitfo_exists(path_aux) < 0)
			return GIT_ENOTAREPO;
	}

	/* path to GIT_WORK_TREE */
	if (git_work_tree == NULL)
		repo->is_bare = 1;
	else {
		error = gitfo_prettify_dir_path(path_aux, git_work_tree);
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
			error = gitfo_prettify_file_path(path_aux, git_index_file);
			if (error < GIT_SUCCESS)
				return error;
		}

		if (!is_repo_being_created) {
			/* Ensure GIT_INDEX_FILE exists */
			if (gitfo_exists(path_aux) < 0)
				return GIT_ENOTAREPO;
		}

		/* store GIT_INDEX_FILE */
		repo->path_index = git__strdup(path_aux);
		if (repo->path_index == NULL)
			return GIT_ENOMEM;
	}
	
	return GIT_SUCCESS;
}

static int guess_repository_DIRs(git_repository *repo, const char *repository_path, int is_repo_being_created)
{
	char path_odb[GIT_PATH_MAX] = "\0", path_index[GIT_PATH_MAX] = "\0", path_work_tree[GIT_PATH_MAX] = "\0";
	char dir_name[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];

	int error = GIT_SUCCESS;

	/* Path to objects database */
	git__joinpath(path_odb, repository_path, GIT_OBJECTS_DIR);

	/* Git directory name */
	if (git__basename_r(dir_name, sizeof(dir_name), repository_path) < 0)
		return GIT_EINVALIDPATH;

	if (strcmp(dir_name, DOT_GIT) == 0) {
		
		/* Path to index file */
		git__joinpath(path_index, repository_path, GIT_INDEX_FILE);

		/* Path to working dir */
		if (git__dirname_r(path_work_tree, sizeof(path_work_tree), repository_path) < 0)
			return GIT_EINVALIDPATH;
	}

	error = assign_repository_DIRs(repo, repository_path, path_odb, !*path_index ? NULL : path_index, !*path_work_tree ? NULL : path_work_tree, is_repo_being_created);
	return error;
}

static git_repository *repository_alloc()
{
	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	repo->objects = git_hashtable_alloc(
			OBJECT_TABLE_SIZE, 
			object_table_hash,
			(git_hash_keyeq_ptr)git_oid_cmp);

	if (repo->objects == NULL) { 
		free(repo);
		return NULL;
	}

	if (git_repository__refcache_init(&repo->references) < GIT_SUCCESS) {
		git_hashtable_free(repo->objects);
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

	error = assign_repository_DIRs(repo, 
			git_dir, 
			NULL,
			git_index_file,
			git_work_tree,
			0);

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

	error = assign_repository_DIRs(repo, 
			git_dir, 
			git_object_directory,
			git_index_file,
			git_work_tree,
			0);

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

static int repository_open_internal(git_repository **repo_out, const char *path, int is_repo_being_created)
{
	git_repository *repo;
	int error = GIT_SUCCESS;

	assert(repo_out && path);

	repo = repository_alloc();
	if (repo == NULL)
		return GIT_ENOMEM;

	error = guess_repository_DIRs(repo, path, is_repo_being_created);
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
	return repository_open_internal(repo_out, path, 0);
}

void git_repository_free(git_repository *repo)
{
	git_object *object;
	const git_oid *oid;

	if (repo == NULL)
		return;

	free(repo->path_workdir);
	free(repo->path_index);
	free(repo->path_repository);
	free(repo->path_odb);

	GIT_HASHTABLE_FOREACH(repo->objects, oid, object, {
		git_object_free(object);
	});

	git_hashtable_free(repo->objects);

	git_repository__refcache_free(&repo->references);

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

static int create_object(git_object **object_out, git_otype type)
{
	git_object *object = NULL;

	assert(object_out);

	*object_out = NULL;

	switch (type) {
	case GIT_OBJ_COMMIT:
	case GIT_OBJ_TAG:
	case GIT_OBJ_BLOB:
		object = git__malloc(git_object__size(type));
		if (object == NULL)
			return GIT_ENOMEM;
		memset(object, 0x0, git_object__size(type));
		break;
		
	case GIT_OBJ_TREE:
		object = (git_object *)git_tree__new();
		if (object == NULL)
			return GIT_ENOMEM;
		break;

	default:
		return GIT_EINVALIDTYPE;
	}

	*object_out = object;
	return GIT_SUCCESS;
}

int git_repository_newobject(git_object **object_out, git_repository *repo, git_otype type)
{
	git_object *object = NULL;
	int error;

	assert(object_out && repo);

	if ((error = create_object(&object, type)) < GIT_SUCCESS)
		return error;

	object->repo = repo;
	object->in_memory = 1;
	object->modified = 1;

	object->source.raw.type = type;

	*object_out = object;
	return GIT_SUCCESS;
}

int git_repository_lookup(git_object **object_out, git_repository *repo, const git_oid *id, git_otype type)
{
	git_object *object = NULL;
	git_rawobj obj_file;
	int error = GIT_SUCCESS;

	assert(repo && object_out && id);

	object = git_hashtable_lookup(repo->objects, id);
	if (object != NULL) {
		*object_out = object;
		return GIT_SUCCESS;
	}

	error = git_odb_read(&obj_file, repo->db, id);
	if (error < GIT_SUCCESS)
		return error;

	if (type != GIT_OBJ_ANY && type != obj_file.type) {
		git_rawobj_close(&obj_file);
		return GIT_EINVALIDTYPE;
	}

	type = obj_file.type;

	if ((error = create_object(&object, type)) < GIT_SUCCESS)
		return error;

	/* Initialize parent object */
	git_oid_cpy(&object->id, id);
	object->repo = repo;
	memcpy(&object->source.raw, &obj_file, sizeof(git_rawobj));
	object->source.open = 1;

	switch (type) {
	case GIT_OBJ_COMMIT:
		error = git_commit__parse((git_commit *)object);
		break;

	case GIT_OBJ_TREE:
		error = git_tree__parse((git_tree *)object);
		break;

	case GIT_OBJ_TAG:
		error = git_tag__parse((git_tag *)object);
		break;

	case GIT_OBJ_BLOB:
		error = git_blob__parse((git_blob *)object);
		break;

	default:
		break;
	}

	if (error < GIT_SUCCESS) {
		git_object_free(object);
		return error;
	}

	git_object__source_close(object);
	git_hashtable_insert(repo->objects, &object->id, object);

	*object_out = object;
	return GIT_SUCCESS;
}

static int repo_init_reinit(repo_init *results)
{
	/* TODO: reinit the repository */
	results->has_been_reinit = 1;
	return GIT_SUCCESS;
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

	error = gitfo_prettify_dir_path(temp_path, path);
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

	error = repository_open_internal(repo_out, results.path_repository, 1);
	if (error < GIT_SUCCESS)
		goto cleanup;

	assert((*repo_out)->is_bare == is_bare);

	error = repo_init_createhead(*repo_out);

cleanup:
	free(results.path_repository);
	return error;
}
