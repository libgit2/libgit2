#include "ignore.h"
#include "path.h"
#include "git2/config.h"

#define GIT_IGNORE_INTERNAL		"[internal]exclude"
#define GIT_IGNORE_FILE_INREPO	"info/exclude"
#define GIT_IGNORE_FILE			".gitignore"
#define GIT_IGNORE_CONFIG		"core.excludesfile"

static int load_ignore_file(
	git_repository *repo, const char *path, git_attr_file *ignores)
{
	int error = GIT_SUCCESS;
	git_buf fbuf = GIT_BUF_INIT;
	git_attr_fnmatch *match = NULL;
	const char *scan = NULL;
	char *context = NULL;

	if (ignores->path == NULL)
		error = git_attr_file__set_path(repo, path, ignores);

	if (git__suffixcmp(ignores->path, GIT_IGNORE_FILE) == 0) {
		context = git__strndup(ignores->path,
			strlen(ignores->path) - strlen(GIT_IGNORE_FILE));
		if (!context) error = GIT_ENOMEM;
	}

	if (error == GIT_SUCCESS)
		error = git_futils_readbuffer(&fbuf, path);

	scan = fbuf.ptr;

	while (error == GIT_SUCCESS && *scan) {
		if (!match && !(match = git__calloc(1, sizeof(git_attr_fnmatch)))) {
			error = GIT_ENOMEM;
			break;
		}

		if (!(error = git_attr_fnmatch__parse(match, context, &scan))) {
			match->flags = match->flags | GIT_ATTR_FNMATCH_IGNORE;
			scan = git__next_line(scan);
			error = git_vector_insert(&ignores->rules, match);
		}

		if (error != GIT_SUCCESS) {
			git__free(match->pattern);
			match->pattern = NULL;

			if (error == GIT_ENOTFOUND)
				error = GIT_SUCCESS;
		} else {
			match = NULL; /* vector now "owns" the match */
		}
	}

	git_buf_free(&fbuf);
	git__free(match);
	git__free(context);

	if (error != GIT_SUCCESS)
		git__rethrow(error, "Could not open ignore file '%s'", path);

	return error;
}

#define push_ignore(R,S,B,F) \
	git_attr_cache__push_file((R),(S),(B),(F),load_ignore_file)

static int push_one_ignore(void *ref, git_buf *path)
{
	git_ignores *ign = (git_ignores *)ref;
	return push_ignore(ign->repo, &ign->ign_path, path->ptr, GIT_IGNORE_FILE);
}

int git_ignore__for_path(git_repository *repo, const char *path, git_ignores *ignores)
{
	int error = GIT_SUCCESS;
	git_config *cfg;
	const char *workdir = git_repository_workdir(repo);

	assert(ignores);

	ignores->repo = repo;
	git_buf_init(&ignores->dir, 0);
	ignores->ign_internal = NULL;
	git_vector_init(&ignores->ign_path, 8, NULL);
	git_vector_init(&ignores->ign_global, 2, NULL);

	if ((error = git_attr_cache__init(repo)) < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_path_find_dir(&ignores->dir, path, workdir)) < GIT_SUCCESS)
		goto cleanup;

	/* set up internals */
	error = git_attr_cache__lookup_or_create_file(
		repo, GIT_IGNORE_INTERNAL, NULL, NULL, &ignores->ign_internal);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* load .gitignore up the path */
	error = git_path_walk_up(&ignores->dir, workdir, push_one_ignore, ignores);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* load .git/info/exclude */
	error = push_ignore(repo, &ignores->ign_global,
		repo->path_repository, GIT_IGNORE_FILE_INREPO);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* load core.excludesfile */
	if ((error = git_repository_config(&cfg, repo)) == GIT_SUCCESS) {
		const char *core_ignore;
		error = git_config_get_string(cfg, GIT_IGNORE_CONFIG, &core_ignore);
		if (error == GIT_SUCCESS && core_ignore != NULL)
			error = push_ignore(repo, &ignores->ign_global, NULL, core_ignore);
		else {
			error = GIT_SUCCESS;
			git_clearerror(); /* don't care if attributesfile is not set */
		}
		git_config_free(cfg);
	}

cleanup:
	if (error < GIT_SUCCESS) {
		git_ignore__free(ignores);
		git__rethrow(error, "Could not get ignore files for '%s'", path);
	}

	return error;
}

int git_ignore__push_dir(git_ignores *ign, const char *dir)
{
	int error = git_buf_joinpath(&ign->dir, ign->dir.ptr, dir);

	if (error == GIT_SUCCESS)
		error = push_ignore(
			ign->repo, &ign->ign_path, ign->dir.ptr, GIT_IGNORE_FILE);

	return error;
}

int git_ignore__pop_dir(git_ignores *ign)
{
	if (ign->ign_path.length > 0) {
		git_attr_file *file = git_vector_last(&ign->ign_path);
		if (git__suffixcmp(ign->dir.ptr, file->path) == 0)
			git_vector_pop(&ign->ign_path);
		git_buf_rtruncate_at_char(&ign->dir, '/');
	}
	return GIT_SUCCESS;
}

void git_ignore__free(git_ignores *ignores)
{
	/* don't need to free ignores->ign_internal since it is in cache */
	git_vector_free(&ignores->ign_path);
	git_vector_free(&ignores->ign_global);
	git_buf_free(&ignores->dir);
}

static int ignore_lookup_in_rules(
	git_vector *rules, git_attr_path *path, int *ignored)
{
	unsigned int j;
	git_attr_fnmatch *match;

	git_vector_rforeach(rules, j, match) {
		if (git_attr_fnmatch__match(match, path) == GIT_SUCCESS) {
			*ignored = ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0);
			return GIT_SUCCESS;
		}
	}

	return GIT_ENOTFOUND;
}

int git_ignore__lookup(git_ignores *ignores, const char *pathname, int *ignored)
{
	int error;
	unsigned int i;
	git_attr_file *file;
	git_attr_path path;

	if ((error = git_attr_path__init(
		&path, pathname, git_repository_workdir(ignores->repo))) < GIT_SUCCESS)
		return git__rethrow(error, "Could not get attribute for '%s'", pathname);

	/* first process builtins */
	error = ignore_lookup_in_rules(
		&ignores->ign_internal->rules, &path, ignored);
	if (error == GIT_SUCCESS)
		return error;

	/* next process files in the path */
	git_vector_foreach(&ignores->ign_path, i, file) {
		error = ignore_lookup_in_rules(&file->rules, &path, ignored);
		if (error == GIT_SUCCESS)
			return error;
	}

	/* last process global ignores */
	git_vector_foreach(&ignores->ign_global, i, file) {
		error = ignore_lookup_in_rules(&file->rules, &path, ignored);
		if (error == GIT_SUCCESS)
			return error;
	}

	*ignored = 0;

	return GIT_SUCCESS;
}
