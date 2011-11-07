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
	char path_aux[GIT_PATH_MAX];
	int error = GIT_SUCCESS;

	assert(repo);

	if (git_dir == NULL)
		return git__throw(GIT_ENOTFOUND, "Failed to open repository. Git dir not found");

	error = git_path_prettify_dir(path_aux, git_dir, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to open repository");

	/* store GIT_DIR */
	repo->path_repository = git__strdup(path_aux);
	if (repo->path_repository == NULL)
		return GIT_ENOMEM;

	/* path to GIT_OBJECT_DIRECTORY */
	if (git_object_directory == NULL)
		git_path_join(path_aux, repo->path_repository, GIT_OBJECTS_DIR);
	else {
		error = git_path_prettify_dir(path_aux, git_object_directory, NULL);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to open repository");
	}

	/* Store GIT_OBJECT_DIRECTORY */
	repo->path_odb = git__strdup(path_aux);
	if (repo->path_odb == NULL)
		return GIT_ENOMEM;

	/* path to GIT_WORK_TREE */
	if (git_work_tree == NULL)
		repo->is_bare = 1;
	else {
		error = git_path_prettify_dir(path_aux, git_work_tree, NULL);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to open repository");

		/* Store GIT_WORK_TREE */
		repo->path_workdir = git__strdup(path_aux);
		if (repo->path_workdir == NULL)
			return GIT_ENOMEM;

		/* Path to GIT_INDEX_FILE */
		if (git_index_file == NULL)
			git_path_join(path_aux, repo->path_repository, GIT_INDEX_FILE);
		else {
			error = git_path_prettify(path_aux, git_index_file, NULL);
			if (error < GIT_SUCCESS)
				return git__rethrow(error, "Failed to open repository");
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

	if (git_futils_isdir(repo->path_repository) < GIT_SUCCESS)
		return git__throw(GIT_ENOTAREPO, "`%s` is not a folder", repo->path_repository);

	/* Ensure GIT_OBJECT_DIRECTORY exists */
	if (git_futils_isdir(repo->path_odb) < GIT_SUCCESS)
		return git__throw(GIT_ENOTAREPO, "`%s` does not exist", repo->path_odb);

	/* Ensure HEAD file exists */
	git_path_join(path_aux, repo->path_repository, GIT_HEAD_FILE);
	if (git_futils_isfile(path_aux) < 0)
		return git__throw(GIT_ENOTAREPO, "HEAD file is missing");

	return GIT_SUCCESS;
}

static int guess_repository_dirs(git_repository *repo, const char *repository_path)
{
	char buffer[GIT_PATH_MAX];
	const char *path_work_tree = NULL;

	/* Git directory name */
	if (git_path_basename_r(buffer, sizeof(buffer), repository_path) < 0)
		return git__throw(GIT_EINVALIDPATH, "Unable to parse folder name from `%s`", repository_path);

	if (strcmp(buffer, DOT_GIT) == 0) {
		/* Path to working dir */
		if (git_path_dirname_r(buffer, sizeof(buffer), repository_path) < 0)
			return git__throw(GIT_EINVALIDPATH, "Unable to parse parent folder name from `%s`", repository_path);
		path_work_tree = buffer;
	}

	return assign_repository_dirs(repo, repository_path, NULL, NULL, path_work_tree);
}

static int quickcheck_repository_dir(const char *repository_path)
{
	char path_aux[GIT_PATH_MAX];

	/* Ensure HEAD file exists */
	git_path_join(path_aux, repository_path, GIT_HEAD_FILE);
	if (git_futils_isfile(path_aux) < 0)
		return GIT_ERROR;

	git_path_join(path_aux, repository_path, GIT_OBJECTS_DIR);
	if (git_futils_isdir(path_aux) < 0)
		return GIT_ERROR;

	git_path_join(path_aux, repository_path, GIT_REFS_DIR);
	if (git_futils_isdir(path_aux) < 0)
		return GIT_ERROR;

	return GIT_SUCCESS;
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
	char config_path[GIT_PATH_MAX];
	int error;

	assert(out && repo);

	error = git_config_new(out);
	if (error < GIT_SUCCESS)
		return error;

	git_path_join(config_path, repo->path_repository, GIT_CONFIG_FILENAME_INREPO);
	error = git_config_add_file_ondisk(*out, config_path, 3);
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
	error = git_path_prettify_dir(path_out, data, base_path);
	git_futils_freebuffer(&file);

	if (error == 0 && git_futils_exists(path_out) == 0)
		return GIT_SUCCESS;

	return git__throw(GIT_EOBJCORRUPTED, "The `.git` file points to an inexisting path");
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

int git_repository_discover(char *repository_path, size_t size, const char *start_path, int across_fs, const char *ceiling_dirs)
{
	int error, ceiling_offset;
	char bare_path[GIT_PATH_MAX];
	char normal_path[GIT_PATH_MAX];
	char *found_path;
	dev_t current_device = 0;

	assert(start_path && repository_path);

	error = git_path_prettify_dir(bare_path, start_path, NULL);
	if (error < GIT_SUCCESS)
		return error;

	if (!across_fs) {
		error = retrieve_device(&current_device, bare_path);
		if (error < GIT_SUCCESS)
			return error;
	}

	ceiling_offset = retrieve_ceiling_directories_offset(bare_path, ceiling_dirs);
	git_path_join(normal_path, bare_path, DOT_GIT);

	while(1) {
		/**
		 * If the `.git` file is regular instead of
		 * a directory, it should contain the path of the actual git repository
		 */
		if (git_futils_isfile(normal_path) == GIT_SUCCESS) {
			error = read_gitfile(repository_path, normal_path, bare_path);

			if (error < GIT_SUCCESS)
				return git__rethrow(error, "Unable to read git file `%s`", normal_path);

			error = quickcheck_repository_dir(repository_path);
			if (error < GIT_SUCCESS)
				return git__throw(GIT_ENOTFOUND, "The `.git` file found at '%s' points"
					"to an inexisting Git folder", normal_path);

			return GIT_SUCCESS;
		}

		/**
		 * If the `.git` file is a folder, we check inside of it
		 */
		if (git_futils_isdir(normal_path) == GIT_SUCCESS) {
			error = quickcheck_repository_dir(normal_path);
			if (error == GIT_SUCCESS) {
				found_path = normal_path;
				break;
			}
		}

		/**
		 * Otherwise, the repository may be bare, let's check
		 * the root anyway
		 */
		error = quickcheck_repository_dir(bare_path);
		if (error == GIT_SUCCESS) {
			found_path = bare_path;
			break;
		}

		if (git_path_dirname_r(normal_path, sizeof(normal_path), bare_path) < GIT_SUCCESS)
			return git__throw(GIT_EOSERR, "Failed to dirname '%s'", bare_path);

		if (!across_fs) {
			dev_t new_device;
			error = retrieve_device(&new_device, normal_path);

			if (error < GIT_SUCCESS || current_device != new_device) {
				return git__throw(GIT_ENOTAREPO,"Not a git repository (or any parent up to mount parent %s)\n"
					"Stopping at filesystem boundary.", bare_path);
			}
			current_device = new_device;
		}

		strcpy(bare_path, normal_path);
		git_path_join(normal_path, bare_path, DOT_GIT);

		// nothing has been found, lets try the parent directory
		if (bare_path[ceiling_offset] == '\0') {
			return git__throw(GIT_ENOTAREPO,"Not a git repository (or any of the parent directories): %s", start_path);
		}
	}

	if (size < strlen(found_path) + 2) {
		return git__throw(GIT_ESHORTBUFFER, "The repository buffer is not long enough to handle the repository path `%s`", found_path);
	}

	git_path_join(repository_path, found_path, "");
	return GIT_SUCCESS;
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
	int error;

	char temp_path[GIT_PATH_MAX];

	if (git_futils_mkdir_r(git_dir, is_bare ? GIT_BARE_DIR_MODE : GIT_DIR_MODE))
		return git__throw(GIT_ERROR, "Failed to initialize repository structure. Could not mkdir");

	/* Hides the ".git" directory */
	if (!is_bare) {
#ifdef GIT_WIN32
		error = p_hide_directory__w32(git_dir);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to initialize repository structure");
#endif
	}

	/* Creates the '/objects/info/' directory */
	git_path_join(temp_path, git_dir, GIT_OBJECTS_INFO_DIR);
	error = git_futils_mkdir_r(temp_path, GIT_OBJECT_DIR_MODE);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to initialize repository structure");

	/* Creates the '/objects/pack/' directory */
	git_path_join(temp_path, git_dir, GIT_OBJECTS_PACK_DIR);
	error = p_mkdir(temp_path, GIT_OBJECT_DIR_MODE);
	if (error < GIT_SUCCESS)
		return git__throw(error, "Unable to create `%s` folder", temp_path);

	/* Creates the '/refs/heads/' directory */
	git_path_join(temp_path, git_dir, GIT_REFS_HEADS_DIR);
	error = git_futils_mkdir_r(temp_path, GIT_REFS_DIR_MODE);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to initialize repository structure");

	/* Creates the '/refs/tags/' directory */
	git_path_join(temp_path, git_dir, GIT_REFS_TAGS_DIR);
	error = p_mkdir(temp_path, GIT_REFS_DIR_MODE);
	if (error < GIT_SUCCESS)
		return git__throw(error, "Unable to create `%s` folder", temp_path);

	/* TODO: what's left? templates? */

	return GIT_SUCCESS;
}

int git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare)
{
	int error = GIT_SUCCESS;
	git_repository *repo = NULL;
	char repository_path[GIT_PATH_MAX];

	assert(repo_out && path);

	git_path_join(repository_path, path, is_bare ? "" : GIT_DIR);

	if (git_futils_isdir(repository_path)) {
		if (quickcheck_repository_dir(repository_path) == GIT_SUCCESS)
			return repo_init_reinit(repository_path, is_bare);
	}

	error = repo_init_structure(repository_path, is_bare);
	if (error < GIT_SUCCESS)
		goto cleanup;

	repo = repository_alloc();
	if (repo == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = guess_repository_dirs(repo, repository_path);
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

	*repo_out = repo;
	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
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
