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

#define GIT_CONFIG_CORE_REPOSITORYFORMATVERSION "core.repositoryformatversion"
#define GIT_REPOSITORYFORMATVERSION 0

static void drop_odb(git_repository *repo)
{
	if (repo->_odb != NULL) {
		GIT_REFCOUNT_OWN(repo->_odb, NULL);
		git_odb_free(repo->_odb);
		repo->_odb = NULL;
	}
}

static void drop_config(git_repository *repo)
{
	if (repo->_config != NULL) {
		GIT_REFCOUNT_OWN(repo->_config, NULL);
		git_config_free(repo->_config);
		repo->_config = NULL;
	}
}

static void drop_index(git_repository *repo)
{
	if (repo->_index != NULL) {
		GIT_REFCOUNT_OWN(repo->_index, NULL);
		git_index_free(repo->_index);
		repo->_index = NULL;
	}
}

void git_repository_free(git_repository *repo)
{
	if (repo == NULL)
		return;

	git_cache_free(&repo->objects);
	git_repository__refcache_free(&repo->references);
	git_attr_cache_flush(repo);

	git__free(repo->path_repository);
	git__free(repo->workdir);

	drop_config(repo);
	drop_index(repo);
	drop_odb(repo);

	git__free(repo);
}

/*
 * Git repository open methods
 *
 * Open a repository object from its path
 */
static int quickcheck_repository_dir(git_buf *repository_path)
{
	/* Check OBJECTS_DIR first, since it will generate the longest path name */
	if (git_path_contains_dir(repository_path, GIT_OBJECTS_DIR, 0) < 0)
		return GIT_ERROR;

	/* Ensure HEAD file exists */
	if (git_path_contains_file(repository_path, GIT_HEAD_FILE, 0) < 0)
		return GIT_ERROR;

	if (git_path_contains_dir(repository_path, GIT_REFS_DIR, 0) < 0)
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

static int load_config_data(git_repository *repo)
{
	int error, is_bare;
	git_config *config;

	error = git_repository_config__weakptr(&config, repo);
	if (error < GIT_SUCCESS)
		return error;

	error = git_config_get_bool(config, "core.bare", &is_bare);
	if (error == GIT_SUCCESS)
		repo->is_bare = is_bare;

	/* TODO: what else can we load/cache here? */

	return GIT_SUCCESS;
}

static int load_workdir(git_repository *repo)
{
	int error;
	git_buf workdir_buf = GIT_BUF_INIT;

	if (repo->is_bare)
		return GIT_SUCCESS;

	git_path_dirname_r(&workdir_buf, repo->path_repository);
	git_path_to_dir(&workdir_buf);

	if ((error = git_buf_lasterror(&workdir_buf)) == GIT_SUCCESS)
		repo->workdir = git_buf_detach(&workdir_buf);

	git_buf_free(&workdir_buf);

	return error;
}

int git_repository_open(git_repository **repo_out, const char *path)
{
	int error = GIT_SUCCESS;
	git_buf path_buf = GIT_BUF_INIT;
	git_repository *repo = NULL;

	error = git_path_prettify_dir(&path_buf, path, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/**
	 * Check if the path we've been given is actually the path
	 * of the working dir, by testing if it contains a `.git`
	 * folder inside of it.
	 */
	git_path_contains_dir(&path_buf, GIT_DIR, 1); /* append on success */
	/* ignore error, since it just means `path/.git` doesn't exist */

	if (quickcheck_repository_dir(&path_buf) < GIT_SUCCESS) {
		error = git__throw(GIT_ENOTAREPO,
			"The given path (%s) is not a valid Git repository", git_buf_cstr(&path_buf));
		goto cleanup;
	}

	repo = repository_alloc();
	if (repo == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	repo->path_repository = git_buf_detach(&path_buf);
	if (repo->path_repository == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = load_config_data(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = load_workdir(repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*repo_out = repo;
	return GIT_SUCCESS;

 cleanup:
	git_repository_free(repo);
	git_buf_free(&path_buf);
	return error;
}

static int load_config(
		git_config **out,
		git_repository *repo,
		const char *global_config_path,
		const char *system_config_path)
{
	git_buf config_path = GIT_BUF_INIT;
	int error;
	git_config *cfg = NULL;

	assert(repo && out);

	error = git_config_new(&cfg);
	if (error < GIT_SUCCESS)
		return error;

	error = git_buf_joinpath(&config_path, repo->path_repository,
							 GIT_CONFIG_FILENAME_INREPO);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_config_add_file_ondisk(cfg, config_path.ptr, 3);
	git_buf_free(&config_path); /* done with config_path now */
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (global_config_path != NULL) {
		error = git_config_add_file_ondisk(cfg, global_config_path, 2);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	if (system_config_path != NULL) {
		error = git_config_add_file_ondisk(cfg, system_config_path, 1);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	*out = cfg;
	return GIT_SUCCESS;

cleanup:
	git_config_free(cfg);
	*out = NULL;
	return error;
}

int git_repository_config__weakptr(git_config **out, git_repository *repo)
{
	if (repo->_config == NULL) {
		int error;
		git_buf global_buf = GIT_BUF_INIT, system_buf = GIT_BUF_INIT;

		const char *global_config_path = NULL;
		const char *system_config_path = NULL;

		if (git_config_find_global_r(&global_buf) == GIT_SUCCESS)
			global_config_path = global_buf.ptr;

		if (git_config_find_system_r(&system_buf) == GIT_SUCCESS)
			system_config_path = system_buf.ptr;

		error = load_config(&repo->_config, repo, global_config_path, system_config_path);

		git_buf_free(&global_buf);
		git_buf_free(&system_buf);

		if (error < GIT_SUCCESS)
			return error;

		GIT_REFCOUNT_OWN(repo->_config, repo);
	}

	*out = repo->_config;
	return GIT_SUCCESS;
}

int git_repository_config(git_config **out, git_repository *repo)
{
	int error = git_repository_config__weakptr(out, repo);

	if (error == GIT_SUCCESS) {
		GIT_REFCOUNT_INC(*out);
	}

	return error;
}

void git_repository_set_config(git_repository *repo, git_config *config)
{
	assert(repo && config);

	drop_config(repo);

	repo->_config = config;
	GIT_REFCOUNT_OWN(repo->_config, repo);
}

int git_repository_odb__weakptr(git_odb **out, git_repository *repo)
{
	assert(repo && out);

	if (repo->_odb == NULL) {
		int error;
		git_buf odb_path = GIT_BUF_INIT;

		error = git_buf_joinpath(&odb_path, repo->path_repository, GIT_OBJECTS_DIR);
		if (error < GIT_SUCCESS)
			return error;

		error = git_odb_open(&repo->_odb, odb_path.ptr);
		git_buf_free(&odb_path); /* done with path */
		if (error < GIT_SUCCESS)
			return error;

		GIT_REFCOUNT_OWN(repo->_odb, repo);
	}

	*out = repo->_odb;
	return GIT_SUCCESS;
}

int git_repository_odb(git_odb **out, git_repository *repo)
{
	int error = git_repository_odb__weakptr(out, repo);

	if (error == GIT_SUCCESS) {
		GIT_REFCOUNT_INC(*out);
	}

	return error;
}

void git_repository_set_odb(git_repository *repo, git_odb *odb)
{
	assert(repo && odb);

	drop_odb(repo);

	repo->_odb = odb;
	GIT_REFCOUNT_OWN(repo->_odb, repo);
}

int git_repository_index__weakptr(git_index **out, git_repository *repo)
{
	assert(out && repo);

	if (repo->_index == NULL) {
		int error;
		git_buf index_path = GIT_BUF_INIT;

		error = git_buf_joinpath(&index_path, repo->path_repository, GIT_INDEX_FILE);
		if (error < GIT_SUCCESS)
			return error;

		error = git_index_open(&repo->_index, index_path.ptr);
		git_buf_free(&index_path); /* done with path */
		if (error < GIT_SUCCESS)
			return error;

		GIT_REFCOUNT_OWN(repo->_index, repo);
	}

	*out = repo->_index;
	return GIT_SUCCESS;
}

int git_repository_index(git_index **out, git_repository *repo)
{
	int error = git_repository_index__weakptr(out, repo);

	if (error == GIT_SUCCESS) {
		GIT_REFCOUNT_INC(*out);
	}

	return error;
}

void git_repository_set_index(git_repository *repo, git_index *index)
{
	assert(repo && index);

	drop_index(repo);

	repo->_index = index;
	GIT_REFCOUNT_OWN(repo->_index, repo);
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

/*
 * This function returns furthest offset into path where a ceiling dir
 * is found, so we can stop processing the path at that point.
 *
 * Note: converting this to use git_bufs instead of GIT_PATH_MAX buffers on
 * the stack could remove directories name limits, but at the cost of doing
 * repeated malloc/frees inside the loop below, so let's not do it now.
 */
static int retrieve_ceiling_directories_offset(
	const char *path,
	const char *ceiling_directories)
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

		if (len == 0 || len >= (int)sizeof(buf) || git_path_root(ceil) == -1)
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

/*
 * Read the contents of `file_path` and set `path_out` to the repo dir that
 * it points to.  Before calling, set `path_out` to the base directory that
 * should be used if the contents of `file_path` are a relative path.
 */
static int read_gitfile(git_buf *path_out, const char *file_path, const char *base_path)
{
	git_fbuffer file;
	int error;

	assert(path_out && file_path);

	error = git_futils_readbuffer(&file, file_path);
	if (error < GIT_SUCCESS)
		return error;

	if (git__prefixcmp((char *)file.data, GIT_FILE_CONTENT_PREFIX)) {
		git_futils_freebuffer(&file);
		return git__throw(GIT_ENOTFOUND, "Invalid gitfile format `%s`", file_path);
	}

	git_futils_fbuffer_rtrim(&file);

	if (strlen(GIT_FILE_CONTENT_PREFIX) == file.len) {
		git_futils_freebuffer(&file);
		return git__throw(GIT_ENOTFOUND, "No path in git file `%s`", file_path);
	}

	error = git_path_prettify_dir(path_out,
		((char *)file.data) + strlen(GIT_FILE_CONTENT_PREFIX), base_path);

	git_futils_freebuffer(&file);

	if (error == GIT_SUCCESS && git_path_exists(path_out->ptr) == 0)
		return GIT_SUCCESS;

	return git__throw(GIT_EOBJCORRUPTED, "The `.git` file points to a nonexistent path");
}

int git_repository_discover(
	char *repository_path,
	size_t size,
	const char *start_path,
	int across_fs,
	const char *ceiling_dirs)
{
	int error, ceiling_offset;
	git_buf bare_path = GIT_BUF_INIT;
	git_buf normal_path = GIT_BUF_INIT;
	git_buf *found_path = NULL;
	dev_t current_device = 0;

	assert(start_path && repository_path);

	*repository_path = '\0';

	error = git_path_prettify_dir(&bare_path, start_path, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (!across_fs) {
		error = retrieve_device(&current_device, bare_path.ptr);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	ceiling_offset = retrieve_ceiling_directories_offset(bare_path.ptr, ceiling_dirs);

	while(1) {
		error = git_buf_joinpath(&normal_path, bare_path.ptr, DOT_GIT);
		if (error < GIT_SUCCESS)
			break;

		/**
		 * If the `.git` file is regular instead of
		 * a directory, it should contain the path of the actual git repository
		 */
		if (git_path_isfile(normal_path.ptr) == GIT_SUCCESS) {
			git_buf gitfile_path = GIT_BUF_INIT;

			error = read_gitfile(&gitfile_path, normal_path.ptr, bare_path.ptr);
			if (error < GIT_SUCCESS)
				git__rethrow(error, "Unable to read git file `%s`", normal_path.ptr);
			else if ((error = quickcheck_repository_dir(&gitfile_path)) < GIT_SUCCESS)
				git__throw(GIT_ENOTFOUND,
					"The `.git` file found at '%s' points "
					"to a nonexistent git folder", normal_path.ptr);
			else {
				git_buf_swap(&normal_path, &gitfile_path);
				found_path = &normal_path;
			}

			git_buf_free(&gitfile_path);
			break;
		}

		/**
		 * If the `.git` file is a folder, we check inside of it
		 */
		if (git_path_isdir(normal_path.ptr) == GIT_SUCCESS) {
			error = quickcheck_repository_dir(&normal_path);
			if (error == GIT_SUCCESS) {
				found_path = &normal_path;
				break;
			}
		}

		/**
		 * Otherwise, the repository may be bare, let's check
		 * the root anyway
		 */
		error = quickcheck_repository_dir(&bare_path);
		if (error == GIT_SUCCESS) {
			found_path = &bare_path;
			break;
		}

		/**
		 * If we didn't find it, walk up the tree
		 */
		error = git_path_dirname_r(&normal_path, bare_path.ptr);
		if (error < GIT_SUCCESS) {
			git__rethrow(GIT_EOSERR, "Failed to dirname '%s'", bare_path.ptr);
			break;
		}

		git_buf_swap(&bare_path, &normal_path);

		if (!across_fs) {
			dev_t new_device;
			error = retrieve_device(&new_device, bare_path.ptr);

			if (error < GIT_SUCCESS || current_device != new_device) {
				error = git__throw(GIT_ENOTAREPO,
					"Not a git repository (or any parent up to mount parent %s)\n"
					"Stopping at filesystem boundary.", normal_path.ptr);
				break;
			}
			current_device = new_device;
		}

		/* nothing has been found, lets try the parent directory
		 * but stop if we hit one of the ceiling directories
		 */
		if (bare_path.ptr[ceiling_offset] == '\0') {
			error = git__throw(GIT_ENOTAREPO,
				"Not a git repository (or any of the parent directories): %s", start_path);
			break;
		}
	}

	assert(found_path || error != GIT_SUCCESS);

	if (found_path) {
		if ((error = git_path_to_dir(found_path)) < GIT_SUCCESS)
			git__rethrow(error, "Could not convert git repository to directory");
		else if (size < (size_t)(found_path->size + 1))
			error = git__throw(GIT_ESHORTBUFFER,
				"The repository buffer is not long enough to "
				"handle the repository path `%s`", found_path->ptr);
		else
			git_buf_copy_cstr(repository_path, size, found_path);
	}

cleanup:
	git_buf_free(&bare_path);
	git_buf_free(&normal_path);
	return error;
}

static int check_repositoryformatversion(git_repository *repo)
{
	git_config *config;
	int version, error = GIT_SUCCESS;

	if ((error = git_repository_config(&config, repo)) < GIT_SUCCESS)
		return git__throw(error, "Failed to open config file.");

	error = git_config_get_int32(config, GIT_CONFIG_CORE_REPOSITORYFORMATVERSION, &version);

	if (GIT_REPOSITORYFORMATVERSION < version)
		error = git__throw(GIT_ERROR, "Unsupported git repository version (Expected version <= %d, found %d).", GIT_REPOSITORYFORMATVERSION, version);

	git_config_free(config);

	return error;
}

static int repo_init_reinit(git_repository **repo_out, const char *repository_path, int is_bare)
{
	int error;
	git_repository *repo = NULL;

	if ((error = git_repository_open(&repo, repository_path)) < GIT_SUCCESS)
		goto error;

	if ((error = check_repositoryformatversion(repo)) < GIT_SUCCESS)
		goto error;

	/* TODO: reinitialize the templates */

	*repo_out = repo;

	return GIT_SUCCESS;

error:
	git_repository_free(repo);

	return git__rethrow(error,
		"Failed to reinitialize the %srepository at '%s'. ",
		is_bare ? "bare " : "", repository_path);
}

static int repo_init_createhead(const char *git_dir)
{
	int error;
	git_buf ref_path = GIT_BUF_INIT;
	git_filebuf ref = GIT_FILEBUF_INIT;

	if (!(error = git_buf_joinpath(&ref_path, git_dir, GIT_HEAD_FILE)) &&
		!(error = git_filebuf_open(&ref, ref_path.ptr, 0)) &&
		!(error = git_filebuf_printf(&ref, "ref: refs/heads/master\n")))
		error = git_filebuf_commit(&ref, GIT_REFS_FILE_MODE);

	git_buf_free(&ref_path);
	return error;
}

static int repo_init_config(const char *git_dir, int is_bare)
{
	git_buf cfg_path = GIT_BUF_INIT;
	git_config *config = NULL;
	int error = GIT_SUCCESS;

#define SET_REPO_CONFIG(type, name, val) {\
	error = git_config_set_##type(config, name, val);\
	if (error < GIT_SUCCESS)\
		goto cleanup;\
}

	error = git_buf_joinpath(&cfg_path, git_dir, GIT_CONFIG_FILENAME_INREPO);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_config_open_ondisk(&config, cfg_path.ptr);
	if (error < GIT_SUCCESS)
		goto cleanup;

	SET_REPO_CONFIG(bool, "core.bare", is_bare);
	SET_REPO_CONFIG(int32, GIT_CONFIG_CORE_REPOSITORYFORMATVERSION, GIT_REPOSITORYFORMATVERSION);
	/* TODO: what other defaults? */

cleanup:
	git_buf_free(&cfg_path);
	git_config_free(config);
	return error;
}

static int repo_init_structure(const char *git_dir, int is_bare)
{
	int error, i;
	struct { const char *dir; mode_t mode; } dirs[] = {
		{ GIT_OBJECTS_INFO_DIR, GIT_OBJECT_DIR_MODE }, /* '/objects/info/' */
		{ GIT_OBJECTS_PACK_DIR, GIT_OBJECT_DIR_MODE }, /* '/objects/pack/' */
		{ GIT_REFS_HEADS_DIR, GIT_REFS_DIR_MODE },     /* '/refs/heads/' */
		{ GIT_REFS_TAGS_DIR, GIT_REFS_DIR_MODE },      /* '/refs/tags/' */
		{ NULL, 0 }
	};

	/* Make the base directory */
	error = git_futils_mkdir_r(git_dir, NULL, is_bare ?
							   GIT_BARE_DIR_MODE : GIT_DIR_MODE);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to initialize repository structure. Could not mkdir");

	/* Hides the ".git" directory */
	if (!is_bare) {
#ifdef GIT_WIN32
		error = p_hide_directory__w32(git_dir);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to initialize repository structure");
#endif
	}

	/* Make subdirectories as needed */
	for (i = 0; dirs[i].dir != NULL; ++i) {
		error = git_futils_mkdir_r(dirs[i].dir, git_dir, dirs[i].mode);
		if (error < GIT_SUCCESS)
			return git__rethrow(error,
				"Failed to create repository folder `%s`", dirs[i].dir);
	}

	/* TODO: what's left? templates? */

	return error;
}

int git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare)
{
	int error = GIT_SUCCESS;
	git_repository *repo = NULL;
	git_buf repository_path = GIT_BUF_INIT;

	assert(repo_out && path);

	error = git_buf_joinpath(&repository_path, path, is_bare ? "" : GIT_DIR);
	if (error < GIT_SUCCESS)
		return error;

	if (git_path_isdir(repository_path.ptr) == GIT_SUCCESS) {
		if (quickcheck_repository_dir(&repository_path) == GIT_SUCCESS) {
			error = repo_init_reinit(repo_out, repository_path.ptr, is_bare);
			git_buf_free(&repository_path);
			return error;
		}
	}

	if (!(error = repo_init_structure(repository_path.ptr, is_bare)) &&
		!(error = repo_init_config(repository_path.ptr, is_bare)) &&
		!(error = repo_init_createhead(repository_path.ptr)))
		error = git_repository_open(repo_out, repository_path.ptr);
	else
		git_repository_free(repo);

	git_buf_free(&repository_path);

	if (error != GIT_SUCCESS)
		git__rethrow(error, "Failed to (re)init the repository `%s`", path);

	return error;
}

int git_repository_head_detached(git_repository *repo)
{
	git_reference *ref;
	int error;
	size_t _size;
	git_otype type;
	git_odb *odb = NULL;

	error = git_repository_odb__weakptr(&odb, repo);
	if (error < GIT_SUCCESS)
		return error;

	error = git_reference_lookup(&ref, repo, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		return error;

	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		git_reference_free(ref);
		return 0;
	}

	error = git_odb_read_header(&_size, &type, odb, git_reference_oid(ref));

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

const char *git_repository_path(git_repository *repo)
{
	assert(repo);
	return repo->path_repository;
}

const char *git_repository_workdir(git_repository *repo)
{
	assert(repo);

	if (repo->is_bare)
		return NULL;

	return repo->workdir;
}

int git_repository_set_workdir(git_repository *repo, const char *workdir)
{
	assert(repo && workdir);

	free(repo->workdir);

	repo->workdir = git__strdup(workdir);
	if (repo->workdir == NULL)
		return GIT_ENOMEM;

	repo->is_bare = 0;
	return GIT_SUCCESS;
}

int git_repository_is_bare(git_repository *repo)
{
	assert(repo);
	return repo->is_bare;
}
