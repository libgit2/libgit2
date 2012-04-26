#include "ignore.h"
#include "path.h"

#define GIT_IGNORE_INTERNAL		"[internal]exclude"
#define GIT_IGNORE_FILE_INREPO	"info/exclude"
#define GIT_IGNORE_FILE			".gitignore"

static int load_ignore_file(
	git_repository *repo, const char *path, git_attr_file *ignores)
{
	int error;
	git_buf fbuf = GIT_BUF_INIT;
	git_attr_fnmatch *match = NULL;
	const char *scan = NULL;
	char *context = NULL;

	if (ignores->path == NULL) {
		if (git_attr_file__set_path(repo, path, ignores) < 0)
			return -1;
	}

	if (git__suffixcmp(ignores->path, GIT_IGNORE_FILE) == 0) {
		context = git__strndup(ignores->path,
			strlen(ignores->path) - strlen(GIT_IGNORE_FILE));
		GITERR_CHECK_ALLOC(context);
	}

	error = git_futils_readbuffer(&fbuf, path);

	scan = fbuf.ptr;

	while (!error && *scan) {
		if (!match) {
			match = git__calloc(1, sizeof(*match));
			GITERR_CHECK_ALLOC(match);
		}

		if (!(error = git_attr_fnmatch__parse(
			match, ignores->pool, context, &scan)))
		{
			match->flags = match->flags | GIT_ATTR_FNMATCH_IGNORE;
			scan = git__next_line(scan);
			error = git_vector_insert(&ignores->rules, match);
		}

		if (error != 0) {
			git__free(match->pattern);
			match->pattern = NULL;

			if (error == GIT_ENOTFOUND)
				error = 0;
		} else {
			match = NULL; /* vector now "owns" the match */
		}
	}

	git_buf_free(&fbuf);
	git__free(match);
	git__free(context);

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
	int error = 0;
	const char *workdir = git_repository_workdir(repo);

	assert(ignores);

	ignores->repo = repo;
	git_buf_init(&ignores->dir, 0);
	ignores->ign_internal = NULL;

	if ((error = git_vector_init(&ignores->ign_path, 8, NULL)) < 0 ||
		(error = git_vector_init(&ignores->ign_global, 2, NULL)) < 0 ||
		(error = git_attr_cache__init(repo)) < 0)
		goto cleanup;

	/* translate path into directory within workdir */
	if ((error = git_path_find_dir(&ignores->dir, path, workdir)) < 0)
		goto cleanup;

	/* set up internals */
	error = git_attr_cache__lookup_or_create_file(
		repo, GIT_IGNORE_INTERNAL, NULL, NULL, &ignores->ign_internal);
	if (error < 0)
		goto cleanup;

	/* load .gitignore up the path */
	error = git_path_walk_up(&ignores->dir, workdir, push_one_ignore, ignores);
	if (error < 0)
		goto cleanup;

	/* load .git/info/exclude */
	error = push_ignore(repo, &ignores->ign_global,
		git_repository_path(repo), GIT_IGNORE_FILE_INREPO);
	if (error < 0)
		goto cleanup;

	/* load core.excludesfile */
	if (git_repository_attr_cache(repo)->cfg_excl_file != NULL)
		error = push_ignore(repo, &ignores->ign_global, NULL,
			git_repository_attr_cache(repo)->cfg_excl_file);

cleanup:
	if (error < 0)
		git_ignore__free(ignores);

	return error;
}

int git_ignore__push_dir(git_ignores *ign, const char *dir)
{
	if (git_buf_joinpath(&ign->dir, ign->dir.ptr, dir) < 0)
		return -1;
	else
		return push_ignore(
			ign->repo, &ign->ign_path, ign->dir.ptr, GIT_IGNORE_FILE);
}

int git_ignore__pop_dir(git_ignores *ign)
{
	if (ign->ign_path.length > 0) {
		git_attr_file *file = git_vector_last(&ign->ign_path);
		if (git__suffixcmp(ign->dir.ptr, file->path) == 0)
			git_vector_pop(&ign->ign_path);
		git_buf_rtruncate_at_char(&ign->dir, '/');
	}
	return 0;
}

void git_ignore__free(git_ignores *ignores)
{
	/* don't need to free ignores->ign_internal since it is in cache */
	git_vector_free(&ignores->ign_path);
	git_vector_free(&ignores->ign_global);
	git_buf_free(&ignores->dir);
}

static bool ignore_lookup_in_rules(
	git_vector *rules, git_attr_path *path, int *ignored)
{
	unsigned int j;
	git_attr_fnmatch *match;

	git_vector_rforeach(rules, j, match) {
		if (git_attr_fnmatch__match(match, path)) {
			*ignored = ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0);
			return true;
		}
	}

	return false;
}

int git_ignore__lookup(git_ignores *ignores, const char *pathname, int *ignored)
{
	unsigned int i;
	git_attr_file *file;
	git_attr_path path;

	if (git_attr_path__init(
		&path, pathname, git_repository_workdir(ignores->repo)) < 0)
		return -1;

	/* first process builtins - success means path was found */
	if (ignore_lookup_in_rules(
			&ignores->ign_internal->rules, &path, ignored))
		goto cleanup;

	/* next process files in the path */
	git_vector_foreach(&ignores->ign_path, i, file) {
		if (ignore_lookup_in_rules(&file->rules, &path, ignored))
			goto cleanup;
	}

	/* last process global ignores */
	git_vector_foreach(&ignores->ign_global, i, file) {
		if (ignore_lookup_in_rules(&file->rules, &path, ignored))
			goto cleanup;
	}

	*ignored = 0;

cleanup:
	git_attr_path__free(&path);
	return 0;
}
