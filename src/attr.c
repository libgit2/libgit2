#include "repository.h"
#include "fileops.h"
#include "config.h"
#include <ctype.h>

static int collect_attr_files(
	git_repository *repo, const char *path, git_vector *files);


int git_attr_get(
    git_repository *repo, const char *pathname,
	const char *name, const char **value)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j;
	git_attr_file *file;
	git_attr_name attr;
	git_attr_rule *rule;

	*value = NULL;

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < GIT_SUCCESS ||
		(error = collect_attr_files(repo, pathname, &files)) < GIT_SUCCESS)
		return git__rethrow(error, "Could not get attribute for %s", pathname);

	attr.name = name;
	attr.name_hash = git_attr_file__name_hash(name);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {
			int pos = git_vector_bsearch(&rule->assigns, &attr);
			git_clearerror(); /* okay if search failed */

			if (pos >= 0) {
				*value = ((git_attr_assignment *)git_vector_get(
							  &rule->assigns, pos))->value;
				goto found;
			}
		}
	}

found:
	git_vector_free(&files);

	return error;
}


typedef struct {
	git_attr_name name;
	git_attr_assignment *found;
} attr_get_many_info;

int git_attr_get_many(
    git_repository *repo, const char *pathname,
    size_t num_attr, const char **names, const char **values)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	attr_get_many_info *info = NULL;
	size_t num_found = 0;

	memset((void *)values, 0, sizeof(const char *) * num_attr);

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < GIT_SUCCESS ||
		(error = collect_attr_files(repo, pathname, &files)) < GIT_SUCCESS)
		return git__rethrow(error, "Could not get attributes for %s", pathname);

	if ((info = git__calloc(num_attr, sizeof(attr_get_many_info))) == NULL) {
		git__rethrow(GIT_ENOMEM, "Could not get attributes for %s", pathname);
		goto cleanup;
	}

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			for (k = 0; k < num_attr; k++) {
				int pos;

				if (info[k].found != NULL) /* already found assignment */
					continue;

				if (!info[k].name.name) {
					info[k].name.name = names[k];
					info[k].name.name_hash = git_attr_file__name_hash(names[k]);
				}

				pos = git_vector_bsearch(&rule->assigns, &info[k].name);
				git_clearerror(); /* okay if search failed */

				if (pos >= 0) {
					info[k].found = (git_attr_assignment *)
						git_vector_get(&rule->assigns, pos);
					values[k] = info[k].found->value;

					if (++num_found == num_attr)
						goto cleanup;
				}
			}
		}
	}

cleanup:
	git_vector_free(&files);
	git__free(info);

	return error;
}


int git_attr_foreach(
    git_repository *repo, const char *pathname,
	int (*callback)(const char *name, const char *value, void *payload),
	void *payload)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	git_attr_assignment *assign;
	git_hashtable *seen = NULL;

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < GIT_SUCCESS ||
		(error = collect_attr_files(repo, pathname, &files)) < GIT_SUCCESS)
		return git__rethrow(error, "Could not get attributes for %s", pathname);

	seen = git_hashtable_alloc(8, git_hash__strhash_cb, git_hash__strcmp_cb);
	if (!seen) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			git_vector_foreach(&rule->assigns, k, assign) {
				/* skip if higher priority assignment was already seen */
				if (git_hashtable_lookup(seen, assign->name))
					continue;

				error = git_hashtable_insert(seen, assign->name, assign);
				if (error != GIT_SUCCESS)
					goto cleanup;

				error = callback(assign->name, assign->value, payload);
				if (error != GIT_SUCCESS)
					goto cleanup;
			}
		}
	}

cleanup:
	if (seen)
		git_hashtable_free(seen);
	git_vector_free(&files);

	if (error != GIT_SUCCESS)
		(void)git__rethrow(error, "Could not get attributes for %s", pathname);

	return error;
}


int git_attr_add_macro(
	git_repository *repo,
	const char *name,
	const char *values)
{
	int error;
	git_attr_rule *macro = NULL;

	if ((error = git_attr_cache__init(repo)) < GIT_SUCCESS)
		return error;

	macro = git__calloc(1, sizeof(git_attr_rule));
	if (!macro)
		return GIT_ENOMEM;

	macro->match.pattern = git__strdup(name);
	if (!macro->match.pattern) {
		git__free(macro);
		return GIT_ENOMEM;
	}

	macro->match.length = strlen(macro->match.pattern);
	macro->match.flags = GIT_ATTR_FNMATCH_MACRO;

	error = git_attr_assignment__parse(repo, &macro->assigns, &values);

	if (error == GIT_SUCCESS)
		error = git_attr_cache__insert_macro(repo, macro);

	if (error < GIT_SUCCESS)
		git_attr_rule__free(macro);

	return error;
}

int git_attr_cache__is_cached(git_repository *repo, const char *path)
{
	const char *cache_key = path;
	if (repo && git__prefixcmp(cache_key, git_repository_workdir(repo)) == 0)
		cache_key += strlen(git_repository_workdir(repo));
	return (git_hashtable_lookup(repo->attrcache.files, cache_key) == NULL);
}

/* add git_attr_file to vector of files, loading if needed */
int git_attr_cache__push_file(
	git_repository *repo,
	git_vector     *stack,
	const char     *base,
	const char     *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *))
{
	int error = GIT_SUCCESS;
	git_attr_cache *cache = &repo->attrcache;
	git_buf path = GIT_BUF_INIT;
	git_attr_file *file = NULL;
	int add_to_cache = 0;
	const char *cache_key;

	if (base != NULL) {
		if ((error = git_buf_joinpath(&path, base, filename)) < GIT_SUCCESS)
			goto cleanup;
		filename = path.ptr;
	}

	/* either get attr_file from cache or read from disk */
	cache_key = filename;
	if (repo && git__prefixcmp(cache_key, git_repository_workdir(repo)) == 0)
		cache_key += strlen(git_repository_workdir(repo));

	file = git_hashtable_lookup(cache->files, cache_key);
	if (file == NULL && git_path_exists(filename) == GIT_SUCCESS) {
		if ((error = git_attr_file__new(&file)) == GIT_SUCCESS) {
			if ((error = loader(repo, filename, file)) < GIT_SUCCESS) {
				git_attr_file__free(file);
				file = NULL;
			}
		}
		add_to_cache = (error == GIT_SUCCESS);
	}

	if (error == GIT_SUCCESS && file != NULL) {
		/* add file to vector, if we found it */
		error = git_vector_insert(stack, file);

		/* add file to cache, if it is new */
		/* do this after above step b/c it is not critical */
		if (error == GIT_SUCCESS && add_to_cache && file->path != NULL)
			error = git_hashtable_insert(cache->files, file->path, file);
	}

cleanup:
	git_buf_free(&path);
	return error;
}

#define push_attrs(R,S,B,F) \
	git_attr_cache__push_file((R),(S),(B),(F),git_attr_file__from_file)

typedef struct {
	git_repository *repo;
	git_vector *files;
} attr_walk_up_info;

static int push_one_attr(void *ref, git_buf *path)
{
	attr_walk_up_info *info = (attr_walk_up_info *)ref;
	return push_attrs(info->repo, info->files, path->ptr, GIT_ATTR_FILE);
}

static int collect_attr_files(
	git_repository *repo, const char *path, git_vector *files)
{
	int error = GIT_SUCCESS;
	git_buf dir = GIT_BUF_INIT;
	git_config *cfg;
	const char *workdir = git_repository_workdir(repo);
	attr_walk_up_info info;

	if ((error = git_attr_cache__init(repo)) < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_vector_init(files, 4, NULL)) < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_path_find_dir(&dir, path, workdir)) < GIT_SUCCESS)
		goto cleanup;

	/* in precendence order highest to lowest:
	 * - $GIT_DIR/info/attributes
	 * - path components with .gitattributes
	 * - config core.attributesfile
	 * - $GIT_PREFIX/etc/gitattributes
	 */

	error = push_attrs(repo, files, repo->path_repository, GIT_ATTR_FILE_INREPO);
	if (error < GIT_SUCCESS)
		goto cleanup;

	info.repo = repo;
	info.files = files;
	error = git_path_walk_up(&dir, workdir, push_one_attr, &info);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_repository_config(&cfg, repo)) == GIT_SUCCESS) {
		const char *core_attribs = NULL;
		git_config_get_string(cfg, GIT_ATTR_CONFIG, &core_attribs);
		git_clearerror(); /* don't care if attributesfile is not set */
		if (core_attribs)
			error = push_attrs(repo, files, NULL, core_attribs);
		git_config_free(cfg);
	}

	if (error == GIT_SUCCESS) {
		error = git_futils_find_system_file(&dir, GIT_ATTR_FILE_SYSTEM);
		if (error == GIT_SUCCESS)
			error = push_attrs(repo, files, NULL, dir.ptr);
		else if (error == GIT_ENOTFOUND)
			error = GIT_SUCCESS;
	}

 cleanup:
	if (error < GIT_SUCCESS) {
		git__rethrow(error, "Could not get attributes for '%s'", path);
		git_vector_free(files);
	}
	git_buf_free(&dir);

	return error;
}


int git_attr_cache__init(git_repository *repo)
{
	int error = GIT_SUCCESS;
	git_attr_cache *cache = &repo->attrcache;

	if (cache->initialized)
		return GIT_SUCCESS;

	if (cache->files == NULL) {
		cache->files = git_hashtable_alloc(
			8, git_hash__strhash_cb, git_hash__strcmp_cb);
		if (!cache->files)
			return git__throw(GIT_ENOMEM, "Could not initialize attribute cache");
	}

	if (cache->macros == NULL) {
		cache->macros = git_hashtable_alloc(
			8, git_hash__strhash_cb, git_hash__strcmp_cb);
		if (!cache->macros)
			return git__throw(GIT_ENOMEM, "Could not initialize attribute cache");
	}

	cache->initialized = 1;

	/* insert default macros */
	error = git_attr_add_macro(repo, "binary", "-diff -crlf");

	return error;
}

void git_attr_cache_flush(
	git_repository *repo)
{
	if (!repo)
		return;

	if (repo->attrcache.files) {
		const void *GIT_UNUSED(name);
		git_attr_file *file;

		GIT_HASHTABLE_FOREACH(repo->attrcache.files, name, file,
			git_attr_file__free(file));

		git_hashtable_free(repo->attrcache.files);
		repo->attrcache.files = NULL;
	}

	if (repo->attrcache.macros) {
		const void *GIT_UNUSED(name);
		git_attr_rule *rule;

		GIT_HASHTABLE_FOREACH(repo->attrcache.macros, name, rule,
			git_attr_rule__free(rule));

		git_hashtable_free(repo->attrcache.macros);
		repo->attrcache.macros = NULL;
	}

	repo->attrcache.initialized = 0;
}

int git_attr_cache__insert_macro(git_repository *repo, git_attr_rule *macro)
{
	if (macro->assigns.length == 0)
		return git__throw(GIT_EMISSINGOBJDATA, "git attribute macro with no values");

	return git_hashtable_insert(
		repo->attrcache.macros, macro->match.pattern, macro);
}
