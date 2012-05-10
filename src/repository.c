/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include <stdarg.h>
#include <ctype.h>

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

#define GIT_FILE_CONTENT_PREFIX "gitdir:"

#define GIT_BRANCH_MASTER "master"

#define GIT_REPO_VERSION 0

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

	git_repository__cvar_cache_clear(repo);
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
	git_submodule_config_free(repo);

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
static bool valid_repository_path(git_buf *repository_path)
{
	/* Check OBJECTS_DIR first, since it will generate the longest path name */
	if (git_path_contains_dir(repository_path, GIT_OBJECTS_DIR) == false)
		return false;

	/* Ensure HEAD file exists */
	if (git_path_contains_file(repository_path, GIT_HEAD_FILE) == false)
		return false;

	if (git_path_contains_dir(repository_path, GIT_REFS_DIR)  == false)
		return false;

	return true;
}

static git_repository *repository_alloc(void)
{
	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	if (git_cache_init(&repo->objects, GIT_DEFAULT_CACHE_SIZE, &git_object__free) < 0) {
		git__free(repo);
		return NULL;
	}

	/* set all the entries in the cvar cache to `unset` */
	git_repository__cvar_cache_clear(repo);

	return repo;
}

static int load_config_data(git_repository *repo)
{
	int is_bare;
	git_config *config;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	if (git_config_get_bool(&is_bare, config, "core.bare") < 0)
		return -1; /* FIXME: We assume that a missing core.bare
					  variable is an error. Is this right? */

	repo->is_bare = is_bare;
	return 0;
}

static int load_workdir(git_repository *repo, git_buf *parent_path)
{
	int         error;
	git_config *config;
	const char *worktree;
	git_buf     worktree_buf = GIT_BUF_INIT;

	if (repo->is_bare)
		return 0;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	error = git_config_get_string(&worktree, config, "core.worktree");
	if (!error && worktree != NULL)
		repo->workdir = git__strdup(worktree);
	else if (error != GIT_ENOTFOUND)
		return error;
	else {
		giterr_clear();

		if (parent_path && git_path_isdir(parent_path->ptr))
			repo->workdir = git_buf_detach(parent_path);
		else {
			git_path_dirname_r(&worktree_buf, repo->path_repository);
			git_path_to_dir(&worktree_buf);
			repo->workdir = git_buf_detach(&worktree_buf);
		}
	}

	GITERR_CHECK_ALLOC(repo->workdir);

	return 0;
}

/*
 * This function returns furthest offset into path where a ceiling dir
 * is found, so we can stop processing the path at that point.
 *
 * Note: converting this to use git_bufs instead of GIT_PATH_MAX buffers on
 * the stack could remove directories name limits, but at the cost of doing
 * repeated malloc/frees inside the loop below, so let's not do it now.
 */
static int find_ceiling_dir_offset(
	const char *path,
	const char *ceiling_directories)
{
	char buf[GIT_PATH_MAX + 1];
	char buf2[GIT_PATH_MAX + 1];
	const char *ceil, *sep;
	size_t len, max_len = 0, min_len;

	assert(path);

	min_len = (size_t)(git_path_root(path) + 1);

	if (ceiling_directories == NULL || min_len == 0)
		return (int)min_len;

	for (sep = ceil = ceiling_directories; *sep; ceil = sep + 1) {
		for (sep = ceil; *sep && *sep != GIT_PATH_LIST_SEPARATOR; sep++);
		len = sep - ceil;

		if (len == 0 || len >= sizeof(buf) || git_path_root(ceil) == -1)
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

	return (int)(max_len <= min_len ? min_len : max_len);
}

/*
 * Read the contents of `file_path` and set `path_out` to the repo dir that
 * it points to.  Before calling, set `path_out` to the base directory that
 * should be used if the contents of `file_path` are a relative path.
 */
static int read_gitfile(git_buf *path_out, const char *file_path)
{
	int     error = 0;
	git_buf file = GIT_BUF_INIT;
	size_t  prefix_len = strlen(GIT_FILE_CONTENT_PREFIX);

	assert(path_out && file_path);

	if (git_futils_readbuffer(&file, file_path) < 0)
		return -1;

	git_buf_rtrim(&file);

	if (file.size <= prefix_len ||
		memcmp(file.ptr, GIT_FILE_CONTENT_PREFIX, prefix_len) != 0)
	{
		giterr_set(GITERR_REPOSITORY, "The `.git` file at '%s' is malformed", file_path);
		error = -1;
	}
	else if ((error = git_path_dirname_r(path_out, file_path)) >= 0) {
		const char *gitlink = ((const char *)file.ptr) + prefix_len;
		while (*gitlink && git__isspace(*gitlink)) gitlink++;
		error = git_path_prettify_dir(path_out, gitlink, path_out->ptr);
	}

	git_buf_free(&file);
	return error;
}

static int find_repo(
	git_buf *repo_path,
	git_buf *parent_path,
	const char *start_path,
	uint32_t flags,
	const char *ceiling_dirs)
{
	int error;
	git_buf path = GIT_BUF_INIT;
	struct stat st;
	dev_t initial_device = 0;
	bool try_with_dot_git = false;
	int ceiling_offset;

	git_buf_free(repo_path);

	if ((error = git_path_prettify_dir(&path, start_path, NULL)) < 0)
		return error;

	ceiling_offset = find_ceiling_dir_offset(path.ptr, ceiling_dirs);

	if ((error = git_buf_joinpath(&path, path.ptr, DOT_GIT)) < 0)
		return error;

	while (!error && !git_buf_len(repo_path)) {
		if (p_stat(path.ptr, &st) == 0) {
			/* check that we have not crossed device boundaries */
			if (initial_device == 0)
				initial_device = st.st_dev;
			else if (st.st_dev != initial_device &&
				(flags & GIT_REPOSITORY_OPEN_CROSS_FS) == 0)
				break;

			if (S_ISDIR(st.st_mode)) {
				if (valid_repository_path(&path)) {
					git_path_to_dir(&path);
					git_buf_set(repo_path, path.ptr, path.size);
					break;
				}
			}
			else if (S_ISREG(st.st_mode)) {
				git_buf repo_link = GIT_BUF_INIT;

				if (!(error = read_gitfile(&repo_link, path.ptr))) {
					if (valid_repository_path(&repo_link))
						git_buf_swap(repo_path, &repo_link);

					git_buf_free(&repo_link);
					break;
				}
				git_buf_free(&repo_link);
			}
		}

		/* move up one directory level */
		if (git_path_dirname_r(&path, path.ptr) < 0) {
			error = -1;
			break;
		}

		if (try_with_dot_git) {
			/* if we tried original dir with and without .git AND either hit
			 * directory ceiling or NO_SEARCH was requested, then be done.
			 */
			if (path.ptr[ceiling_offset] == '\0' ||
				(flags & GIT_REPOSITORY_OPEN_NO_SEARCH) != 0)
				break;
			/* otherwise look first for .git item */
			error = git_buf_joinpath(&path, path.ptr, DOT_GIT);
		}
		try_with_dot_git = !try_with_dot_git;
	}

	if (!error && parent_path != NULL) {
		if (!git_buf_len(repo_path))
			git_buf_clear(parent_path);
		else {
			git_path_dirname_r(parent_path, path.ptr);
			git_path_to_dir(parent_path);
		}
		if (git_buf_oom(parent_path))
			return -1;
	}

	git_buf_free(&path);

	if (!git_buf_len(repo_path) && !error) {
		giterr_set(GITERR_REPOSITORY,
			"Could not find repository from '%s'", start_path);
		error = GIT_ENOTFOUND;
	}

	return error;
}

int git_repository_open_ext(
	git_repository **repo_ptr,
	const char *start_path,
	uint32_t flags,
	const char *ceiling_dirs)
{
	int error;
	git_buf path = GIT_BUF_INIT, parent = GIT_BUF_INIT;
	git_repository *repo;

	*repo_ptr = NULL;

	if ((error = find_repo(&path, &parent, start_path, flags, ceiling_dirs)) < 0)
		return error;

	repo = repository_alloc();
	GITERR_CHECK_ALLOC(repo);

	repo->path_repository = git_buf_detach(&path);
	GITERR_CHECK_ALLOC(repo->path_repository);

	if ((error = load_config_data(repo)) < 0 ||
		(error = load_workdir(repo, &parent)) < 0)
	{
		git_repository_free(repo);
		return error;
	}

	git_buf_free(&parent);
	*repo_ptr = repo;
	return 0;
}

int git_repository_open(git_repository **repo_out, const char *path)
{
	return git_repository_open_ext(
		repo_out, path, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL);
}

int git_repository_discover(
	char *repository_path,
	size_t size,
	const char *start_path,
	int across_fs,
	const char *ceiling_dirs)
{
	git_buf path = GIT_BUF_INIT;
	uint32_t flags = across_fs ? GIT_REPOSITORY_OPEN_CROSS_FS : 0;
	int error;

	assert(start_path && repository_path && size > 0);

	*repository_path = '\0';

	if ((error = find_repo(&path, NULL, start_path, flags, ceiling_dirs)) < 0)
		return error != GIT_ENOTFOUND ? -1 : error;

	if (size < (size_t)(path.size + 1)) {
		giterr_set(GITERR_REPOSITORY,
			"The given buffer is too long to store the discovered path");
		git_buf_free(&path);
		return -1;
	}

	/* success: we discovered a repository */
	git_buf_copy_cstr(repository_path, size, &path);
	git_buf_free(&path);
	return 0;
}

static int load_config(
	git_config **out,
	git_repository *repo,
	const char *global_config_path,
	const char *system_config_path)
{
	git_buf config_path = GIT_BUF_INIT;
	git_config *cfg = NULL;

	assert(repo && out);

	if (git_config_new(&cfg) < 0)
		return -1;

	if (git_buf_joinpath(
		&config_path, repo->path_repository, GIT_CONFIG_FILENAME_INREPO) < 0)
		goto on_error;

	if (git_config_add_file_ondisk(cfg, config_path.ptr, 3) < 0)
		goto on_error;

	git_buf_free(&config_path);

	if (global_config_path != NULL) {
		if (git_config_add_file_ondisk(cfg, global_config_path, 2) < 0)
			goto on_error;
	}

	if (system_config_path != NULL) {
		if (git_config_add_file_ondisk(cfg, system_config_path, 1) < 0)
			goto on_error;
	}

	*out = cfg;
	return 0;

on_error:
	git_buf_free(&config_path);
	git_config_free(cfg);
	*out = NULL;
	return -1;
}

int git_repository_config__weakptr(git_config **out, git_repository *repo)
{
	if (repo->_config == NULL) {
		git_buf global_buf = GIT_BUF_INIT, system_buf = GIT_BUF_INIT;
		int res;

		const char *global_config_path = NULL;
		const char *system_config_path = NULL;

		if (git_config_find_global_r(&global_buf) == 0)
			global_config_path = global_buf.ptr;

		if (git_config_find_system_r(&system_buf) == 0)
			system_config_path = system_buf.ptr;

		res = load_config(&repo->_config, repo, global_config_path, system_config_path);

		git_buf_free(&global_buf);
		git_buf_free(&system_buf);

		if (res < 0)
			return -1;

		GIT_REFCOUNT_OWN(repo->_config, repo);
	}

	*out = repo->_config;
	return 0;
}

int git_repository_config(git_config **out, git_repository *repo)
{
	if (git_repository_config__weakptr(out, repo) < 0)
		return -1;

	GIT_REFCOUNT_INC(*out);
	return 0;
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
		git_buf odb_path = GIT_BUF_INIT;
		int res;

		if (git_buf_joinpath(&odb_path, repo->path_repository, GIT_OBJECTS_DIR) < 0)
			return -1;

		res = git_odb_open(&repo->_odb, odb_path.ptr);
		git_buf_free(&odb_path); /* done with path */

		if (res < 0)
			return -1;

		GIT_REFCOUNT_OWN(repo->_odb, repo);
	}

	*out = repo->_odb;
	return 0;
}

int git_repository_odb(git_odb **out, git_repository *repo)
{
	if (git_repository_odb__weakptr(out, repo) < 0)
		return -1;

	GIT_REFCOUNT_INC(*out);
	return 0;
}

void git_repository_set_odb(git_repository *repo, git_odb *odb)
{
	assert(repo && odb);

	drop_odb(repo);

	repo->_odb = odb;
	GIT_REFCOUNT_OWN(repo->_odb, repo);
	GIT_REFCOUNT_INC(odb);
}

int git_repository_index__weakptr(git_index **out, git_repository *repo)
{
	assert(out && repo);

	if (repo->_index == NULL) {
		int res;
		git_buf index_path = GIT_BUF_INIT;

		if (git_buf_joinpath(&index_path, repo->path_repository, GIT_INDEX_FILE) < 0)
			return -1;

		res = git_index_open(&repo->_index, index_path.ptr);
		git_buf_free(&index_path); /* done with path */

		if (res < 0)
			return -1;

		GIT_REFCOUNT_OWN(repo->_index, repo);
	}

	*out = repo->_index;
	return 0;
}

int git_repository_index(git_index **out, git_repository *repo)
{
	if (git_repository_index__weakptr(out, repo) < 0)
		return -1;

	GIT_REFCOUNT_INC(*out);
	return 0;
}

void git_repository_set_index(git_repository *repo, git_index *index)
{
	assert(repo && index);

	drop_index(repo);

	repo->_index = index;
	GIT_REFCOUNT_OWN(repo->_index, repo);
	GIT_REFCOUNT_INC(index);
}

static int check_repositoryformatversion(git_repository *repo)
{
	git_config *config;
	int version;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	if (git_config_get_int32(&version, config, "core.repositoryformatversion") < 0)
		return -1;

	if (GIT_REPO_VERSION < version) {
		giterr_set(GITERR_REPOSITORY,
			"Unsupported repository version %d. Only versions up to %d are supported.",
			version, GIT_REPO_VERSION);
		return -1;
	}

	return 0;
}

static int repo_init_reinit(git_repository **repo_out, const char *repository_path, int is_bare)
{
	git_repository *repo = NULL;

	GIT_UNUSED(is_bare);

	if (git_repository_open(&repo, repository_path) < 0)
		return -1;

	if (check_repositoryformatversion(repo) < 0) {
		git_repository_free(repo);
		return -1;
	}

	/* TODO: reinitialize the templates */

	*repo_out = repo;
	return 0;
}

static int repo_init_createhead(const char *git_dir)
{
	git_buf ref_path = GIT_BUF_INIT;
	git_filebuf ref = GIT_FILEBUF_INIT;

	if (git_buf_joinpath(&ref_path, git_dir, GIT_HEAD_FILE) < 0 ||
		git_filebuf_open(&ref, ref_path.ptr, 0) < 0 ||
		git_filebuf_printf(&ref, "ref: refs/heads/master\n") < 0 ||
		git_filebuf_commit(&ref, GIT_REFS_FILE_MODE) < 0)
		return -1;

	git_buf_free(&ref_path);
	return 0;
}

static int repo_init_config(const char *git_dir, int is_bare)
{
	git_buf cfg_path = GIT_BUF_INIT;
	git_config *config = NULL;

#define SET_REPO_CONFIG(type, name, val) {\
	if (git_config_set_##type(config, name, val) < 0) { \
		git_buf_free(&cfg_path); \
		git_config_free(config); \
		return -1; } \
}

	if (git_buf_joinpath(&cfg_path, git_dir, GIT_CONFIG_FILENAME_INREPO) < 0)
		return -1;

	if (git_config_open_ondisk(&config, cfg_path.ptr) < 0) {
		git_buf_free(&cfg_path);
		return -1;
	}

	SET_REPO_CONFIG(bool, "core.bare", is_bare);
	SET_REPO_CONFIG(int32, "core.repositoryformatversion", GIT_REPO_VERSION);
	/* TODO: what other defaults? */

	git_buf_free(&cfg_path);
	git_config_free(config);
	return 0;
}

#define GIT_HOOKS_DIR "hooks/"
#define GIT_HOOKS_DIR_MODE 0755

#define GIT_HOOKS_README_FILE GIT_HOOKS_DIR "README.sample"
#define GIT_HOOKS_README_MODE 0755
#define GIT_HOOKS_README_CONTENT \
"#!/bin/sh\n"\
"#\n"\
"# Place appropriately named executable hook scripts into this directory\n"\
"# to intercept various actions that git takes.  See `git help hooks` for\n"\
"# more information.\n"

#define GIT_INFO_DIR "info/"
#define GIT_INFO_DIR_MODE 0755

#define GIT_INFO_EXCLUDE_FILE GIT_INFO_DIR "exclude"
#define GIT_INFO_EXCLUDE_MODE 0644
#define GIT_INFO_EXCLUDE_CONTENT \
"# File patterns to ignore; see `git help ignore` for more information.\n"\
"# Lines that start with '#' are comments.\n"

#define GIT_DESC_FILE "description"
#define GIT_DESC_MODE 0644
#define GIT_DESC_CONTENT "Unnamed repository; edit this file 'description' to name the repository.\n"

static int repo_write_template(
	const char *git_dir, const char *file, mode_t mode, const char *content)
{
	git_buf path = GIT_BUF_INIT;
	int fd, error = 0;

	if (git_buf_joinpath(&path, git_dir, file) < 0)
		return -1;

	fd = p_open(git_buf_cstr(&path), O_WRONLY | O_CREAT | O_EXCL, mode);

	if (fd >= 0) {
		error = p_write(fd, content, strlen(content));

		p_close(fd);
	}
	else if (errno != EEXIST)
		error = fd;

	git_buf_free(&path);

	if (error)
		giterr_set(GITERR_OS,
			"Failed to initialize repository with template '%s'", file);

	return error;
}

static int repo_init_structure(const char *git_dir, int is_bare)
{
	int i;
	struct { const char *dir; mode_t mode; } dirs[] = {
		{ GIT_OBJECTS_INFO_DIR, GIT_OBJECT_DIR_MODE }, /* '/objects/info/' */
		{ GIT_OBJECTS_PACK_DIR, GIT_OBJECT_DIR_MODE }, /* '/objects/pack/' */
		{ GIT_REFS_HEADS_DIR, GIT_REFS_DIR_MODE },     /* '/refs/heads/' */
		{ GIT_REFS_TAGS_DIR, GIT_REFS_DIR_MODE },      /* '/refs/tags/' */
		{ GIT_HOOKS_DIR, GIT_HOOKS_DIR_MODE },         /* '/hooks/' */
		{ GIT_INFO_DIR, GIT_INFO_DIR_MODE },           /* '/info/' */
		{ NULL, 0 }
	};
	struct { const char *file; mode_t mode; const char *content; } tmpl[] = {
		{ GIT_DESC_FILE, GIT_DESC_MODE, GIT_DESC_CONTENT },
		{ GIT_HOOKS_README_FILE, GIT_HOOKS_README_MODE, GIT_HOOKS_README_CONTENT },
		{ GIT_INFO_EXCLUDE_FILE, GIT_INFO_EXCLUDE_MODE, GIT_INFO_EXCLUDE_CONTENT },
		{ NULL, 0, NULL }
	};

	/* Make the base directory */
	if (git_futils_mkdir_r(git_dir, NULL, is_bare ? GIT_BARE_DIR_MODE : GIT_DIR_MODE) < 0)
		return -1;

	/* Hides the ".git" directory */
	if (!is_bare) {
#ifdef GIT_WIN32
		if (p_hide_directory__w32(git_dir) < 0) {
			giterr_set(GITERR_REPOSITORY,
				"Failed to mark Git repository folder as hidden");
			return -1;
		}
#endif
	}

	/* Make subdirectories as needed */
	for (i = 0; dirs[i].dir != NULL; ++i) {
		if (git_futils_mkdir_r(dirs[i].dir, git_dir, dirs[i].mode) < 0)
			return -1;
	}

	/* Make template files as needed */
	for (i = 0; tmpl[i].file != NULL; ++i) {
		if (repo_write_template(
				git_dir, tmpl[i].file, tmpl[i].mode, tmpl[i].content) < 0)
			return -1;
	}

	return 0;
}

int git_repository_init(git_repository **repo_out, const char *path, unsigned is_bare)
{
	git_buf repository_path = GIT_BUF_INIT;

	assert(repo_out && path);

	if (git_buf_joinpath(&repository_path, path, is_bare ? "" : GIT_DIR) < 0)
		return -1;

	if (git_path_isdir(repository_path.ptr) == true) {
		if (valid_repository_path(&repository_path) == true) {
			int res = repo_init_reinit(repo_out, repository_path.ptr, is_bare);
			git_buf_free(&repository_path);
			return res;
		}
	}

	if (repo_init_structure(repository_path.ptr, is_bare) < 0 ||
		repo_init_config(repository_path.ptr, is_bare) < 0 || 
		repo_init_createhead(repository_path.ptr) < 0 ||
		git_repository_open(repo_out, repository_path.ptr) < 0) {
		git_buf_free(&repository_path);
		return -1;
	}

	git_buf_free(&repository_path);
	return 0;
}

int git_repository_head_detached(git_repository *repo)
{
	git_reference *ref;
	git_odb *odb = NULL;
	int exists;

	if (git_repository_odb__weakptr(&odb, repo) < 0)
		return -1;

	if (git_reference_lookup(&ref, repo, GIT_HEAD_FILE) < 0)
		return -1;

	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		git_reference_free(ref);
		return 0;
	}

	exists = git_odb_exists(odb, git_reference_oid(ref));

	git_reference_free(ref);
	return exists;
}

int git_repository_head(git_reference **head_out, git_repository *repo)
{
	return git_reference_lookup_resolved(head_out, repo, GIT_HEAD_FILE, -1);
}

int git_repository_head_orphan(git_repository *repo)
{
	git_reference *ref = NULL;
	int error;

	error = git_repository_head(&ref, repo);
	git_reference_free(ref);

	if (error == GIT_ENOTFOUND)
		return 1;

	if (error < 0)
		return -1;

	return 0;
}

int git_repository_is_empty(git_repository *repo)
{
	git_reference *head = NULL, *branch = NULL;
	int error;

	if (git_reference_lookup(&head, repo, "HEAD") < 0)
		return -1;

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

	if (error == GIT_ENOTFOUND)
		return 1;

	if (error < 0)
		return -1;

	return 0;
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
	git_buf path = GIT_BUF_INIT;

	assert(repo && workdir);

	if (git_path_prettify_dir(&path, workdir, NULL) < 0)
		return -1;

	git__free(repo->workdir);

	repo->workdir = git_buf_detach(&path);
	repo->is_bare = 0;
	return 0;
}

int git_repository_is_bare(git_repository *repo)
{
	assert(repo);
	return repo->is_bare;
}

int git_repository_head_tree(git_tree **tree, git_repository *repo)
{
	git_oid head_oid;
	git_object *obj = NULL;

	if (git_reference_name_to_oid(&head_oid, repo, GIT_HEAD_FILE) < 0) {
		/* cannot resolve HEAD - probably brand new repo */
		giterr_clear();
		*tree = NULL;
		return 0;
	}

	if (git_object_lookup(&obj, repo, &head_oid, GIT_OBJ_ANY) < 0 ||
		git_object__resolve_to_type(&obj, GIT_OBJ_TREE) < 0)
		return -1;

	*tree = (git_tree *)obj;
	return 0;
}
