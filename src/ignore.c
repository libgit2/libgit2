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
	git_fbuffer fbuf = GIT_FBUFFER_INIT;
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

	scan = fbuf.data;

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

	git_futils_freebuffer(&fbuf);
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
	return push_ignore(ign->repo, &ign->stack, path->ptr, GIT_IGNORE_FILE);
}

int git_ignore__for_path(git_repository *repo, const char *path, git_ignores *ignores)
{
	int error = GIT_SUCCESS;
	git_buf dir = GIT_BUF_INIT;
	git_config *cfg;
	const char *workdir = git_repository_workdir(repo);

	assert(ignores);

	if ((error = git_attr_cache__init(repo)) < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_path_find_dir(&dir, path, workdir)) < GIT_SUCCESS)
		goto cleanup;

	ignores->repo = repo;
	ignores->dir  = NULL;
	git_vector_init(&ignores->stack, 2, NULL);

	/* insert internals */
	if ((error = push_ignore(repo, &ignores->stack, NULL, GIT_IGNORE_INTERNAL)) < GIT_SUCCESS)
		goto cleanup;

	/* load .gitignore up the path */
	if ((error = git_path_walk_up(&dir, workdir, push_one_ignore, ignores)) < GIT_SUCCESS)
		goto cleanup;

	/* load .git/info/exclude */
	if ((error = push_ignore(repo, &ignores->stack, repo->path_repository, GIT_IGNORE_FILE_INREPO)) < GIT_SUCCESS)
		goto cleanup;

	/* load core.excludesfile */
	if ((error = git_repository_config(&cfg, repo)) == GIT_SUCCESS) {
		const char *core_ignore;
		error = git_config_get_string(cfg, GIT_IGNORE_CONFIG, &core_ignore);
		if (error == GIT_SUCCESS && core_ignore != NULL)
			error = push_ignore(repo, &ignores->stack, NULL, core_ignore);
		else {
			error = GIT_SUCCESS;
			git_clearerror(); /* don't care if attributesfile is not set */
		}
		git_config_free(cfg);
	}

cleanup:
	if (error < GIT_SUCCESS)
		git__rethrow(error, "Could not get ignore files for '%s'", path);
	else
		ignores->dir = git_buf_detach(&dir);

	git_buf_free(&dir);

	return error;
}

void git_ignore__free(git_ignores *ignores)
{
	git__free(ignores->dir);
	ignores->dir = NULL;
	git_vector_free(&ignores->stack);
}

int git_ignore__lookup(git_ignores *ignores, const char *pathname, int *ignored)
{
	int error;
	unsigned int i, j;
	git_attr_file *file;
	git_attr_path path;
	git_attr_fnmatch *match;

	if ((error = git_attr_path__init(
		&path, pathname, git_repository_workdir(ignores->repo))) < GIT_SUCCESS)
		return git__rethrow(error, "Could not get attribute for '%s'", pathname);

	*ignored = 0;

	git_vector_foreach(&ignores->stack, i, file) {
		git_vector_rforeach(&file->rules, j, match) {
			if (git_attr_fnmatch__match(match, &path) == GIT_SUCCESS) {
				*ignored = ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0);
				goto found;
			}
		}
	}
found:

	return error;
}
