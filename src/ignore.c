#include "git2/ignore.h"
#include "ignore.h"
#include "path.h"
#include "config.h"

#define GIT_IGNORE_INTERNAL		"[internal]exclude"
#define GIT_IGNORE_FILE_INREPO	"info/exclude"
#define GIT_IGNORE_FILE			".gitignore"

#define GIT_IGNORE_DEFAULT_RULES ".\n..\n.git\n"

static int parse_ignore_file(
	git_repository *repo, void *parsedata, const char *buffer, git_attr_file *ignores)
{
	int error = 0;
	git_attr_fnmatch *match = NULL;
	const char *scan = NULL;
	char *context = NULL;
	bool ignore_case = false;
	git_config *cfg = NULL;
	int val;

	/* Prefer to have the caller pass in a git_ignores as the parsedata object.
	 * If they did not, then we can (much more slowly) find the value of
	 * ignore_case by using the repository object. */
	if (parsedata != NULL) {
		ignore_case = ((git_ignores *)parsedata)->ignore_case;
	} else {
		if ((error = git_repository_config(&cfg, repo)) < 0)
			return error;

		if (git_config_get_bool(&val, cfg, "core.ignorecase") == 0)
			ignore_case = (val != 0);

		git_config_free(cfg);
	}

	if (ignores->key && git__suffixcmp(ignores->key, "/" GIT_IGNORE_FILE) == 0) {
		context = ignores->key + 2;
		context[strlen(context) - strlen(GIT_IGNORE_FILE)] = '\0';
	}

	scan = buffer;

	while (!error && *scan) {
		if (!match) {
			match = git__calloc(1, sizeof(*match));
			GITERR_CHECK_ALLOC(match);
		}

		match->flags = GIT_ATTR_FNMATCH_ALLOWSPACE;

		if (!(error = git_attr_fnmatch__parse(
			match, ignores->pool, context, &scan)))
		{
			match->flags |= GIT_ATTR_FNMATCH_IGNORE;

			if (ignore_case)
				match->flags |= GIT_ATTR_FNMATCH_ICASE;

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

	git__free(match);
	/* restore file path used for context */
	if (context)
		context[strlen(context)] = '.'; /* first char of GIT_IGNORE_FILE */

	return error;
}

#define push_ignore_file(R,IGN,S,B,F) \
	git_attr_cache__push_file((R),(B),(F),GIT_ATTR_FILE_FROM_FILE,parse_ignore_file,(IGN),(S))

static int push_one_ignore(void *ref, git_buf *path)
{
	git_ignores *ign = (git_ignores *)ref;
	return push_ignore_file(ign->repo, ign, &ign->ign_path, path->ptr, GIT_IGNORE_FILE);
}

static int get_internal_ignores(git_attr_file **ign, git_repository *repo)
{
	int error;

	if (!(error = git_attr_cache__init(repo)))
		error = git_attr_cache__internal_file(repo, GIT_IGNORE_INTERNAL, ign);

	if (!error && !(*ign)->rules.length)
		error = parse_ignore_file(repo, NULL, GIT_IGNORE_DEFAULT_RULES, *ign);

	return error;
}

int git_ignore__for_path(
	git_repository *repo,
	const char *path,
	git_ignores *ignores)
{
	int error = 0;
	const char *workdir = git_repository_workdir(repo);
	git_config *cfg = NULL;
	int val;

	assert(ignores);

	ignores->repo = repo;
	git_buf_init(&ignores->dir, 0);
	ignores->ign_internal = NULL;

	/* Set the ignore_case flag appropriately */
	if ((error = git_repository_config(&cfg, repo)) < 0)
		goto cleanup;

	if (git_config_get_bool(&val, cfg, "core.ignorecase") == 0)
		ignores->ignore_case = (val != 0);
	else
		ignores->ignore_case = 0;

	git_config_free(cfg);

	if ((error = git_vector_init(&ignores->ign_path, 8, NULL)) < 0 ||
		(error = git_vector_init(&ignores->ign_global, 2, NULL)) < 0 ||
		(error = git_attr_cache__init(repo)) < 0)
		goto cleanup;

	/* given a unrooted path in a non-bare repo, resolve it */
	if (workdir && git_path_root(path) < 0)
		error = git_path_find_dir(&ignores->dir, path, workdir);
	else
		error = git_buf_sets(&ignores->dir, path);
	if (error < 0)
		goto cleanup;

	/* set up internals */
	error = get_internal_ignores(&ignores->ign_internal, repo);
	if (error < 0)
		goto cleanup;

	/* load .gitignore up the path */
	if (workdir != NULL) {
		error = git_path_walk_up(
			&ignores->dir, workdir, push_one_ignore, ignores);
		if (error < 0)
			goto cleanup;
	}

	/* load .git/info/exclude */
	error = push_ignore_file(repo, ignores, &ignores->ign_global,
		git_repository_path(repo), GIT_IGNORE_FILE_INREPO);
	if (error < 0)
		goto cleanup;

	/* load core.excludesfile */
	if (git_repository_attr_cache(repo)->cfg_excl_file != NULL)
		error = push_ignore_file(repo, ignores, &ignores->ign_global, NULL,
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
		return push_ignore_file(
			ign->repo, ign, &ign->ign_path, ign->dir.ptr, GIT_IGNORE_FILE);
}

int git_ignore__pop_dir(git_ignores *ign)
{
	if (ign->ign_path.length > 0) {
		git_attr_file *file = git_vector_last(&ign->ign_path);
		if (git__suffixcmp(ign->dir.ptr, file->key + 2) == 0)
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
	size_t j;
	git_attr_fnmatch *match;

	git_vector_rforeach(rules, j, match) {
		if (git_attr_fnmatch__match(match, path)) {
			*ignored = ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0);
			return true;
		}
	}

	return false;
}

int git_ignore__lookup(
	git_ignores *ignores, const char *pathname, int *ignored)
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

int git_ignore_add_rule(
	git_repository *repo,
	const char *rules)
{
	int error;
	git_attr_file *ign_internal;

	if (!(error = get_internal_ignores(&ign_internal, repo)))
		error = parse_ignore_file(repo, NULL, rules, ign_internal);

	return error;
}

int git_ignore_clear_internal_rules(
	git_repository *repo)
{
	int error;
	git_attr_file *ign_internal;

	if (!(error = get_internal_ignores(&ign_internal, repo))) {
		git_attr_file__clear_rules(ign_internal);

		return parse_ignore_file(
			repo, NULL, GIT_IGNORE_DEFAULT_RULES, ign_internal);
	}

	return error;
}

int git_ignore_path_is_ignored(
	int *ignored,
	git_repository *repo,
	const char *pathname)
{
	int error;
	const char *workdir;
	git_attr_path path;
	char *tail, *end;
	bool full_is_dir;
	git_ignores ignores;
	unsigned int i;
	git_attr_file *file;

	assert(ignored && pathname);

	workdir = repo ? git_repository_workdir(repo) : NULL;

	if ((error = git_attr_path__init(&path, pathname, workdir)) < 0)
		return error;

	tail = path.path;
	end  = &path.full.ptr[path.full.size];
	full_is_dir = path.is_dir;

	while (1) {
		/* advance to next component of path */
		path.basename = tail;

		while (tail < end && *tail != '/') tail++;
		*tail = '\0';

		path.full.size = (tail - path.full.ptr);
		path.is_dir = (tail == end) ? full_is_dir : true;

		/* update ignores for new path fragment */
		if (path.basename == path.path)
			error = git_ignore__for_path(repo, path.path, &ignores);
		else
			error = git_ignore__push_dir(&ignores, path.basename);
		if (error < 0)
			break;

		/* first process builtins - success means path was found */
		if (ignore_lookup_in_rules(
				&ignores.ign_internal->rules, &path, ignored))
			goto cleanup;

		/* next process files in the path */
		git_vector_foreach(&ignores.ign_path, i, file) {
			if (ignore_lookup_in_rules(&file->rules, &path, ignored))
				goto cleanup;
		}

		/* last process global ignores */
		git_vector_foreach(&ignores.ign_global, i, file) {
			if (ignore_lookup_in_rules(&file->rules, &path, ignored))
				goto cleanup;
		}

		/* if we found no rules before reaching the end, we're done */
		if (tail == end)
			break;

		/* reinstate divider in path */
		*tail = '/';
		while (*tail == '/') tail++;
	}

	*ignored = 0;

cleanup:
	git_attr_path__free(&path);
	git_ignore__free(&ignores);
	return error;
}

