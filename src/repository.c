/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include <stdarg.h>

#include "git2/object.h"

#include "common.h"
#include "repository.h"
#include "commit.h"
#include "tag.h"
#include "blob.h"
#include "fileops.h"
#include "config.h"
#include "refs.h"

#define GIT_OBJECTS_INFO_DIR GIT_OBJECTS_DIR "info/"
#define GIT_OBJECTS_PACK_DIR GIT_OBJECTS_DIR "pack/"

#define GIT_FILE_CONTENT_PREFIX "gitdir: "

#define GIT_BRANCH_MASTER "master"

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
	git_path path_aux = GIT_PATH_INIT;
	int error = GIT_SUCCESS;

	assert(repo);

	if (git_dir == NULL)
		return git__throw(GIT_ENOTFOUND, "Failed to open repository. Git dir not found");

	error = git_path_prettify_dir(&path_aux, git_dir, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* store GIT_DIR */
	repo->path_repository = git__path_take_data(&path_aux);

	/* path to GIT_OBJECT_DIRECTORY */
	if (git_object_directory == NULL)
		error = git_path_join(&path_aux, repo->path_repository, GIT_OBJECTS_DIR);
	else
		error = git_path_prettify_dir(&path_aux, git_object_directory, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Store GIT_OBJECT_DIRECTORY */
	repo->path_odb = git__path_take_data(&path_aux);

	/* path to GIT_WORK_TREE */
	if (git_work_tree == NULL)
		repo->is_bare = 1;
	else {
		error = git_path_prettify_dir(&path_aux, git_work_tree, NULL);
		if (error < GIT_SUCCESS)
			goto cleanup;

		/* Store GIT_WORK_TREE */
		repo->path_workdir = git__path_take_data(&path_aux);

		/* Path to GIT_INDEX_FILE */
		if (git_index_file == NULL)
			error = git_path_join(&path_aux, repo->path_repository, GIT_INDEX_FILE);
		else
			error = git_path_prettify(&path_aux, git_index_file, NULL);
		if (error < GIT_SUCCESS)
			goto cleanup;

		/* store GIT_INDEX_FILE */
		repo->path_index = git__path_take_data(&path_aux);
	}

cleanup:
	git__path_free(&path_aux);

	return (error < GIT_SUCCESS) ?
		git__rethrow(error, "Failed to open repository") : error;
}

static int check_repository_dirs(git_repository *repo)
{
	int error = GIT_SUCCESS;
	git_path path_aux = GIT_PATH_INIT;

	if (git_futils_isdir(repo->path_repository) < GIT_SUCCESS)
		return git__throw(GIT_ENOTAREPO, "`%s` is not a folder", repo->path_repository);

	/* Ensure GIT_OBJECT_DIRECTORY exists */
	if (git_futils_isdir(repo->path_odb) < GIT_SUCCESS)
		return git__throw(GIT_ENOTAREPO, "`%s` does not exist", repo->path_odb);

	/* Ensure HEAD file exists */
	error = git_path_join(&path_aux, repo->path_repository, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		return error;

	if (git_futils_isfile(path_aux.data) < 0)
		error = git__throw(GIT_ENOTAREPO, "HEAD file is missing");

	git__path_free(&path_aux);
	return error;
}

static int guess_repository_dirs(git_repository *repo, const char *repository_path)
{
	git_path path = GIT_PATH_INIT;
	const char *path_work_tree = NULL;
	int error = GIT_SUCCESS;

	/* Git directory name */
	if (git_path_basename_r(&path, repository_path) < 0) {
		git__path_free(&path);
		return git__throw(GIT_EINVALIDPATH, "Unable to parse folder name from `%s`", repository_path);
	}

	if (strcmp(path.data, DOT_GIT) == 0) {
		/* Path to working dir */
		if (git_path_dirname_r(&path, repository_path) < 0) {
			git__path_free(&path);
			return git__throw(GIT_EINVALIDPATH, "Unable to parse parent folder name from `%s`", repository_path);
		}
		path_work_tree = path.data;
	}

	error = assign_repository_dirs(repo, repository_path, NULL, NULL, path_work_tree);

	git__path_free(&path);

	return error;
}

static int quickcheck_repository_dir(const char *repository_path)
{
	int error = GIT_SUCCESS;
	git_path path_aux = GIT_PATH_INIT;

	/* Check OBJECTS_DIR first, since it will generate the longest path name */
	error = git_path_join(&path_aux, repository_path, GIT_OBJECTS_DIR);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (git_futils_isdir(path_aux.data) < 0) {
		error = GIT_EINVALIDPATH;
		goto cleanup;
	}

	/* Ensure HEAD file exists */
	error = git_path_join(&path_aux, repository_path, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (git_futils_isfile(path_aux.data) < 0) {
		error = GIT_EINVALIDPATH;
		goto cleanup;
	}

	error = git_path_join(&path_aux, repository_path, GIT_REFS_DIR);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (git_futils_isdir(path_aux.data) < 0) {
		error = GIT_EINVALIDPATH;
		goto cleanup;
	}

cleanup:
	git__path_free(&path_aux);
	return error;
}

static git_repository *repository_alloc(void)
{
	int error;

	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	error = git_cache_init(&repo->objects, GIT_DEFAULT_CACHE_SIZE, &git_object__free);
	if (error < GIT_SUCCESS) {
		git__free(repo);
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
		return git__throw(GIT_EINVALIDARGS, "Failed to open repository. `object_database` can't be null");

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
	return git__rethrow(error, "Failed to open repository");
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
	return git__rethrow(error, "Failed to open repository");
}

int git_repository_config(
		git_config **out,
		git_repository *repo,
		const char *global_config_path,
		const char *system_config_path)
{
	git_path config_path = GIT_PATH_INIT;
	int error;

	assert(out && repo);

	error = git_config_new(out);
	if (error < GIT_SUCCESS)
		return error;

	error = git_path_join(&config_path, repo->path_repository, GIT_CONFIG_FILENAME_INREPO);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_config_add_file_ondisk(*out, config_path.data, 3);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (global_config_path != NULL) {
		error = git_config_add_file_ondisk(*out, global_config_path, 2);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	if (system_config_path != NULL) {
		error = git_config_add_file_ondisk(*out, system_config_path, 1);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	(*out)->repo = repo;
	return GIT_SUCCESS;

cleanup:
	git__path_free(&config_path);
	git_config_free(*out);
	return error;
}

int git_repository_config_autoload(
		git_config **out,
		git_repository *repo)
{
	char global[GIT_PATH_MAX], system[GIT_PATH_MAX];
	char *global_path, *system_path;
	int error;


	error = git_config_find_global(global);
	global_path = error < GIT_SUCCESS ? NULL : global;

	error = git_config_find_system(system);
	system_path = error < GIT_SUCCESS ? NULL : system;

	return git_repository_config(out, repo, global_path, system_path);
}

static int discover_repository_dirs(git_repository *repo, const char *path)
{
	int error;

	error = guess_repository_dirs(repo, path);
	if (error < GIT_SUCCESS)
		return error;

	error = check_repository_dirs(repo);
	if (error < GIT_SUCCESS)
		return error;

	return GIT_SUCCESS;
}

int git_repository_open(git_repository **repo_out, const char *path)
{
	git_repository *repo;
	int error = GIT_SUCCESS;

	assert(repo_out && path);

	repo = repository_alloc();
	if (repo == NULL)
		return GIT_ENOMEM;

	error = discover_repository_dirs(repo, path);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = init_odb(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	return git__rethrow(error, "Failed to open repository");
}

static int retrieve_device(dev_t *device_out, const char *path)
{
	struct stat path_info;

	assert(device_out);

	if (p_lstat(path, &path_info))
		return git__throw(GIT_EOSERR, "Failed to get file informations: %s", path);

	*device_out = path_info.st_dev;

	return GIT_SUCCESS;
}

static int retrieve_ceiling_directories_offset(const char *path, const char *ceiling_directories)
{
	char buf[GIT_PATH_MAX + 1];
	char buf2[GIT_PATH_MAX + 1];
	const char *ceil, *sep;
	int len, max_len = -1;
	int min_len;

	assert(path);

	min_len = git_path_root(path) + 1;

	if (ceiling_directories == NULL || min_len == 0)
		return min_len;

	for (sep = ceil = ceiling_directories; *sep; ceil = sep + 1) {
		for (sep = ceil; *sep && *sep != GIT_PATH_LIST_SEPARATOR; sep++);
		len = sep - ceil;

		if (len == 0 || len > GIT_PATH_MAX || git_path_root(ceil) == -1)
			continue;

		strncpy(buf, ceil, len);
		buf[len] = '\0';

		if (p_realpath(buf, buf2) == NULL)
			continue;

		len = strlen(buf2);
		if (len > 0 && buf2[len-1] == '/')
			buf[--len] = '\0';

		if (!strncmp(path, buf2, len) &&
			path[len] == '/' &&
			len > max_len)
		{
			max_len = len;
		}
	}

	return max_len <= min_len ? min_len : max_len;
}

static int read_gitfile(char *path_out, const char *file_path, const char *base_path)
{
	git_fbuffer file;
	int error;
	size_t end_offset;
	char *data;
	git_path found_path = GIT_PATH_INIT;

	assert(path_out && file_path && base_path);

	error = git_futils_readbuffer(&file, file_path);

	if (error < GIT_SUCCESS)
		return error;

	data = (char*)(file.data);

	if (git__prefixcmp(data, GIT_FILE_CONTENT_PREFIX)) {
		git_futils_freebuffer(&file);
		return git__throw(GIT_ENOTFOUND, "Invalid gitfile format `%s`", file_path);
	}

	end_offset = strlen(data) - 1;

	for (;data[end_offset] == '\r' || data[end_offset] == '\n'; --end_offset);
	data[end_offset + 1] = '\0';

	if (strlen(GIT_FILE_CONTENT_PREFIX) == end_offset + 1) {
		git_futils_freebuffer(&file);
		return git__throw(GIT_ENOTFOUND, "No path in git file `%s`", file_path);
	}

	data = data + strlen(GIT_FILE_CONTENT_PREFIX);
	error = git_path_prettify_dir(&found_path, data, base_path);

	git_futils_freebuffer(&file);

	if (error == GIT_SUCCESS && git_futils_exists(found_path.data) == 0) {
		strncpy(path_out, found_path.data, GIT_PATH_MAX);
		git__path_free(&found_path);
		return GIT_SUCCESS;
	}

	git__path_free(&found_path);

	return git__throw(GIT_EOBJCORRUPTED, "The `.git` file points to a nonexistent path");
}

static void git_repository__free_dirs(git_repository *repo)
{
	git__free(repo->path_workdir);
	repo->path_workdir = NULL;
	git__free(repo->path_index);
	repo->path_index = NULL;
	git__free(repo->path_repository);
	repo->path_repository = NULL;
	git__free(repo->path_odb);
	repo->path_odb = NULL;
}

void git_repository_free(git_repository *repo)
{
	if (repo == NULL)
		return;

	git_cache_free(&repo->objects);
	git_repository__refcache_free(&repo->references);
	git_repository__free_dirs(repo);

	if (repo->db != NULL)
		git_odb_close(repo->db);

	git__free(repo);
}

int git_repository_discover(
	char *repository_path,
	size_t size,
	const char *start_path,
	int across_fs,
	const char *ceiling_dirs)
{
	int error, ceiling_offset;
	git_path bare_path = GIT_PATH_INIT;
	git_path normal_path = GIT_PATH_INIT;
	git_path *found_path = NULL;
	dev_t current_device = 0;

	assert(start_path && repository_path);

	*repository_path = '\0';

	error = git_path_prettify_dir(&bare_path, start_path, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (!across_fs) {
		error = retrieve_device(&current_device, bare_path.data);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	ceiling_offset = retrieve_ceiling_directories_offset(bare_path.data, ceiling_dirs);

	error = git_path_join(&normal_path, bare_path.data, DOT_GIT);
	if (error < GIT_SUCCESS)
		goto cleanup;

	while(1) {
		/**
		 * If the `.git` file is regular instead of
		 * a directory, it should contain the path of the actual git repository
		 */
		if (git_futils_isfile(normal_path.data) == GIT_SUCCESS) {
			error = read_gitfile(repository_path, normal_path.data, bare_path.data);

			if (error < GIT_SUCCESS) {
				git__rethrow(error, "Unable to read git file `%s`", normal_path.data);
			} else {
				error = quickcheck_repository_dir(repository_path);

				if (error < GIT_SUCCESS) {
					git__throw(GIT_ENOTFOUND, "The `.git` file found at '%s' points"
							   "to a nonexistent git folder", normal_path.data);
				}
			}

			goto cleanup;
		}

		/**
		 * If the `.git` file is a folder, we check inside of it
		 */
		if (git_futils_isdir(normal_path.data) == GIT_SUCCESS) {
			error = quickcheck_repository_dir(normal_path.data);
			if (error == GIT_SUCCESS) {
				found_path = &normal_path;
				break;
			}
		}

		/**
		 * Otherwise, the repository may be bare, let's check
		 * the root anyway
		 */
		error = quickcheck_repository_dir(bare_path.data);
		if (error == GIT_SUCCESS) {
			found_path = &bare_path;
			break;
		}

		error = git_path_dirname_r(&normal_path, bare_path.data);
		if (error < GIT_SUCCESS) {
			git__rethrow(error, "Failed to dirname '%s'", bare_path.data);
			goto cleanup;
		}

		if (!across_fs) {
			dev_t new_device;
			error = retrieve_device(&new_device, normal_path.data);

			if (error < GIT_SUCCESS || current_device != new_device) {
				error = git__throw(GIT_ENOTAREPO,"Not a git repository (or any parent up to mount parent %s)\n"
					"Stopping at filesystem boundary.", bare_path.data);
				goto cleanup;
			}
			current_device = new_device;
		}

		git__path_swap(&bare_path, &normal_path);
		error = git_path_join(&normal_path, bare_path.data, DOT_GIT);
		if (error < GIT_SUCCESS)
			goto cleanup;

		// nothing has been found, lets try the parent directory
		if (bare_path.data[ceiling_offset] == '\0') {
			error = git__throw(GIT_ENOTAREPO,"Not a git repository (or any of the parent directories): %s", start_path);
			goto cleanup;
		}
	}

	assert(found_path);

	if (git_path_as_dir(found_path) < GIT_SUCCESS) {
		git__throw(GIT_ENOMEM, "Could not convert git repository path to directory");
		goto cleanup;
	}

	if (size < strlen(found_path->data) + 1) {
		error = git__throw(GIT_ESHORTBUFFER, "The repository buffer is not long enough to handle the repository path `%s`", found_path->data);
	} else {
		strncpy(repository_path, found_path->data, size);
	}

cleanup:
	git__path_free(&bare_path);
	git__path_free(&normal_path);
	return error;
}

git_odb *git_repository_database(git_repository *repo)
{
	assert(repo);
	return repo->db;
}

static int repo_init_reinit(const char *repository_path, int is_bare)
{
	/* TODO: reinit the repository */
	return git__throw(GIT_ENOTIMPLEMENTED,
		"Failed to reinitialize the %srepository at '%s'. "
		"This feature is not yet implemented",
		is_bare ? "bare" : "", repository_path);
}

static int repo_init_createhead(git_repository *repo)
{
	int error;
	git_reference *head_reference;

	error = git_reference_create_symbolic(&head_reference, repo, GIT_HEAD_FILE, GIT_REFS_HEADS_MASTER_FILE, 0);

	git_reference_free(head_reference);

	return error;
}

static int repo_init_structure(const char *git_dir, int is_bare)
{
	int error = GIT_SUCCESS;
	git_path temp_path = GIT_PATH_INIT;

	if (git_futils_mkdir_r(git_dir, is_bare ? GIT_BARE_DIR_MODE : GIT_DIR_MODE))
		return git__throw(GIT_ERROR, "Failed to initialize repository structure. Could not mkdir");

	/* Hides the ".git" directory */
	if (!is_bare) {
#ifdef GIT_WIN32
		error = p_hide_directory__w32(git_dir);
		if (error < GIT_SUCCESS)
			goto rethrow_and_cleanup;
#endif
	}

	/* Creates the '/objects/info/' directory */
	error = git_path_join(&temp_path, git_dir, GIT_OBJECTS_INFO_DIR);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	error = git_futils_mkdir_r(temp_path.data, GIT_OBJECT_DIR_MODE);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	/* Creates the '/objects/pack/' directory */
	error = git_path_join(&temp_path, git_dir, GIT_OBJECTS_PACK_DIR);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	error = p_mkdir(temp_path.data, GIT_OBJECT_DIR_MODE);
	if (error < GIT_SUCCESS) {
		git__throw(error, "Unable to create `%s` folder", temp_path.data);
		goto cleanup;
	}

	/* Creates the '/refs/heads/' directory */
	error = git_path_join(&temp_path, git_dir, GIT_REFS_HEADS_DIR);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	error = git_futils_mkdir_r(temp_path.data, GIT_REFS_DIR_MODE);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	/* Creates the '/refs/tags/' directory */
	error = git_path_join(&temp_path, git_dir, GIT_REFS_TAGS_DIR);
	if (error < GIT_SUCCESS)
		goto rethrow_and_cleanup;

	error = p_mkdir(temp_path.data, GIT_REFS_DIR_MODE);
	if (error < GIT_SUCCESS) {
		git__throw(error, "Unable to create `%s` folder", temp_path.data);
		goto cleanup;
	}

	/* TODO: what's left? templates? */

rethrow_and_cleanup:
	if (error)
		git__rethrow(error, "Failed to initialize repository structure");

cleanup:
	git__path_free(&temp_path);
	return error;
}

int git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare)
{
	int error = GIT_SUCCESS;
	git_repository *repo = NULL;
	git_path repo_path = GIT_PATH_INIT;

	assert(repo_out && path);

	error = git_path_join(&repo_path, path, is_bare ? "" : GIT_DIR);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (git_futils_isdir(repo_path.data)) {
		if (quickcheck_repository_dir(repo_path.data) == GIT_SUCCESS) {
			error = repo_init_reinit(repo_path.data, is_bare);
			git__path_free(&repo_path);
			return error;
		}
	}

	error = repo_init_structure(repo_path.data, is_bare);
	if (error < GIT_SUCCESS)
		goto cleanup;

	repo = repository_alloc();
	if (repo == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = guess_repository_dirs(repo, repo_path.data);
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

	git__path_free(&repo_path);

	*repo_out = repo;

	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	git__path_free(&repo_path);
	return git__rethrow(error, "Failed to (re)init the repository `%s`", path);
}

int git_repository_head_detached(git_repository *repo)
{
	git_reference *ref;
	int error;
	size_t GIT_UNUSED(_size);
	git_otype type;

	error = git_reference_lookup(&ref, repo, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		return error;

	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		git_reference_free(ref);
		return 0;
	}

	error = git_odb_read_header(&_size, &type, repo->db, git_reference_oid(ref));

	git_reference_free(ref);

	if (error < GIT_SUCCESS)
		return error;

	if (type != GIT_OBJ_COMMIT)
		return git__throw(GIT_EOBJCORRUPTED, "HEAD is not a commit");

	return 1;
}

int git_repository_head(git_reference **head_out, git_repository *repo)
{
	git_reference *ref, *resolved_ref;
	int error;

	*head_out = NULL;

	error = git_reference_lookup(&ref, repo, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		return git__rethrow(GIT_ENOTAREPO, "Failed to locate the HEAD");

	error = git_reference_resolve(&resolved_ref, ref);
	if (error < GIT_SUCCESS) {
		git_reference_free(ref);
		return git__rethrow(error, "Failed to resolve the HEAD");
	}

	git_reference_free(ref);

	*head_out = resolved_ref;
	return GIT_SUCCESS;
}

int git_repository_head_orphan(git_repository *repo)
{
	git_reference *ref;
	int error;

	error = git_repository_head(&ref, repo);

	if (error == GIT_SUCCESS)
		git_reference_free(ref);

	return error == GIT_ENOTFOUND ? 1 : error;
}

int git_repository_is_empty(git_repository *repo)
{
	git_reference *head = NULL, *branch = NULL;
	int error;

	error = git_reference_lookup(&head, repo, "HEAD");
	if (error < GIT_SUCCESS)
		return git__throw(error, "Corrupted repository. HEAD does not exist");

	if (git_reference_type(head) != GIT_REF_SYMBOLIC) {
		git_reference_free(head);
		return 0;
	}

	if (strcmp(git_reference_target(head), "refs/heads/master") != 0) {
		git_reference_free(head);
		return 0;
	}

	error = git_reference_resolve(&branch, head);

	git_reference_free(head);
	git_reference_free(branch);

	return error == GIT_ENOTFOUND ? 1 : error;
}

const char *git_repository_path(git_repository *repo, git_repository_pathid id)
{
	assert(repo);

	switch (id) {
	case GIT_REPO_PATH:
		return repo->path_repository;

	case GIT_REPO_PATH_INDEX:
		return repo->path_index;

	case GIT_REPO_PATH_ODB:
		return repo->path_odb;

	case GIT_REPO_PATH_WORKDIR:
		return repo->path_workdir;

	default:
		return NULL;
	}
}

int git_repository_is_bare(git_repository *repo)
{
	assert(repo);
	return repo->is_bare;
}
